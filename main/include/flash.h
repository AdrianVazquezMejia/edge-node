/*
 * flash.h
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
#include "esp_err.h"
#include <stdint.h>

#ifndef MAIN_INCLUDE_FLASH_H_
#define MAIN_INCLUDE_FLASH_H_

void flash_save(uint32_t value);
void flash_get(uint32_t *value);
esp_err_t search_init_partition(uint8_t *pnumber);
esp_err_t get_initial_pulse(uint32_t *test_pulses, uint8_t partition_number);
#endif /* MAIN_INCLUDE_FLASH_H_ */
