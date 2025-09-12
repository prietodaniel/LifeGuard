#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "defines.h" // SOS_BUTTON, LED_PIN, EMERGENCY_PHONE, SOS_DEBOUNCE_DELAY

/* -------- UART / DMA config -------- */
#define GPS_UART            uart1
#define GPS_BAUDRATE        9600
#define GPS_TX_PIN          4    // atentos al hardware de la rp
#define GPS_RX_PIN          5    // atentos al hardware de la rp

#define DMA_BUF_SIZE        1024   // circular DMA buffer
#define WORK_BUF_SIZE       2048
#define PARSER_TASK_STACK   2048

#define FRAME_TERM1         '\r'
#define FRAME_TERM2         '\n'
#define FRAME_TIMEOUT_MS    120     // timeout para completar una oración NMEA

/* -------- Shared DMA buffer -------- */
static uint8_t dma_buf[DMA_BUF_SIZE];
static volatile unsigned dma_chan = (unsigned)-1;
static volatile size_t dma_last_pos = 0; // índice consumido por parser

/* -------- Work buffer for assembling frames -------- */
static uint8_t work_buf[WORK_BUF_SIZE];
static size_t work_len = 0;

/* -------- Parser task handle & GPS data mutex -------- */
static TaskHandle_t parserTaskHandle = NULL;
static TaskHandle_t sosTaskHandle = NULL;
static SemaphoreHandle_t gps_mutex = NULL;

/* Thread-safe storage for last lat/lon as strings */
#define LAT_STR_LEN 32
#define LON_STR_LEN 32
static char last_lat[LAT_STR_LEN];
static char last_lon[LON_STR_LEN];

/* ---------- Utility: CRC NMEA (XOR) ---------- */
static bool nmea_checksum_ok(const char *s, size_t len) {
    // expects s to include leading '$' and ending with '*' then two hex and \r\n removed
    // We'll compute XOR between char after '$' up to char before '*' 
    const char *p = s;
    if (len < 5) return false;
    if (p[0] != '$') return false;
    const char *aster = strchr(p, '*');
    if (!aster) return false;
    uint8_t chk = 0;
    for (const char *c = p + 1; c < aster; ++c) chk ^= (uint8_t)(*c);
    // parse received hex
    int rx = strtol(aster + 1, NULL, 16);
    return chk == (uint8_t)rx;
}

