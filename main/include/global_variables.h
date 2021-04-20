/*
 * global_variables.h
 *
 *  Created on: Apr 7, 2021
 *      Author: adrian-estelio
 */

#ifndef MAIN_INCLUDE_GLOBAL_VARIABLES_H_
#define MAIN_INCLUDE_GLOBAL_VARIABLES_H_
#include "flash.h"
#include "freertos/semphr.h"
#include <stdint.h>

extern uint8_t NODE_ID;

extern SemaphoreHandle_t smph_pulse_handler;
extern int IMPULSE_CONVERSION;
extern uint32_t INITIAL_ENERGY;
extern nvs_address_t pulse_address;

extern uint8_t SLAVES;

#endif /* MAIN_INCLUDE_GLOBAL_VARIABLES_H_ */
