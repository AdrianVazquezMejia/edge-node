/**
 * @file
 * @brief File with function of LoRa functionalities
 */

#ifndef ESP_RF1276
#define ESP_RF1276
#include "driver/uart.h"
#include "freertos/queue.h"
#include "stdint.h"
#include <esp_err.h>

#define BUF_SIZE    (1024)     /**< Size of the uart queue*/
#define RD_BUF_SIZE (BUF_SIZE) /**< Size of the lora queue*/
#define UART_RF1276 UART_NUM_2 /**< Definition the UART 2 as the LoRa UART*/
#define DEBUG_LORA             /**< Definition to activate debug logs*/

/**
 * @brief Used to pass the UART LoRa configuration data.
 */
typedef struct name {
    uint8_t uart_tx; /**< TX GPIO connected to LoRA chip*/

    uint8_t uart_rx; /**< RX GPIO connected to LoRA chip*/

    int baud_rate; /**< Baurate to communicate with LoRa chip: 9600 by default*/

    uart_port_t uart_num; /**< UART connected to LoRa chip*/
} uart_lora_t;

/**
 * @struct
 * @brief Used to pass the Network LoRa configuration data.
 */
typedef struct {
    uint32_t baud_rate;   /**< Baudate to set the LoRa chip*/
    uint16_t network_id;  /**< Network Id to work with*/
    uint16_t node_id;     /**< ID of the LoRa node*/
    uint8_t power;        /**< Transmit power*/
    uint8_t routing_time; /**< Routing Time*/
    float freq;           /**< Frequency to work with, default 433 MHz*/
    uint8_t port_check;   /**< Port Check*/
} config_rf1276_t;

/**
 * @brief Union to divide word size data to byte size divisible data.
 */
typedef union {
    struct {
        uint8_t low_byte_;  /**< Lower byte*/
        uint8_t high_byte_; /**< Higher byte*/
    };
    uint16_t word_; /**< Word size data of the union*/

} word_t;

/**
 * @brief Union to divide double word size data to word size divisible data.
 */
typedef union {
    struct {
        word_t low_word_;  /**< Lower word data of the struct*/
        word_t high_word_; /**< Upper data of the struct*/
    };                     /**< Struct divisible */
    uint32_t doubleword_;  /**< Double word size of the data*/
} doubleword_t;

/**
 * @brief LoRa struct with the frame load of Routing information.
 */
typedef struct {
    word_t dest_address_;    /**< Destination address of the node*/
    uint8_t ack_request_;    /**< Request of acknowledge of sent data*/
    uint8_t sending_radius_; /**< Routing lifetime*/
    uint8_t routing_type_;   /**< Routing Type*/
    uint8_t data_len_;       /**< Len of the actual payload*/
    uint8_t data_[109];      /**< Payload */
} lora_rout_load_t;

/**
 * @brief Struct with the frame load of received data.
 */
typedef struct {
    word_t source_address_; /**< ID of the source of the data*/
    uint8_t signal_;        /**< Signal information of the last hop*/
    uint8_t data_len_;      /**< Actual payload len*/
    uint8_t data_[113];     /**< Payload*/
} lora_recv_load_t;

/**
 * @brief Struct of config frame.
 * @note Not implemented, but about to be done
 */
typedef struct {

} lora_conf_load_t;

/**
 * @brief Struct of the load frame of the local response.
 */
typedef struct {
    word_t dest_address_; /**< Destination address which the data was sent to*/
    uint8_t result;       /**< Result code of the process*/
} lora_local_resp_t;

/**
 * @brief Union of the different load frames.
 */
typedef union {
    lora_rout_load_t transmit_load_; /**< Struct of transmit Load*/
    lora_recv_load_t recv_load_;     /**< Struct of receive Load*/
    lora_local_resp_t local_resp_;   /**< Struct of Local response Load*/
    lora_conf_load_t config_load_;   /**< Struct of Config Load*/
} lora_load_t;

/**
 * @brief Struct of the header of the LoRa frame.
 */
typedef struct {
    uint8_t load_len_;     /**< Load frame length*/
    uint8_t command_type_; /**< Command type, determines the purpose of the load
                              frame regading the frame type*/
    uint8_t frame_number_; /**< The frame number is currently unused and the
                              value is fixed to 0x00.*/
    uint8_t frame_type_;   /**<The frame type is used to identify different
                              application frame types.*/
} lora_header_t;

/**
 * @brief Struct of the whole LoRa frame.
 */
typedef struct {
    uint8_t check_sum_; /**< The frame end is one byte checksum.This checksum is
                           the result of XOR operation from the first byte to
                           the last second byte*/
    lora_load_t load_;  /**<Its format is determined by the types of frames and
                           the types of  commands*/
    lora_header_t header_; /**< Header of the frame*/

} lora_mesh_t;

enum {
    INTERNAL_USE     = 0x01,
    APPLICATION_DATA = 0x05
}; /**< Enum of the frame types. */
enum {
    ACK_SEND     = 0x81,
    RECV_PACKAGE = 0x82
};                            /**< Enum of the  receiving Commands types. */
enum { SENDING_DATA = 0x01 }; /**< Enum of the sending command types. */

enum {
    ACK_RESET_MODEM = 0x87
}; /**< Enum of the internal used command types. */

/**
 *@brief Function to send data to other node using the LoRa peripheral
 *
 *@param sendFrame struct with the data to send, and additional routing
 *information.
 *@return >=0 on success
 *@see lora_mesh_t
 */
int lora_send(lora_mesh_t *sendFrame);

/**
 *@brief Function to start serial port LoRa peripheral
 *
 *@param uartParameters struct with the data to set the serial port to Lora up.
 *@see uart_lora_t
 */
esp_err_t init_lora_uart(uart_lora_t *uartParameters);

/**
 *@brief Function to set up LoRa peripheral
 *
 *@param uart_num uart number where the LoRa is connected to.
 *@param loraParameters struct with the data to set the network Lora up.
 *@param loraQueue Queue to pass data from the backend.
 *@see uart_lora_t
 *@see config_rf1276_t
 *@see QueueHandle_t
 */
esp_err_t init_lora_mesh(config_rf1276_t *loraParameters,
                         QueueHandle_t *loraQueue, uart_port_t uart_num);

int lora_reset_modem(void);

uint8_t CHECK_SUM(uint8_t *frame, uint8_t len);
#endif
