/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define driver "driver/gpio.h"
#include "esp_log.h"
#include <stdio.h>

#define PULSE_PERIPHERAL
char *TAG = "INFO";

void task_pulse(void *arg) {
    ESP_LOGI(TAG, "Pulse counter taks Started");

    while (1) {

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    ESP_LOGI(TAG, "MCU initialized");

#ifdef PULSE_PERIPHERAL
    ESP_LOGI(TAG, "Start peripheral");
    xTaskCreatePinnedToCore(task_pulse, "task_pulse", 1024 * 2, NULL, 10, NULL,
                            0);
#endif
}
