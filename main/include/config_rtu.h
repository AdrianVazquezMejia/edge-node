/*
 * config_rtu.h
 *
 *  Created on: Dec 13, 2020
 *      Author: adrian-estelio
 */
/**
 * @file
 *
 *This file contains the function to set up the MCU when restart, reading
 *confinguration data from flash
 */

#ifndef MAIN_INCLUDE_CONFIG_RTU_H_
#define MAIN_INCLUDE_CONFIG_RTU_H_
#include "stdint.h"

/**
 * This function read from flash the condiguration data: ID, SLAVES, INITIAL
 ENERGY, IMPULSE_K
 *
 *@see NODE_ID
 *@see IMPULSE_CONVERSION
 *@see INITIAL ENERGY
 *

 */
void check_rtu_config(void);

#endif /* MAIN_INCLUDE_CONFIG_RTU_H_ */
