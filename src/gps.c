#include "gps.h"
#include <stdio.h>
#include <string.h>

static char latitude[COORD_BUFFER_SIZE] = "";
static char longitude[COORD_BUFFER_SIZE] = "";

void gps_init(void) {
    uart_init(UART_GPS, GPS_BAUD_RATE);
    gpio_set_function(GPS_TX, GPIO_FUNC_UART);
    gpio_set_function(GPS_RX, GPIO_FUNC_UART);
}

void gps_read(void) {
    char line[GPS_LINE_BUFFER_SIZE];
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

const char* gps_get_latitude(void) {
    return latitude;
}

const char* gps_get_longitude(void) {
    return longitude;
}