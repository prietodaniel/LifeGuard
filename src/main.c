#include "pico/stdlib.h"
#include <stdio.h>
#include "defines.h"
#include "gps.h"
#include "gsm.h"

int main() {
    stdio_init_all();

    // Initialize modules
    gps_init();
    gsm_init();

    // Initialize GPIO
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
            sprintf(message, "EMERGENCIA! Ubicación: Lat=%s, Lon=%s", 
                    gps_get_latitude(), gps_get_longitude());
            gsm_send_sms(EMERGENCY_PHONE, message);
            sleep_ms(SOS_DEBOUNCE_DELAY);
        } else {
            gpio_put(LED_PIN, 0);
        }
    }
}