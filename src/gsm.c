#include "gsm.h"
#include <stdio.h>

void gsm_init(void) {
    uart_init(UART_GSM, GSM_BAUD_RATE);
    gpio_set_function(GSM_TX, GPIO_FUNC_UART);
    gpio_set_function(GSM_RX, GPIO_FUNC_UART);
}

void gsm_send_cmd(const char *cmd) {
    uart_puts(UART_GSM, cmd);
    uart_puts(UART_GSM, "\r\n");
    sleep_ms(GSM_CMD_DELAY);
}

void gsm_send_sms(const char *number, const char *message) {
    char buffer[GSM_BUFFER_SIZE];
    gsm_send_cmd("AT+CMGF=1"); // Set text mode
    sprintf(buffer, "AT+CMGS=\"%s\"", number);
    gsm_send_cmd(buffer);
    sleep_ms(GSM_SMS_PREP_DELAY);
    uart_puts(UART_GSM, message);
    uart_putc(UART_GSM, 26); // Ctrl+Z to send
    sleep_ms(GSM_SMS_SEND_DELAY);
}
