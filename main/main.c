/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define PULSE_PERIPHERAL
char *TAG = "INFO";

void task_pulse(void *arg) {
    ESP_LOGI(TAG, "Pulse counter task started");
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    while (1) {
        ESP_LOGI(TAG, "level is: %d", gpio_get_level(GPIO_NUM_0));
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
