/*
 * modbus_master.h
 *
 *  Created on: Oct 14, 2020
 *      Author: adrian-estelio
 */
/**
 * @file
 * @brief Functions to handle the Modbus master funcionality.
 * */
#ifndef MAIN_INCLUDE_MODBUS_MASTER_H_
#define MAIN_INCLUDE_MODBUS_MASTER_H_
#include "stdbool.h"
#include "stdint.h"
/**
 * @brief Function to read a series of input register in a external slave
 *
 * @param[in] slave Slave which is been polled
 * @param[in] start_address Starting address of the registers
 * @param[in] quantity Quantity of registers to be polled after {@link
 * start_address}
 * */
void read_input_register(uint8_t slave, uint16_t start_address,
                         uint16_t quantity);
/**
 * @brief Function to save a series of input registers read in a slave
 *
 * @param[in] data Pointer to the data of the modbus frame
 * @param[in] length Length of the data
 * @param[in] modbus_registers Pointer to the pointes of the modbus registers
 * */
void save_register(uint8_t *data, uint8_t length, uint16_t **modbus_registers);

/**
 * @brief Function to check exception in the incoming data from a slave
 *
 * @param[in] data Pointer to the data of the modbus frame
 * */
int check_exceptions(uint8_t *data);
/**
 * @brief Function to start the Slave array used when the masters modbus polls
 *
 * @param[out] slaves Pointer to the array of bools which inidices represents
 * the slaves
 * */
void init_slaves(bool *slaves);
#endif /* MAIN_INCLUDE_MODBUS_MASTER_H_ */
