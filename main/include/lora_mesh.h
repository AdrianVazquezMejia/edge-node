/*
 * lora_mesh.h
 *
 *  Created on: Apr 23, 2021
 *      Author: adrian-estelio
 */

#ifndef MAIN_INCLUDE_LORA_MESH_H_
#define MAIN_INCLUDE_LORA_MESH_H_
#include "esp_rf1276.h"
#include "modbus_slave.h"

void prepare_to_send(lora_mesh_t *loraFrame, mb_response_t *modbus_response);
#endif /* MAIN_INCLUDE_LORA_MESH_H_ */
