#ifndef GPS_H
#define GPS_H

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "defines.h"

// Function declarations
void gps_init(void);
void gps_read(void);
const char* gps_get_latitude(void);
const char* gps_get_longitude(void);

#endif // GPS_H
