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
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "global_variables.h"
#include "math.h"
#include "nvs.h"
#include "nvs_flash.h"

#define MAX_PARTITIONS 3
#define MAX_ENTRIES    126
#define MAX_PAGES      15
#define MAX_WRITES     80000
char *TAG_NVS   = "NVS";
char *TAG_NVS_2 = "NVS_2";
//#define FLASH_LOG

#define ESP_INTR_FLAG_DEFAULT 0
SemaphoreHandle_t smph_pulse_handler = NULL;

void IRAM_ATTR pulse_isr(void *arg) {
    xSemaphoreGiveFromISR(smph_pulse_handler, NULL);
}
void pulse_isr_init(gpio_num_t gpio_num) {
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_pad_select_gpio(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);
    gpio_isr_handler_add(gpio_num, pulse_isr, NULL);
    gpio_set_intr_type(gpio_num, GPIO_INTR_POSEDGE);
}
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
static esp_err_t search_init_partition(uint8_t *pnumber) {
    char *pname;
    nvs_handle_t my_handle;
    uint8_t isFull;
    esp_err_t err;

    for (uint8_t i = 1; i <= MAX_PARTITIONS; i++) {
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

        nvs_close(my_handle);
        nvs_flash_deinit_partition(pname);

        if (isFull) {
            ESP_LOGW(TAG_NVS, "Partition number %d is full, trying next", i);
            if (i == MAX_PARTITIONS) {
                ESP_LOGE(TAG_NVS, "All partitions are full");
                free(pname);
                return ESP_FAIL;
            }
            continue;
        }
#ifdef FLASH_LOG
        ESP_LOGI(TAG_NVS, "NVS init success in  %s", pname);
#endif
        *pnumber = i;
        free(pname);
        break;
    }
    return ESP_OK;
}
static esp_err_t get_pulse_counter_info(char *partition_name, char *page_name,
                                        uint8_t *entry_index,
                                        uint32_t *counter_value) {
    nvs_handle_t my_handle;
    nvs_iterator_t entry;
    nvs_entry_info_t info;
    esp_err_t err;
    char *entry_key;
    entry = nvs_entry_find(partition_name, page_name, NVS_TYPE_ANY);
    while (entry != NULL) {
        (*entry_index)++;
        nvs_entry_info(entry, &info);
        entry = nvs_entry_next(entry);
    };
    nvs_release_iterator(entry);
    if (*entry_index > 0)
        (*entry_index)--;

    if (get_name(&entry_key, "entry", *entry_index)) {
        free(entry_key);
        return ESP_FAIL;
    }

    err = nvs_open_from_partition(partition_name, page_name, NVS_READWRITE,
                                  &my_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_u32(my_handle, entry_key, counter_value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {

        err            = nvs_set_u32(my_handle, entry_key, 0);
        *counter_value = 0;
        if (err != ESP_OK)
            ESP_LOGE(TAG_NVS, "Set %s error %s", entry_key,
                     esp_err_to_name(err));
        err = nvs_commit(my_handle);
        if (err != ESP_OK)
            ESP_LOGE(TAG_NVS, "Commit %s error %s", entry_key,
                     esp_err_to_name(err));
    } else if (err != ESP_OK)
        return err;
    nvs_close(my_handle);
    free(entry_key);
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

esp_err_t get_initial_pulse(uint32_t *pulse_counter, nvs_address_t *address) {
    esp_err_t err;
    char *partition_name;
    char *page_name;
    uint8_t page_index;
    uint8_t entry_index = 0;

    err = search_init_partition(&address->partition);
    if (get_name(&partition_name, "app", address->partition)) {
        return ESP_FAIL;
    }
    err = nvs_flash_init_partition(partition_name);
    if (err != ESP_OK)
        return err;

    err = read_pvcounter_from_storage(partition_name, &page_index);
    if (err != ESP_OK)
        return err;

    if (get_name(&page_name, "page", page_index)) {
        return ESP_FAIL;
    }

    err = get_pulse_counter_info(partition_name, page_name, &entry_index,
                                 pulse_counter);
    free(page_name);
    if (err != ESP_OK)
        return err;

    err = nvs_flash_deinit_partition(partition_name);
    if (err != ESP_OK)
        return err;
    address->page        = page_index;
    address->entry_index = entry_index;
#ifdef FLASH_LOG
    ESP_LOGI(TAG_NVS, "ADDRESS:");
    ESP_LOGI(TAG_NVS, "\n-Partition: %d \n - Page: %d\n - Entry %d",
             address->partition, address->page, address->entry_index);
#endif

    return ESP_OK;
}
esp_err_t save2address(uint32_t data, nvs_address_t address) {
    nvs_handle_t my_handle;
    esp_err_t err;
    char *partition_name;
    char *page_name;
    char *entry_key;

    if (get_name(&partition_name, "app", address.partition)) {
        return ESP_FAIL;
    }
    if (get_name(&page_name, "page", address.page)) {
        return ESP_FAIL;
    }
    if (get_name(&entry_key, "entry", address.entry_index)) {
        return ESP_FAIL;
    }

    nvs_flash_init_partition(partition_name);
    err = nvs_open_from_partition(partition_name, page_name, NVS_READWRITE,
                                  &my_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u32(my_handle, entry_key, data);
    if (err != ESP_OK)
        ESP_LOGE(TAG_NVS_2, "Set %s error", entry_key);
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        ESP_LOGE(TAG_NVS_2, "Commit %s error", entry_key);
    if (err != ESP_OK)
        return err;

    nvs_close(my_handle);
    nvs_flash_deinit_partition(partition_name);
    ESP_LOGI(TAG_NVS, "Data successfully saved %d", data);
    free(entry_key);
    free(partition_name);
    free(page_name);
    return ESP_OK;
}
esp_err_t put_nvs(uint32_t data, nvs_address_t *address) {

    bool writeNext = false;
    //    uint32_t total_entries;
    //    total_entries = (address->entry_index + 1) +
    //                    (address->page - 1) * MAX_ENTRIES +
    //                    (address->partition - 1) * MAX_PAGES * MAX_ENTRIES;
    uint32_t writes_offset =
        round(((float)INITIAL_ENERGY / 100 * (float)IMPULSE_CONVERSION));
    uint32_t writes_count = data - writes_offset;
    writeNext             = writes_count % MAX_WRITES;

    if (!writeNext && (writes_count > 0)) {
        ESP_LOGI(TAG_NVS, "Next entry");
        if (address->entry_index >= MAX_ENTRIES - 1) {
            ESP_LOGI(TAG_NVS, "Next page");
            if (address->page >= MAX_PAGES) {
                ESP_LOGI(TAG_NVS, "Next partition");
                if (address->partition >= MAX_PARTITIONS) {
                    ESP_LOGE(TAG_NVS, "FLASH FULL"); // TODO handle this
                    return ESP_FAIL;
                } else {
                    address->partition++;
                    address->page        = 1;
                    address->entry_index = 0;
                }
            } else {
                address->page++;
                address->entry_index = 0;
            }
        } else
            address->entry_index++;
    }
    save2address(data, *address);
    return ESP_OK;
}
