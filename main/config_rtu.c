/*
 * config_rtu.c
 *
 *  Created on: Dec 13, 2020
 *      Author: adrian-estelio
 */
#include "config_rtu.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "stdint.h"
static char *TAG = "CONFIG";
void check_rtu_config(void) {
    nvs_handle_t my_handle;
    extern uint8_t NODE_ID;

    extern int IMPULSE_CONVERSION;

    nvs_flash_init();
    nvs_open("storage", NVS_READWRITE, &my_handle);
#ifdef CONFIG_CONFIG_MODE
    NODE_ID = CONFIG_ID;
    nvs_set_u8(my_handle, "NODE_ID", NODE_ID);
#ifdef CONFIG_MASTER_MODBUS
    extern uint8_t SLAVES;
    SLAVES = CONFIG_SLAVES;
    nvs_set_u8(my_handle, "SLAVES", SLAVES);
#endif
#ifdef CONFIG_PULSE_PERIPHERAL
    IMPULSE_CONVERSION = CONFIG_IMPULSE_CONVERSION;
    nvs_set_i32(my_handle, "IMPULSE_CONVERSION", IMPULSE_CONVERSION);
#endif

#endif

#ifdef CONFIG_PRODUCTION_PATCH
    nvs_get_u8(my_handle, "NODE_ID", &NODE_ID);
#ifdef CONFIG_MASTER_MODBUS
    nvs_get_u8(my_handle, "SLAVES", &SLAVES);
    ESP_LOGI(TAG, "SLAVES >>> % d", SLAVES);
#endif
#ifdef CONFIG_PULSE_PERIPHERAL
    IMPULSE_CONVERSION = CONFIG_IMPULSE_CONVERSION;
    nvs_get_i32(my_handle, "IMPULSE_CONVERSION", &IMPULSE_CONVERSION);
    ESP_LOGI(TAG, "IMPULSE CONVERSION >>> % d", SLAVES);
#endif

#endif

    nvs_commit(my_handle);
    nvs_close(my_handle);
    nvs_flash_deinit();

    ESP_LOGI(TAG, "NODE ID >>> % d", NODE_ID);
}
