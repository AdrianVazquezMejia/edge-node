/*
 * modbus_lora.h
 *
 *  Created on: Oct 21, 2020
 *      Author: adrian-estelio
 */

#ifndef MAIN_INCLUDE_MODBUS_LORA_H_
#define MAIN_INCLUDE_MODBUS_LORA_H_
#include "stdint.h"
typedef struct mb_response {
    uint8_t frame[256];
    uint8_t len;
} mb_response_t;
mb_response_t modbus_lora_functions(const uint8_t *frame, uint8_t lengh,
                                    uint16_t **modbus_registers);
#endif /* MAIN_INCLUDE_MODBUS_LORA_H_ */
