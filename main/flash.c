/*
 * flash.c
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
#include "flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

char *TAG_NVS = "NVS";
//#define FLAH_LOG
void flash_save(uint32_t value) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
#ifdef FLASH_LOG
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
#endif
    } else {
        err = nvs_set_u32(my_handle, "value", value);
#ifdef FLAH_LOG
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        printf("Committing updates in NVS ... ");
#endif
        err = nvs_commit(my_handle);
#ifdef FLAH_LOG
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
#endif
        nvs_close(my_handle);
    }
}
void flash_get(uint32_t *value) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
#ifdef FLASH_LOG

        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
#endif
    } else {
#ifdef FLASH_LOG

        printf("Done\n");

        // Read
        printf("Reading restart counter from NVS ... ");
#endif
        *value = 0; // value will default to 0, if not set yet in NVS
        err    = nvs_get_u32(my_handle, "value", value);
#ifdef FLASH_LOG

        switch (err) {
        case ESP_OK:
            printf("Done\n");
            printf("Counter start is = %d\n", *value);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
#endif
        nvs_close(my_handle);
    }
}

esp_err_t search_init_partition(uint8_t *pnumber) {

    char *pname;
    nvs_stats_t info;
    nvs_handle_t my_handle;
    uint8_t isFull;
    esp_err_t err;

    for (uint8_t i = 1; i <= 3; i++) {
        if (asprintf(&pname, "app%d", i) < 0) {
            free(pname);
            ESP_LOGI(TAG_NVS, "Partition name not created");
            return ESP_FAIL;
        }
        ESP_LOGE(TAG_NVS, "%s", pname);
        err = nvs_flash_init_partition(pname);

        //  Reset if corrupted
        if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
            gpio_get_level(GPIO_NUM_0) == 0) {
            ESP_ERROR_CHECK(nvs_flash_erase_partition("app1"));
            ESP_ERROR_CHECK(nvs_flash_erase_partition("app2"));
            ESP_ERROR_CHECK(nvs_flash_erase_partition("app3"));
            ESP_ERROR_CHECK(nvs_flash_init_partition(pname));
        }
        // info
        err = nvs_get_stats(pname, &info);
        if (err == ESP_OK)
            ESP_LOGI(TAG_NVS,
                     "\n Total entries: %d\n - Used entries:%d\n - Free"
                     "entries: %d\n - "
                     "Namespace count: %d",
                     info.total_entries, info.used_entries, info.free_entries,
                     info.namespace_count);
        // Check partition status
        err = nvs_open_from_partition(pname, "storage", NVS_READWRITE,
                                      &my_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_NVS, "ERROR IN nvs_open_from_partition");
            return ESP_FAIL;
        }
        err = nvs_get_u8(my_handle, "isFull", &isFull);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGE(TAG_NVS, "ERROR (%s) IN GET", esp_err_to_name(err));
        else if (err == ESP_ERR_NVS_NOT_FOUND) {
            // create if not exists
            isFull = false;
            err    = nvs_set_u8(my_handle, "finished", isFull);
            err    = nvs_commit(my_handle);
            if (err != ESP_OK)
                ESP_LOGE(TAG_NVS, "ERROR IN COMMIT");
        }
        // Save partition number
        err = nvs_get_u8(my_handle, "pnumber", pnumber);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGE(TAG_NVS, "ERROR IN GET");
        else if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = nvs_set_u8(my_handle, "pnumber", i);
            ESP_LOGW(TAG_NVS, "Partition number set first time: %d", i);
            err = nvs_commit(my_handle);
            if (err != ESP_OK)
                ESP_LOGE(TAG_NVS, "ERROR IN COMMIT");
        }
        ESP_LOGI(TAG_NVS, "Partition number set: %u", *pnumber);
        // Close partition
        nvs_close(my_handle);
        nvs_flash_deinit_partition(pname);

        if (isFull) {
            ESP_LOGW(TAG_NVS, "Partition number %d is full, trying next", i);
            if (i == 3) {
                ESP_LOGE(TAG_NVS, "All partitions are full");
                return ESP_FAIL;
            }
            continue;
        }
        ESP_LOGI(TAG_NVS, "NVS init can be done in partition %s", pname);
        *pnumber = i;
        break;
    }
    return ESP_OK;
}
