#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "FreeRTOS.h"
#include "task.h"

// =========================
// Defines de hardware
// =========================
#define UART_ID         uart0
#define BAUD_RATE       9600
#define UART_TX_PIN     0
#define UART_RX_PIN     1

#define SOS_BUTTON      15
#define LED_PIN         25
#define EMERGENCY_PHONE "+549XXXXXXXXXX"
#define SOS_DEBOUNCE_DELAY 3000

// =========================
// Buffers GPS
// =========================
#define GPS_BUFFER_SIZE 256
static char gps_dma_buffer[GPS_BUFFER_SIZE];
static char gps_line[GPS_BUFFER_SIZE];
static volatile uint dma_chan;
static volatile size_t gps_line_len = 0;
static volatile bool gps_sentence_ready = false;

// =========================
// Prototipos
// =========================
void gps_dma_irq_handler();
void gps_init();
bool gps_parse_sentence(const char *sentence, char *lat, char *lon);

// =========================
// Task handles
// =========================
TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t sosTaskHandle = NULL;

// =========================
// Contador global 
// =========================
static uint32_t idleTicks = 0;

void vApplicationIdleHook(void) {
    idleTicks++;
}

void vMonitorTask(void *pvParameters) {
    const TickType_t xDelay = pdMS_TO_TICKS(1000);
    while (1) {
        uint32_t lastIdle = idleTicks;
        idleTicks = 0;

        // Este metodo muestra cada 1 segundo el tiempo que el sistema 
        // estuvo en IDLE.
        printf("[Monitor] IdleTicks=%lu en 1s\n", lastIdle);
        vTaskDelay(xDelay);
    }
}

// =========================
// UART con DMA para GPS
// =========================
void gps_init() {
    // Inicializa UART
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Configura DMA para recepción circular
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, uart_get_dreq(UART_ID, true));

    dma_channel_configure(
        dma_chan,
        &c,
        gps_dma_buffer,            // destino
        &uart_get_hw(UART_ID)->dr, // fuente (registro UART)
        GPS_BUFFER_SIZE,
        true // iniciar
    );

    // Habilita interrupción DMA
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, gps_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

// Interrupción DMA: detecta cuando llegó un buffer completo
void gps_dma_irq_handler() {
    dma_hw->ints0 = 1u << dma_chan; // limpia flag
    // Copia línea actual (simulamos búsqueda de '\n')
    for (size_t i = 0; i < GPS_BUFFER_SIZE; i++) {
        char c = gps_dma_buffer[i];
        if (c == '\n' || gps_line_len >= GPS_BUFFER_SIZE - 1) {
            gps_line[gps_line_len] = '\0';
            gps_sentence_ready = true;
            gps_line_len = 0;
            break;
        } else if (c != '\r' && c != 0) {
            gps_line[gps_line_len++] = c;
        }
    }
    // Rearma DMA para seguir recibiendo
    dma_channel_set_read_addr(dma_chan, &uart_get_hw(UART_ID)->dr, true);
}

// Parser básico de sentencias NMEA
bool gps_parse_sentence(const char *sentence, char *lat, char *lon) {
    // Ejemplo: $GPGGA,123519,4807.038,N,01131.000,E,...
    if (strncmp(sentence, "$GPGGA", 6) != 0) return false;

    char copy[GPS_BUFFER_SIZE];
    strncpy(copy, sentence, GPS_BUFFER_SIZE);
    char *token = strtok(copy, ",");

    int field = 0;
    while (token != NULL) {
        field++;
        if (field == 3) { // latitud
            strncpy(lat, token, 15);
        } else if (field == 4) { // N/S
            strcat(lat, token);
        } else if (field == 5) { // longitud
            strncpy(lon, token, 15);
        } else if (field == 6) { // E/W
            strcat(lon, token);
            return true; // ya tenemos lat/lon
        }
        token = strtok(NULL, ",");
    }
    return false;
}

// =========================
// Task GPS
// =========================
void vGPSTask(void *pvParameters) {
    char lat[20], lon[20];
    while (true) {
        if (gps_sentence_ready) {
            gps_sentence_ready = false;
            if (gps_parse_sentence(gps_line, lat, lon)) {
                printf("GPS OK -> Lat=%s Lon=%s\n", lat, lon);
            } else {
                printf("GPS inválido: %s\n", gps_line);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// =========================
// Task SOS
// =========================
void vSOSTask(void *pvParameters) {
    char lat[20] = "0", lon[20] = "0";
    while (true) {
        if (gpio_get(SOS_BUTTON)) {
            gpio_put(LED_PIN, 1);
            char message[160];
            sprintf(message, "EMERGENCIA! Ubicación: Lat=%s, Lon=%s", lat, lon);
            // gsm_send_sms(EMERGENCY_PHONE, message); // implementar con tu módulo GSM
            printf(">>> SMS: %s\n", message);
            vTaskDelay(pdMS_TO_TICKS(SOS_DEBOUNCE_DELAY));
        } else {
            gpio_put(LED_PIN, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vApplicationIdleHook(void) {
    // Memoria libre en el heap
    size_t freeHeap = xPortGetFreeHeapSize();

    // Memoria mínima registrada (pico de uso)
    size_t minEverFreeHeap = xPortGetMinimumEverFreeHeapSize();

    // CPU Usage (método simple): podés contar "ticks" de idle
    static uint32_t idleCounter = 0;
    idleCounter++;

    // Cada cierto número de llamadas mostramos estadísticas
    if (idleCounter % 100000 == 0) {
        printf("[IdleHook] FreeHeap=%u bytes | MinHeap=%u bytes\n",
               (unsigned int)freeHeap,
               (unsigned int)minEverFreeHeap);
    }
}


// =========================
// main()
// =========================
int main() {
    stdio_init_all();

    // Inicializa GPS y GSM
    gps_init();
    // gsm_init(); // implementar según tu módulo

    // Inicializa GPIO
    gpio_init(SOS_BUTTON);
    gpio_set_dir(SOS_BUTTON, GPIO_IN);
    gpio_pull_down(SOS_BUTTON);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Crea tareas
    xTaskCreate(vGPSTask, "GPS Task", 1024, NULL, 2, &gpsTaskHandle);
    xTaskCreate(vSOSTask, "SOS Task", 1024, NULL, 1, &sosTaskHandle);
    
    //Agregamos la tarea de monitoreo
    xTaskCreate(vMonitorTask, "Monitor", 1024, NULL, 1, NULL);

    // Arranca scheduler
    vTaskStartScheduler();

    while (true) { }
}
