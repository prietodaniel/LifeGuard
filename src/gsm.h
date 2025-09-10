#ifndef GSM_H
#define GSM_H

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "defines.h"

// Function declarations
void gsm_init(void);
void gsm_send_cmd(const char *cmd);
void gsm_send_sms(const char *number, const char *message);

#endif // GSM_H
