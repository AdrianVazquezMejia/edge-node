/*
 * modbus_master.h
 *
 *  Created on: Oct 14, 2020
 *      Author: adrian-estelio
 */

#ifndef MAIN_INCLUDE_MODBUS_MASTER_H_
#define MAIN_INCLUDE_MODBUS_MASTER_H_
#include "stdint.h"

void read_input_register(uint8_t slave, uint16_t start_address,
                         uint16_t quantity);
void save_register(uint8_t data, uint16_t **modbus_registers);
#endif /* MAIN_INCLUDE_MODBUS_MASTER_H_ */
