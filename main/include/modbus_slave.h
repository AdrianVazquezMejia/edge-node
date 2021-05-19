/*
 * modbus_slave.h
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */

/**
 * @file
 * @brief Library to handle the modbus slave functions
 * */
#ifndef MAIN_INCLUDE_MODBUS_SLAVE_H_
#define MAIN_INCLUDE_MODBUS_SLAVE_H_
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
/**
 * @brief Union to separate the the 16 bit registers into 8 bits parts.
 * */
typedef union {
    uint16_t Val; /**< Word size part*/
    /**
     * @brief Struct for byte separation into most significant and lest
     * significant byte.
     * */
    struct {
        uint8_t LB; /**< Lower byte*/
        uint8_t HB; /**< Upper byte*/
    } byte;
} INT_VAL;
/**
 * @brief Struct to handle modbus frame and length s
 * */
typedef struct {
    uint8_t frame[256]; /**< Frame modbus, 256 bytes max length*/
    uint8_t len;        /**< Length of the frame*/
} mb_response_t;
/**
 * @brief Function to start the uart that handles the Modbus RTU serial
 * communication
 *
 * @param[in] queue Pointer to the queue that communicates the back-end with the
 * front-end task.
 * */
void uart_init(QueueHandle_t *queue);

/**
 * @brief Function to execute the modbus request and process the answers.
 *
 * @param[out] response_frame Pointer to the structure that will have the
 * reponse frame modbus and its length.
 * @param[in] frame Input frame received from the master.s
 * @param[in] length Length of the input frame.
 * */
int modbus_slave_functions(mb_response_t *response_frame, const uint8_t *frame,
                           uint8_t length, uint16_t **modbus_registers);
/**
 * @brief Function to save the pulse counter to the input registers.
 *
 * The data i saved in the index of the register corresponding with the ID and
 * ID+1.
 * @param[in] value Counter value
 * @param[in] modbus_register Pointer to the input registers array.
 * */
void register_save(uint32_t value, uint16_t *modbus_register);
/**
 * @brief Function to normalize the pulses to a virtual meter with constat 100
 * pulses per KWh.
 *
 * @param[in] value Counter to be normalized
 * @return Normalized pulse
 * */
uint32_t normalize_pulses(uint32_t value);
/**
 * @brief Function to create the CRC exception Modbus
 *
 * @param[in] frame Pointer to the input modbus frame
 * @param[out] response_modbus Pointer to the output data structure which
 * contains the answer modbus
 * */
void crc_error_response(mb_response_t *response_frame, const uint8_t *frame);
#endif /* MAIN_INCLUDE_MODBUS_SLAVE_H_ */
