/*
 * flash.c
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
#include "flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

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
