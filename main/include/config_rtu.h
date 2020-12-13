/*
 * config_rtu.h
 *
 *  Created on: Dec 13, 2020
 *      Author: adrian-estelio
 */

#ifndef MAIN_INCLUDE_CONFIG_RTU_H_
#define MAIN_INCLUDE_CONFIG_RTU_H_
#include "stdint.h"
uint8_t NODE_ID;
uint8_t SLAVES;
int IMPULSE_CONVERSION;
void check_rtu_config(void);

#endif /* MAIN_INCLUDE_CONFIG_RTU_H_ */
