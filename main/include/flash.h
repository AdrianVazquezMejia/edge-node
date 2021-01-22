/*
 * flash.h
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>

#ifndef MAIN_INCLUDE_FLASH_H_
#define MAIN_INCLUDE_FLASH_H_
typedef struct {
    uint8_t partition;
    uint8_t page;
    uint8_t entry_index;
} nvs_address_t;
void pulse_isr_init(gpio_num_t gpio_num);
void flash_save(uint32_t value);
void flash_get(uint32_t *value);
esp_err_t search_init_partition(uint8_t *pnumber);
esp_err_t get_initial_pulse(uint32_t *test_pulses, nvs_address_t *address);
esp_err_t put_nvs(uint32_t data, nvs_address_t *address);
#endif /* MAIN_INCLUDE_FLASH_H_ */
