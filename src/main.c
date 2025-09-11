#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "defines.h"
#include "gps.h"
#include "gsm.h"

// Task handles
TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t sosTaskHandle = NULL;

// Task functions
void vGPSTask(void *pvParameters) {
    while (true) {
        gps_read();
        vTaskDelay(pdMS_TO_TICKS(100)); // Ajusta este delay según necesites
    }
}

void vSOSTask(void *pvParameters) {
    while (true) {
        if (gpio_get(SOS_BUTTON)) {
            gpio_put(LED_PIN, 1);
            char message[160];
            sprintf(message, "EMERGENCIA! Ubicación: Lat=%s, Lon=%s", 
                    gps_get_latitude(), gps_get_longitude());
            gsm_send_sms(EMERGENCY_PHONE, message);
            vTaskDelay(pdMS_TO_TICKS(SOS_DEBOUNCE_DELAY));
        } else {
            gpio_put(LED_PIN, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Pequeño delay para no saturar el CPU
    }
}

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

    // Create tasks
    xTaskCreate(
        vGPSTask,
        "GPS Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        2, // Mayor prioridad que SOS task
        &gpsTaskHandle
    );

    xTaskCreate(
        vSOSTask,
        "SOS Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        1,
        &sosTaskHandle
    );

    // Start the scheduler
    vTaskStartScheduler();

    // Should never get here
    while (true) {
        // Empty
    }
}