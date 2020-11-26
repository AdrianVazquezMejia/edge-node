/*
 * ota_update.c
 *
 *  Created on: Nov 25, 2020
 *      Author: adrian-estelio
 */
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_types.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "protocol_common.h"
#include <stdio.h>

#define TIMER_DIVIDER       16
#define TIMER_SCALE         (TIMER_BASE_CLK / TIMER_DIVIDER)
#define TIMER_INTERVAL0_SEC (60)
#define WITH_RELOAD         1

char *TAG_OTA                       = "OTA_UPDATE";
static SemaphoreHandle_t xSemaphore = NULL;

void IRAM_ATTR timer_group0_isr(void *para) {
    int timer_idx = (int)para;
    static BaseType_t xHigherPriorityTaskWoken;
    TIMERG0.int_clr_timers.t0                   = 1;
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;
    xHigherPriorityTaskWoken                    = pdFALSE;
    xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
}

static void ota_timer_init(int timer_idx, bool auto_reload,
                           double timer_interval_sec) {
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {.divider     = TIMER_DIVIDER,
                             .counter_dir = TIMER_COUNT_UP,
                             .counter_en  = TIMER_PAUSE,
                             .alarm_en    = TIMER_ALARM_EN,
                             .intr_type   = TIMER_INTR_LEVEL,
                             .auto_reload = auto_reload};
    timer_init(TIMER_GROUP_0, timer_idx, &config);
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx,
                          timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
                       (void *)timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}

void task_ota(void *p) {
    ESP_LOGI(TAG_OTA, "Started OTA Task");
    ota_timer_init(TIMER_0, WITH_RELOAD, TIMER_INTERVAL0_SEC);
    xSemaphore    = xSemaphoreCreateBinary();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#if CONFIG_OTA
    ESP_ERROR_CHECK(connect());
#endif
    while (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG_OTA, "Update Firmware");

        vTaskDelay(100);
    }
}
