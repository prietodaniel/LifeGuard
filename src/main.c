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