/* ---------- Convert NMEA lat/lon to decimal string ---------- */
/* NMEA lat example: "4807.038,N" -> 48 + 07.038/60 = 48.1173 */
static bool nmea_latlon_to_decstr(const char *field, const char *dir, char *out, size_t outlen) {
    if (!field || !dir || !out) return false;
    if (strlen(field) < 4) return false;
    // find '.'
    char tmp[32];
    strncpy(tmp, field, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    char *dot = strchr(tmp, '.');
    if (!dot) return false;
    // degrees: for latitude first 2 digits, lon first 3 digits; detect by length of degrees part
    int deg_len = (strlen(tmp) > 7) ? 3 : 2; // crude: if big length, use 3
    // better: if field length before '.' >= 5 -> lon (3 deg) else lat (2 deg)
    int before_dot = dot - tmp;
    if (before_dot > 4) deg_len = 3; else deg_len = 2;
    char degs[8] = {0};
    char mins[16] = {0};
    strncpy(degs, tmp, deg_len);
    strcpy(mins, tmp + deg_len);
    double deg = atof(degs);
    double minutes = atof(mins);
    double dec = deg + minutes / 60.0;
    if (dir[0] == 'S' || dir[0] == 'W') dec = -dec;
    // format with 6 decimal places
    snprintf(out, outlen, "%.6f", dec);
    return true;
}

/* ---------- Thread-safe getters (para SOS task) ---------- */
void gps_set_last_coords(const char *lat, const char *lon) {
    if (gps_mutex) xSemaphoreTake(gps_mutex, portMAX_DELAY);
    strncpy(last_lat, lat, LAT_STR_LEN-1); last_lat[LAT_STR_LEN-1] = '\0';
    strncpy(last_lon, lon, LON_STR_LEN-1); last_lon[LON_STR_LEN-1] = '\0';
    if (gps_mutex) xSemaphoreGive(gps_mutex);
}

const char *gps_get_latitude(void) {
    // Return pointer to internal buffer (thread-safe assumption: caller reads quickly).
    // For extra safety, you could copy to caller buffer instead.
    static char tmp[LAT_STR_LEN];
    if (gps_mutex) xSemaphoreTake(gps_mutex, portMAX_DELAY);
    strncpy(tmp, last_lat, LAT_STR_LEN-1);
    tmp[LAT_STR_LEN-1] = '\0';
    if (gps_mutex) xSemaphoreGive(gps_mutex);
    return tmp;
}

const char *gps_get_longitude(void) {
    static char tmp[LON_STR_LEN];
    if (gps_mutex) xSemaphoreTake(gps_mutex, portMAX_DELAY);
    strncpy(tmp, last_lon, LON_STR_LEN-1);
    tmp[LON_STR_LEN-1] = '\0';
    if (gps_mutex) xSemaphoreGive(gps_mutex);
    return tmp;
}

/* ---------- process_payload hook ---------- */
void gps_feed_frame(uint8_t *payload, size_t payload_len) {
    // payload is raw bytes of NMEA sentence WITHOUT trailing CRLF (or possibly with).
    // We expect null-terminated string for convenience:
    char s[256];
    size_t l = payload_len < sizeof(s)-1 ? payload_len : sizeof(s)-1;
    memcpy(s, payload, l);
    s[l] = '\0';

    // validate checksum
    if (!nmea_checksum_ok(s, l)) {
        return;
    }

    // Parse minimal GPRMC or GNGGA for lat/lon
    if (strncmp(s+1, "GPRMC", 5) == 0 || strncmp(s+1, "GNRMC", 5) == 0) {
        // Comma-separated: $GPRMC,time,status,lat,NS,lon,EW,...
        char *copy = strdup(s);
        char *tok = strtok(copy, ",");
        // fields
        int idx = 0;
        char *time_field = NULL;
        char *status = NULL;
        char *lat = NULL; char *ns = NULL;
        char *lon = NULL; char *ew = NULL;
        while (tok) {
            if (idx == 1) time_field = tok;
            else if (idx == 2) status = tok;
            else if (idx == 3) lat = tok;
            else if (idx == 4) ns = tok;
            else if (idx == 5) lon = tok;
            else if (idx == 6) ew = tok;
            tok = strtok(NULL, ",");
            idx++;
        }
        if (status && status[0] == 'A' && lat && ns && lon && ew) {
            char latstr[32], lonstr[32];
            if (nmea_latlon_to_decstr(lat, ns, latstr, sizeof(latstr)) &&
                nmea_latlon_to_decstr(lon, ew, lonstr, sizeof(lonstr))) {
                gps_set_last_coords(latstr, lonstr);
            }
        }
        free(copy);
    } else if (strncmp(s+1, "GPGGA", 5) == 0 || strncmp(s+1, "GNGGA",5) == 0) {
        // $GPGGA,time,lat,NS,lon,EW,fix,...
        char *copy = strdup(s);
        char *tok = strtok(copy, ",");
        int idx = 0;
        char *lat = NULL; char *ns = NULL;
        char *lon = NULL; char *ew = NULL;
        while (tok) {
            if (idx == 2) lat = tok;
            else if (idx == 3) ns = tok;
            else if (idx == 4) lon = tok;
            else if (idx == 5) ew = tok;
            tok = strtok(NULL, ",");
            idx++;
        }
        if (lat && ns && lon && ew) {
            char latstr[32], lonstr[32];
            if (nmea_latlon_to_decstr(lat, ns, latstr, sizeof(latstr)) &&
                nmea_latlon_to_decstr(lon, ew, lonstr, sizeof(lonstr))) {
                gps_set_last_coords(latstr, lonstr);
            }
        }
        free(copy);
    }
}

/* ---------- DMA IRQ handler (minimal) ---------- */
void dma_handler() {
    // Clear the interrupt for our channel
    dma_hw->ints0 = 1u << dma_chan;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Notify parser task (gives 1 count)
    vTaskNotifyGiveFromISR(parserTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ---------- UART + DMA init ---------- */
static void uart_dma_init(void) {
    // UART init
    uart_init(GPS_UART, GPS_BAUDRATE);
    gpio_set_function(GPS_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(GPS_RX_PIN, GPIO_FUNC_UART);

    // Enable RX FIFO interrupts? We'll rely on DMA.
    // DMA channel config
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);   // read from peripheral (no increment)
    channel_config_set_write_increment(&c, true);   // write to buffer (increment)
    channel_config_set_dreq(&c, uart_get_dreq(GPS_UART, true)); // DREQ for UART RX

    dma_channel_configure(
        dma_chan,
        &c,
        dma_buf,                          // dest
        &uart_get_hw(GPS_UART)->dr,       // src
        DMA_BUF_SIZE,                     // transfer count
        true                              // start immediately
    );

    // Enable channel IRQ
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

/* ---------- Parser task ---------- */
static void parser_task(void *pvParameters) {
    TickType_t last_data_tick = 0;
    const TickType_t timeout_ticks = pdMS_TO_TICKS(FRAME_TIMEOUT_MS);

    // initialize indices
    dma_last_pos = 0;
    work_len = 0;

    for (;;) {
        // Wait for notification from ISR or timeout check periodically
        // ulTaskNotifyTake blocks until notified or timeout -> use short timeout to check timeouts
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        // Determine current write position in DMA buffer:
        // remaining transfers:
        uint32_t remain = dma_channel_get_trans_count(dma_chan);
        size_t cur_pos = (DMA_BUF_SIZE - remain) % DMA_BUF_SIZE; // current write index

        // Compute bytes available (new data) between dma_last_pos and cur_pos
        size_t new_len = 0;
        if (cur_pos >= dma_last_pos) new_len = cur_pos - dma_last_pos;
        else new_len = (DMA_BUF_SIZE - dma_last_pos) + cur_pos;

        if (new_len > 0) {
            // Copy new bytes to work_buf (handle overflow)
            if (work_len + new_len > WORK_BUF_SIZE) {
                // overflow: reset assembly (or handle partial)
                work_len = 0;
            }
            if (cur_pos >= dma_last_pos) {
                memcpy(work_buf + work_len, &dma_buf[dma_last_pos], new_len);
            } else {
                size_t first = DMA_BUF_SIZE - dma_last_pos;
                memcpy(work_buf + work_len, &dma_buf[dma_last_pos], first);
                memcpy(work_buf + work_len + first, &dma_buf[0], cur_pos);
            }
            work_len += new_len;
            dma_last_pos = cur_pos % DMA_BUF_SIZE;
            last_data_tick = xTaskGetTickCount();
        }

        // Try to extract complete NMEA sentences terminated by \r\n
        size_t i = 0;
        while (i + 1 < work_len) {
            // look for \r\n sequence
            if (work_buf[i] == FRAME_TERM1 && work_buf[i+1] == FRAME_TERM2) {
                // sentence spans from 0..i-1 (assuming starts at 0)
                size_t sentence_len = i; // exclude CRLF
                if (sentence_len > 0) {
                    // copy sentence to temp and process
                    uint8_t tmp[1024];
                    size_t copy_len = sentence_len < sizeof(tmp)-1 ? sentence_len : sizeof(tmp)-1;
                    memcpy(tmp, work_buf, copy_len);
                    tmp[copy_len] = '\0';
                    // Validate checksum and parse
                    gps_feed_frame(tmp, copy_len);
                }
                // remove processed sentence + CRLF from work_buf
                size_t remaining = work_len - (i + 2);
                if (remaining > 0) memmove(work_buf, work_buf + i + 2, remaining);
                work_len = remaining;
                i = 0; // restart search
                continue;
            }
            i++;
        }

        // If no new data for timeout -> treat as timeout (force attempt)
        if (work_len > 0 && last_data_tick != 0) {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_data_tick) >= timeout_ticks) {
                // consider current content as a frame attempt (if contains '*XX' checksum maybe)
                // If it has '*' then try to findsterisk pos
                char *aster = memchr(work_buf, '*', work_len);
                if (aster) {
                    // if checksum and enough tail length (2 hex chars)
                    size_t pos = aster - (char*)work_buf;
                    if (work_len >= pos + 3) {
                        // process up to pos+3 maybe (e.g. "*5C")
                        size_t sentence_len = pos + 3; // note: not exact; this is heuristic
                        uint8_t tmp[1024];
                        size_t copy_len = sentence_len < sizeof(tmp)-1 ? sentence_len : sizeof(tmp)-1;
                        memcpy(tmp, work_buf, copy_len);
                        tmp[copy_len] = '\0';
                        if (nmea_checksum_ok((char*)tmp, copy_len)) {
                            gps_feed_frame(tmp, copy_len);
                        }
                    }
                }
                // clear buffer at timeout
                work_len = 0;
                last_data_tick = 0;
            }
        }
    }
}

/* ---------- SOS task (adapted from tu código original) ---------- */
void vSOSTask(void *pvParameters) {
    while (true) {
        if (gpio_get(SOS_BUTTON)) {
            gpio_put(LED_PIN, 1);
            char message[160];
            // lee lat/lon thread-safe
            const char *lat = gps_get_latitude();
            const char *lon = gps_get_longitude();
            snprintf(message, sizeof(message), "EMERGENCIA! Ubicación: Lat=%s, Lon=%s", lat, lon);
            gsm_send_sms(EMERGENCY_PHONE, message);
            vTaskDelay(pdMS_TO_TICKS(SOS_DEBOUNCE_DELAY));
        } else {
            gpio_put(LED_PIN, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ---------- Inicialización y main ---------- */
int main(void) {
    stdio_init_all();

    // init gpio
    gpio_init(SOS_BUTTON);
    gpio_set_dir(SOS_BUTTON, GPIO_IN);
    gpio_pull_down(SOS_BUTTON);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // create mutex for gps data
    gps_mutex = xSemaphoreCreateMutex();

    // init GSM (tu implementación): para poder enviar SMS en SOS task
    gsm_init();

    // init UART + DMA for GPS
    uart_dma_init();

    // create parser task
    xTaskCreate(
        parser_task,
        "GPS Parser",
        PARSER_TASK_STACK,
        NULL,
        3, // prioridad
        &parserTaskHandle
    );

    // create SOS task (igual a tu código)
    xTaskCreate(
        vSOSTask,
        "SOS Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        1,
        &sosTaskHandle
    );

    // start scheduler
    vTaskStartScheduler();

    // should never reach here
    while (1) tight_loop_contents();
    return 0;
}
