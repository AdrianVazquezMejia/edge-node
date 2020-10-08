/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
char *TAG = "INFO2";
void app_main() {

  while (1) {
    ESP_LOGI(TAG, "Hello World");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
