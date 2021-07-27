/*
 * modbus_master.c
 *
 *  Created on: Oct 14, 2020
 *      Author: adrian-estelio
 */
#include "modbus_master.h"
#include "CRC.h"
#include "config_rtu.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "global_variables.h"
#include "stdint.h"
#define TX_BUF_SIZE 1024
static char *TAG = "MASTER";
bool modbus_coils[512];
typedef union {
    uint16_t Val;
    struct {
        uint8_t LB;
        uint8_t HB;
    } byte;

} INT_VAL;
typedef union {
    uint32_t doubleword;
    struct {
        uint16_t wordL;
        uint16_t wordH;
    } word;

} WORD_VAL;
enum modbus_function_t { READ_HOLDING = 3, READ_INPUT };
enum modbus_excep_t {
    ILLEGAL_FUNC = 1,
    ILLEGAL_DATA_AD,
    ILLEGAL_DATA,
    SLAVE_FAIL,
    ACK,
    BUSY_SLAVE,
    NEG_ACK,
    PAR_ERR
};
uint8_t slave_to_change;
void write_single_coil(uint8_t slave, bool coil_state) {
    INT_VAL address;
    INT_VAL Number;
    INT_VAL CRC;

    uint8_t *frame = (uint8_t *)malloc(TX_BUF_SIZE);
    frame[0]       = slave;
    frame[1]       = 0x05;
    address.Val    = slave;
    frame[2]       = address.byte.HB;
    frame[3]       = address.byte.LB;
    if (coil_state == true) {
        Number.Val = 0xff00;
    } else {
        Number.Val = 0x0000;
    }
    frame[4] = Number.byte.HB;
    frame[5] = Number.byte.LB;
    CRC.Val  = CRC16(frame, 6);
    frame[6] = CRC.byte.LB;
    frame[7] = CRC.byte.HB;
    if (uart_write_bytes(UART_NUM_1, (const char *)frame, 8) == ESP_FAIL) {
        ESP_LOGE(TAG, "Error writing UART data");
        free(frame);
        return;
    }
    free(frame);
    ESP_LOGI(TAG, "Poll sent to slave : %d...", slave_to_change);
    slave_to_change = 0;
}
void read_input_register(uint8_t slave, uint16_t start_address,
                         uint16_t quantity) {
    INT_VAL address;
    INT_VAL Number;
    INT_VAL CRC;

    uint8_t *frame = (uint8_t *)malloc(TX_BUF_SIZE);
    frame[0]       = slave;
    frame[1]       = 0x04;
    address.Val    = start_address;
    frame[2]       = address.byte.HB;
    frame[3]       = address.byte.LB;
    Number.Val     = quantity;
    frame[4]       = Number.byte.HB;
    frame[5]       = Number.byte.LB;
    CRC.Val        = CRC16(frame, 6);
    frame[6]       = CRC.byte.LB;
    frame[7]       = CRC.byte.HB;
    if (uart_write_bytes(UART_NUM_1, (const char *)frame, 8) == ESP_FAIL) {
        ESP_LOGE(TAG, "Error writing UART data");
        free(frame);
        return;
    }
    free(frame);
    ESP_LOGI(TAG, "Poll sent to slave : %d...", slave);
}
static void check_toletance(uint8_t slave, INT_VAL *tolerance_register,
                            uint16_t *input_register) {
    WORD_VAL previous;
    WORD_VAL current;
    const uint TOLERANCE = 1000;

    previous.word.wordH = tolerance_register[0].Val;
    previous.word.wordL = tolerance_register[1].Val;
    current.word.wordH  = input_register[slave * 2 - 1];
    current.word.wordL  = input_register[slave * 2];
    uint slope          = abs(current.doubleword - previous.doubleword);
    modbus_coils[slave] = false;
    if (slope > TOLERANCE) {
        modbus_coils[slave] = true;
        ESP_LOGW(TAG, "Noise overcome the predetermined trigger [%d]",
                 modbus_coils[slave]);
    }
}
void save_register(uint8_t *data, uint8_t length, uint16_t **modbus_registers) {
    uint8_t FUNCTION        = data[1];
    uint16_t *inputRegister = modbus_registers[1];
    uint8_t SLAVE           = data[0];
    switch (FUNCTION) {
    case READ_INPUT:;
        uint8_t byte_count = data[2];
        INT_VAL aux_registro;
        INT_VAL *tolerance_register =
            (INT_VAL *)malloc((byte_count / 2) * sizeof(INT_VAL));
        for (uint8_t i = 0; i < byte_count / 2; i++) {
            aux_registro.byte.HB             = data[3 + 2 * i];
            aux_registro.byte.LB             = data[4 + 2 * i];
            tolerance_register[i].Val        = inputRegister[SLAVE * 2 + i - 1];
            inputRegister[SLAVE * 2 + i - 1] = aux_registro.Val;
        }
        check_toletance(SLAVE, tolerance_register, inputRegister);
        ESP_LOGI(TAG, "Received data saved...");
        free(tolerance_register);
        break;
    }
}
#ifdef CONFIG_MASTER_MODBUS
void init_slaves(bool *slaves) {
    int i = 0;
    for (i = 0; i < 256; i++)
        slaves[i] = false;
    for (i = 1; i <= SLAVES; i++)
        slaves[i] = true;
}
#endif

int check_exceptions(uint8_t *data) {
    uint8_t FUNCTION  = data[1];
    uint8_t SLAVE     = data[0];
    uint8_t EXCEPTION = data[2];
    if (FUNCTION == 0x04)
        return ESP_OK;
    switch (EXCEPTION) {
    case ILLEGAL_FUNC:
        ESP_LOGE(TAG, "Invalid Function %d requested in slave %d",
                 FUNCTION - 0x80, SLAVE);
        break;
    case ILLEGAL_DATA_AD:
        ESP_LOGE(TAG, "Invalid data address requested in slave %d", SLAVE);
        break;
    case ILLEGAL_DATA:
        ESP_LOGE(TAG, "Invalid data value requested in slave %d", SLAVE);
        break;
    case SLAVE_FAIL:
        ESP_LOGE(TAG,
                 "Slave  %d fail attempting to execute the requested action",
                 SLAVE);
        break;
    case ACK:
        ESP_LOGE(TAG, "Slave %d ACK, more time is needed ", SLAVE);
        break;
    case BUSY_SLAVE:
        ESP_LOGE(TAG, "Slave %d is busy, requested action rejected ", SLAVE);
        break;
    case NEG_ACK:
        ESP_LOGE(TAG,
                 "Slave %d ACK can not perform the action with the "
                 "requested query ",
                 SLAVE);
        break;
    case PAR_ERR:
        ESP_LOGE(TAG, "Slave %d received data with CRC ERROR ", SLAVE);
        break;
    default:
        ESP_LOGE(TAG, "Unknown EXCEPTION (%d) in slave %d  ", EXCEPTION, SLAVE);
    }
    return ESP_FAIL;
}
