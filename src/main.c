#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <stdio.h>
#include <string.h>

#define UART_GPS uart0
#define UART_GSM uart1

#define GPS_TX 0
#define GPS_RX 1
#define GSM_TX 4
#define GSM_RX 5

#define SOS_BUTTON 10
#define LED_PIN 25

char latitude[16] = "";
char longitude[16] = "";

void gsm_send_cmd(const char *cmd) {
    uart_puts(UART_GSM, cmd);
    uart_puts(UART_GSM, "\r\n");
    sleep_ms(500);
}

void gsm_send_sms(const char *number, const char *message) {
    char buffer[128];
    gsm_send_cmd("AT+CMGF=1"); // modo texto
    sprintf(buffer, "AT+CMGS=\"%s\"", number);
    gsm_send_cmd(buffer);
    sleep_ms(200);
    uart_puts(UART_GSM, message);
    uart_putc(UART_GSM, 26); // Ctrl+Z
    sleep_ms(5000);
}

void gps_read() {
    char line[100];
    int idx = 0;

    while (uart_is_readable(UART_GPS)) {
        char c = uart_getc(UART_GPS);
        if (c == '\n') {
            line[idx] = '\0';
            if (strncmp(line, "$GPGGA", 6) == 0) {
                sscanf(line, "$GPGGA,%*f,%15[^,],N,%15[^,],E", latitude, longitude);
            }
            idx = 0;
        } else if (idx < sizeof(line) - 1) {
            line[idx++] = c;
        }
    }
}

int main() {
    stdio_init_all();

    uart_init(UART_GPS, 9600);
    gpio_set_function(GPS_TX, GPIO_FUNC_UART);
    gpio_set_function(GPS_RX, GPIO_FUNC_UART);

    uart_init(UART_GSM, 9600);
    gpio_set_function(GSM_TX, GPIO_FUNC_UART);
    gpio_set_function(GSM_RX, GPIO_FUNC_UART);

    gpio_init(SOS_BUTTON);
    gpio_set_dir(SOS_BUTTON, GPIO_IN);
    gpio_pull_down(SOS_BUTTON);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (true) {
        gps_read();

        if (gpio_get(SOS_BUTTON)) {
            gpio_put(LED_PIN, 1);
            char message[160];
            sprintf(message, "EMERGENCIA! Ubicación: Lat=%s, Lon=%s", latitude, longitude);
            gsm_send_sms("+549XXXXXXXXXX", message);
            sleep_ms(10000);
        } else {
            gpio_put(LED_PIN, 0);
        }
    }
}