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

#define MAX_PARTITIONS 3
char *TAG_NVS   = "NVS";
char *TAG_NVS_2 = "NVS_2";
//#define FLASH_LOG
inline int get_name(char **output, char *prefix, int n) {
    if (asprintf(output, "%s%d", prefix, n) < 0) {
        ESP_LOGE(TAG_NVS, "Create %s name error", prefix);
        return -1;
    };
    ESP_LOGI(TAG_NVS, "%s", *output);
    return 0;
}
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
#ifdef FLASH_LOG
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        printf("Committing updates in NVS ... ");
#endif
        err = nvs_commit(my_handle);
#ifdef FLASH_LOG
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
/*Reset partition if corrupted*/
void check_partition(char *name) {
    esp_err_t err;
    err = nvs_flash_init_partition(name);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase_partition(name));
        ESP_ERROR_CHECK(nvs_flash_init_partition(name));
    }
    nvs_flash_deinit_partition(name);
}
/*Print partition info*/
void put_partition_info(char *name) {
    nvs_stats_t info;
    esp_err_t err;
    ESP_ERROR_CHECK(nvs_flash_init_partition(name));
    err = nvs_get_stats(name, &info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Error (%s)", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG_NVS,
             "\n Total entries: %d\n - Used entries:%d\n - Free"
             "entries: %d\n - "
             "Namespace count: %d",
             info.total_entries, info.used_entries, info.free_entries,
             info.namespace_count);
    nvs_flash_deinit_partition(name);
}
esp_err_t search_init_partition(uint8_t *pnumber) {
    // TODO on hardware reset with button
    char *pname;
    nvs_handle_t my_handle;
    uint8_t isFull;
    esp_err_t err;

    for (uint8_t i = 1; i <= 3; i++) {
        if (get_name(&pname, "app", i)) {
            return ESP_FAIL;
        }
        check_partition(pname);
        put_partition_info(pname);

        ESP_ERROR_CHECK(nvs_flash_init_partition(pname));
        err = nvs_open_from_partition(pname, "storage", NVS_READWRITE,
                                      &my_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_NVS, "nvs_open_from_partition error, Error (%s)",
                     esp_err_to_name(err));
            return ESP_FAIL;
        }
        // Get status
        err = nvs_get_u8(my_handle, "isFull", &isFull);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            // create if not exists
            isFull = false;
            err    = nvs_set_u8(my_handle, "isFull", isFull);
            if (err != ESP_OK)
                ESP_LOGE(TAG_NVS, "Set error");
            err = nvs_commit(my_handle);
            if (err != ESP_OK)
                ESP_LOGE(TAG_NVS, "Commit error");
        } else if (err != ESP_OK)
            ESP_LOGE(TAG_NVS, "Get error (%s)", esp_err_to_name(err));

        // Get partition number
        err = nvs_get_u8(my_handle, "pnumber", pnumber);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = nvs_set_u8(my_handle, "pnumber", i);
            if (err != ESP_OK)
                ESP_LOGE(TAG_NVS, "Set error");
            err = nvs_commit(my_handle);
            if (err != ESP_OK)
                ESP_LOGE(TAG_NVS, "Commit error");
            ESP_LOGI(TAG_NVS, "Partition number set first time: %d", i);
        } else if (err != ESP_OK)
            ESP_LOGE(TAG_NVS, "Get error (%s)", esp_err_to_name(err));
        ESP_LOGI(TAG_NVS, "Partition number set: %u", *pnumber);

        nvs_close(my_handle);
        nvs_flash_deinit_partition(pname);

        if (isFull) {
            ESP_LOGW(TAG_NVS, "Partition number %d is full, trying next", i);
            if (i == MAX_PARTITIONS) {
                ESP_LOGE(TAG_NVS, "All partitions are full");
                return ESP_FAIL;
            }
            continue;
        }
        ESP_LOGI(TAG_NVS, "NVS init success in  %s", pname);
        *pnumber = i;
        break;
    }
    return ESP_OK;
}

static esp_err_t read_pvcounter_from_storage(char *partition_name,
                                             uint8_t *page_index) {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open_from_partition(partition_name, "storage", NVS_READWRITE,
                                  &my_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_u8(my_handle, "page_index", page_index);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *page_index = 1;
        err         = nvs_set_u8(my_handle, "page_index", *page_index);
        if (err != ESP_OK)
            ESP_LOGE(TAG_NVS_2, "Set %d error", *page_index);
        err = nvs_commit(my_handle);
        if (err != ESP_OK)
            ESP_LOGE(TAG_NVS_2, "Commit %d error", *page_index);
    } else if (err != ESP_OK)
        return err;

    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t get_initial_pulse(uint32_t *pulse_counter, uint8_t partition_number) {
    esp_err_t err;
    char *partition_name, *pvActual;
    uint8_t indexPv;

    if (get_name(&partition_name, "app", partition_number)) {
        return ESP_FAIL;
    }
    err = nvs_flash_init_partition(partition_name);
    err = read_pvcounter_from_storage(partition_name, &indexPv);
    err = nvs_flash_deinit_partition(partition_name);
    return ESP_OK;
}
