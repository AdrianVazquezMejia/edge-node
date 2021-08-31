/*
 * global_variables.h
 *
 *  Created on: Apr 7, 2021
 *      Author: adrian-estelio
 */
/**
 * @file
 *
 * @brief This contain the declaration of the global variables
 * */

#ifndef MAIN_INCLUDE_GLOBAL_VARIABLES_H_
#define MAIN_INCLUDE_GLOBAL_VARIABLES_H_
#include "flash.h"
#include "freertos/semphr.h"
#include <stdint.h>

#define TIME_SCAN  1000

extern uint8_t NODE_ID; /**< ID of this node, can be accessible globally. */

extern int IMPULSE_CONVERSION;  /**< Constat of the energy meter, used to
                                   nomalize the pulses quantity */
extern uint32_t INITIAL_ENERGY; /**< Energy in the enery meter when couples with
                                   this device */
extern uint8_t
    SLAVES; /**<Slave quantity, used to poll if this is a master modbus*/
extern nvs_address_t
    pulse_address; /**< Address where is currently saving the pulses counter,
                      can be accessible globally. */
extern SemaphoreHandle_t
    smph_pulse_handler; /**< semaphore to the pulse input interruption*/

extern bool modbus_coils[512];

extern uint8_t slave_to_change;

extern uint8_t queue_size;
extern uint8_t queue_changed_slaves[256];

extern QueueHandle_t uart_send_queue;

#endif /* MAIN_INCLUDE_GLOBAL_VARIABLES_H_ */
