/*
 * modbus_slave.c
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
#include "modbus_slave.h"
#include "modbus_master.h"
#include "CRC.h"
#include "config_rtu.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "global_variables.h"
#include "math.h"
#include "stdint.h"
#include "string.h"
#include "esp_rf1276.h"
#ifdef CONFIG_PRODUCTION
#define RTS_PIN 25
#else
#define RTS_PIN 27
#endif
#define TXD_PIN     33
#define RXD_PIN     26
#define CTS_PIN     2
#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024

#define EXCEPTION_LEN 3




typedef union {
    uint32_t doubleword;
    struct {
        uint16_t wordL;
        uint16_t wordH;
    } word;

} WORD_VAL;

static char *TAG = "MODBUS SLAVE";

uint8_t queue_size = 0;
uint8_t queue_changed_slaves[256] = {0};

void uart_init(QueueHandle_t *queue) {
    uart_driver_delete(UART_NUM_1);
    int uart_baudarate              = 9600;
    const uart_config_t uart_config = {
        .baud_rate = uart_baudarate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, RTS_PIN, CTS_PIN);
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, 20, queue,
                        0);
    uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX);

    gpio_set_direction(CLOSE_RELAY, GPIO_MODE_OUTPUT);
    gpio_set_direction(OPEN_RELAY, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CLOSE_RELAY, GPIO_PULLDOWN_ONLY);
    gpio_set_pull_mode(OPEN_RELAY, GPIO_PULLDOWN_ONLY);
}

void crc_error_response(mb_response_t *response_frame, const uint8_t *frame) {
    uint8_t response_len = 0;
    INT_VAL CRC;

    response_frame->frame[0] = NODE_ID;
    response_frame->frame[1] = frame[1] + 0x80;
    response_frame->frame[2] = 0x08;
    response_len             = EXCEPTION_LEN;
    CRC.Val                  = CRC16(response_frame->frame, response_len);
    response_frame->frame[response_len++] = CRC.byte.LB;
    response_frame->frame[response_len++] = CRC.byte.HB;
    response_frame->len                   = response_len;
}

void access_write_key_validate(uint8_t *frame){
	if(CHECK_SUM(frame, 6)==0){
		 modbus_coils[0] = true;
	ESP_LOGE(TAG, "Access granted");}
	else{
		ESP_LOGE(TAG, "Access Denied");
	}
}

int modbus_slave_functions(mb_response_t *response_frame, uint8_t *frame,
                           uint8_t length, uint16_t **modbus_registers) {
    uint8_t FUNCTION = frame[1];
    INT_VAL address;
    address.byte.HB = frame[2];
    address.byte.LB = frame[3];
    INT_VAL value;
    value.byte.HB = frame[4];
    value.byte.LB = frame[5];
    INT_VAL CRC;
    uint8_t response_len     = 0;
    INT_VAL *inputRegister   = (INT_VAL *)modbus_registers[1];
    modbus_poll_event_t* poll_event = malloc(sizeof(modbus_poll_event_t));
    response_frame->frame[0] = frame[0];
    response_frame->frame[1] = frame[1];
    slave_to_change          = 0;
	gpio_set_level(CLOSE_RELAY, 0);
	gpio_set_level(OPEN_RELAY, 0);


    if (CRC16(frame, length) == 0) {
        switch (FUNCTION) {
        case READ_INPUT:
            response_frame->frame[2] = frame[5] * 2;
            response_len             = 3;
            for (uint16_t i = 0; i < value.Val; i++) {
                if (modbus_coils[(address.Val + i + 1) / 2]) {
                    response_frame->frame[response_len++] = 0;
                    response_frame->frame[response_len++] = 0;
                } else {
                    response_frame->frame[response_len] =
                        inputRegister[address.Val + i].byte.HB;
                    response_len++;
                    response_frame->frame[response_len] =
                        inputRegister[address.Val + i].byte.LB;
                    response_len++;
                }
            }
            CRC.Val = CRC16(response_frame->frame, response_len);
            response_frame->frame[response_len++] = CRC.byte.LB;
            response_frame->frame[response_len++] = CRC.byte.HB;
            response_frame->len                   = response_len;
            break;
        case WRITE_SINGLE_COIL:
            ESP_LOGI(TAG, "Writing a COIl");

			if (address.Val == 0) {
				ESP_LOGE(TAG, "ERROR");
				esp_restart();
			}
#ifdef CONFIG_MASTER_MODBUS
            access_write_key_validate(frame);
#endif
#ifndef CONFIG_MASTER_MODBUS
            modbus_coils[0] = true;
#endif
            value.byte.LB = 0;
            if(modbus_coils[0]){

				if (address.Val == NODE_ID) {
					if(value.Val == 0xff00){
						ESP_LOGE(TAG, "Setting to 1");
						gpio_set_level(OPEN_RELAY, 1);

					}
					else if(value.Val == 0x0000) {
						gpio_set_level(CLOSE_RELAY, 1);
						ESP_LOGE(TAG, "Setting to 0");
					}
				}

				if (address.Val != NODE_ID){
					queue_size++;
					ESP_LOGW(TAG, "Queue size %d, slave %d changed", queue_size,(uint8_t)address.Val);
					queue_changed_slaves[queue_size-1] = (uint8_t)address.Val;
					poll_event->slave =  address.Val;
					poll_event->function = WRITE_SINGLE_COIL;
					if (value.Val == 0xff00) {
						ESP_LOGW(TAG, "Setting to 1 ");
						modbus_coils[address.Val] = true;
						poll_event->data.coil_state= true;

					} else if(value.Val == 0x0000){
						ESP_LOGW(TAG, "Setting to 0");
						modbus_coils[address.Val] = false;
						poll_event->data.coil_state= false;

					}
					xQueueSend(uart_send_queue,poll_event,pdMS_TO_TICKS(TIME_SCAN));
				}

            }
			modbus_coils[0] = false;
            memcpy(response_frame->frame, frame, length);
            response_frame->len = length;
            ESP_LOG_BUFFER_HEX(TAG, response_frame->frame, response_frame->len);
            ESP_LOGI(TAG, "---------------------------------------------------");
            break;
        case WRITE_MULTIPLES_COILS:

            ESP_LOGW(TAG, "Writing Multiples coils");
            response_len = 6;
            memcpy(response_frame->frame, frame, response_len);
            CRC.Val = CRC16(response_frame->frame, response_len);
            response_frame->frame[response_len++] = CRC.byte.LB;
            response_frame->frame[response_len++] = CRC.byte.HB;
            ESP_LOG_BUFFER_HEX(TAG, response_frame->frame, response_frame->len);
            response_frame->len = response_len;
            ESP_LOGI(TAG, "-----------");
            break;

        default:
            response_frame->frame[0] = NODE_ID;
            response_frame->frame[1] = frame[1] + 0x80;
            response_frame->frame[2] = 0x01;
            response_len             = EXCEPTION_LEN;
            CRC.Val = CRC16(response_frame->frame, response_len);
            response_frame->frame[response_len++] = CRC.byte.LB;
            response_frame->frame[response_len++] = CRC.byte.HB;
            response_frame->len                   = response_len;

            return response_frame->frame[2];

            break;
        }
    } else {
        crc_error_response(response_frame, frame);
        return response_frame->frame[2];
    }
    return ESP_OK;
}
void register_save(uint32_t value, uint16_t *modbus_register) {
    WORD_VAL aux_register;
    aux_register.doubleword      = normalize_pulses(value);
    modbus_register[NODE_ID]     = aux_register.word.wordH;
    modbus_register[NODE_ID + 1] = aux_register.word.wordL;
}

uint32_t normalize_pulses(uint32_t value) {
    uint32_t normalized_value;
    double value_kWh = (double)value / (double)IMPULSE_CONVERSION;
    normalized_value = round(value_kWh / 0.01);
    return normalized_value;
}
