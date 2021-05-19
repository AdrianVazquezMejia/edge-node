/*
 * lora_mesh.h
 *
 *  Created on: Apr 23, 2021
 *      Author: adrian-estelio
 */
/**
 *@file
 *@brief File for function that handle specific applications functions to LoRa.
 * */
#ifndef MAIN_INCLUDE_LORA_MESH_H_
#define MAIN_INCLUDE_LORA_MESH_H_
#include "esp_rf1276.h"
#include "modbus_slave.h"
/**
 * @brief Function to sort the Modbus frame into the LoRaFrame data structure
 * previously to be sent
 *
 * @param[out] loraFrame Pointer to to the data structure to be sent
 * @param[in] modbus_response Pointer to the modbus frame to be sent through
 * LoRa.
 * */
void prepare_to_send(lora_mesh_t *loraFrame, mb_response_t *modbus_response);
#endif /* MAIN_INCLUDE_LORA_MESH_H_ */
