/*
 * flash.h
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
/**
 *@file
 *@brief Functions to save in non volatile memory (flash) in a cyclic way
 * */

#ifndef MAIN_INCLUDE_FLASH_H_
#define MAIN_INCLUDE_FLASH_H_
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>
/**
 * @brief Struct to point a memory location in a partition, page and entry
 * */
typedef struct {
    uint8_t partition;   /**< Number of flash partition*/
    uint8_t page;        /**< Number of page in the current partition*/
    uint8_t entry_index; /**< Number of entry in the current page*/
} nvs_address_t;

/**
 * @brief Set up the interrupt service routine to detect the rising edge of the
 * pulse that come from the meter.
 *
 * @param gpio_num GPIO number which the incoming pulse in connected to.
 * */
void pulse_isr_init(gpio_num_t gpio_num);

/**
 * @brief Function to save a value in the nvs in a solely location
 *
 * @param[in] value Value to be stored
 *
 * @note This function should be used only for testing or debuging purposes,
 * because high frequency use of this fucntion could damage the the flash
 * location where the value is written.
 * */
void flash_save(uint32_t value);
/**
 * @brief Function to retrieve the value from flash previously saved with {@link
 * flash_save}
 *
 * @param[out] value Pointer where the value saved using the function {@link
 * flash_save} will be stored after the call.
 *
 * @see flash_save
 * */
void flash_get(uint32_t *value);

/**
 * @brief Function to retrieve the last counter value and the address saved
 * before reboot.
 *
 * @param[out] test_pulses Pointer to the where the counter pulse will be saved
 * to.
 * @param[out] address Pointer to the address where the address of the last
 * counter will saved to.
 *
 * @see nvs_address
 * */
esp_err_t get_initial_pulse(uint32_t *test_pulses, nvs_address_t *address);
/**
 * @brief Function to save the the counter pulse to a specific address in the
 * nvs.
 *
 * @param[in] data Counter value to be saved
 * @param[in] address Address where the {@link data} will be saved to.
 *
 * @see nvs_address_t
 * */
esp_err_t put_nvs(uint32_t data, nvs_address_t *address);
#endif /* MAIN_INCLUDE_FLASH_H_ */
