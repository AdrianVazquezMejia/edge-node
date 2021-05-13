#include "include/esp_rf1276.h"

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string.h>

static const char *RF1276 = "ESP_RF1276";

QueueHandle_t *mainQueue;
QueueHandle_t uartQueue;

int baud_rate_aux;

#define WRITE_FLAG 0x01
static uint8_t CHECK_SUM(uint8_t *frame, uint8_t len) {

    uint8_t checkSum = frame[0];
    for (uint i = 0; i < len - 1; i++) {
        checkSum = checkSum ^ frame[i + 1];
    }
    return checkSum;
}
static void recv_data_prepare(lora_mesh_t *recvParameter,
                              const uint8_t *uart_data, const uint8_t len) {
    recvParameter->header_.frame_type_   = uart_data[0];
    recvParameter->header_.frame_number_ = uart_data[1];
    recvParameter->header_.command_type_ = uart_data[2];
    recvParameter->header_.load_len_     = uart_data[3];

    switch (recvParameter->header_.frame_type_) {
    case INTERNAL_USE:
        /// TODO
        break;
    case APPLICATION_DATA:
        switch (recvParameter->header_.command_type_) {

        case ACK_SEND:
            recvParameter->load_.local_resp_.dest_address_.high_byte_ =
                uart_data[4];
            recvParameter->load_.local_resp_.dest_address_.low_byte_ =
                uart_data[5];
            recvParameter->load_.local_resp_.result = uart_data[6];
            recvParameter->check_sum_               = uart_data[7];
            break;
        case RECV_PACKAGE:
            recvParameter->load_.recv_load_.source_address_.high_byte_ =
                uart_data[4];
            recvParameter->load_.recv_load_.source_address_.low_byte_ =
                uart_data[5];
            recvParameter->load_.recv_load_.signal_   = uart_data[6];
            recvParameter->load_.recv_load_.data_len_ = uart_data[7];
            memcpy(recvParameter->load_.recv_load_.data_, uart_data + 8,
                   recvParameter->load_.recv_load_.data_len_);
            recvParameter->check_sum_ =
                uart_data[8 + recvParameter->load_.recv_load_.data_len_];
            break;
        default:
#ifdef DEBUG_LORA
            ESP_LOGW(RF1276, "Unknown Application Data Command Type");
#endif
            break;
        }

        break;
    default:
#ifdef DEBUG_LORA
        ESP_LOGW(RF1276, "Unknown Received Frame Type");
#endif
        break;
    }
}
static void uart_event_task(void *pvParameters) {
    uart_event_t event;
    uint8_t *dtmp          = (uint8_t *)malloc(RD_BUF_SIZE);
    lora_mesh_t *recvFrame = (lora_mesh_t *)malloc(sizeof(lora_mesh_t));
    while (1) {
        if (xQueueReceive(uartQueue, (void *)&event,
                          (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            switch (event.type) {
            case UART_DATA:
                uart_read_bytes(UART_RF1276, dtmp, event.size,
                                pdMS_TO_TICKS(500));
#ifdef DEBUG_LORA
                for (int i = 0; i < event.size; i++)
                    ESP_LOGI(RF1276, "LoRa frame %x", dtmp[i]);
#endif
                if (CHECK_SUM(dtmp, event.size) != 0) {
#ifdef DEBUG_LORA
                    ESP_LOGW(RF1276, "Check sum error");
#endif
                    break;
                }
                recv_data_prepare(recvFrame, dtmp, event.size);
                xQueueSend(*mainQueue, recvFrame, portMAX_DELAY);
                break;

            default:
                break;
            }
        }
    }
    free(dtmp);
    free(recvFrame);
    dtmp = NULL;
    vTaskDelete(NULL);
}

esp_err_t init_lora_uart(uart_lora_t *uartParameters) {

    esp_err_t err;
    int loraSerialBaudarate = uartParameters->baud_rate;
    int loraUARTTX          = uartParameters->uart_tx;
    int loraUARTRX          = uartParameters->uart_rx;
    uart_port_t loraUARTNUM = uartParameters->uart_num;

    uart_config_t uart_config = {.baud_rate = loraSerialBaudarate,
                                 .data_bits = UART_DATA_8_BITS,
                                 .parity    = UART_PARITY_DISABLE,
                                 .stop_bits = UART_STOP_BITS_1,
                                 .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    err                       = uart_param_config(loraUARTNUM, &uart_config);
    if (err == ESP_FAIL) {
        return err;
    }

    err = uart_set_pin(UART_RF1276, loraUARTTX, loraUARTRX, UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err == ESP_FAIL) {
        return err;
    }

    err =
        uart_driver_install(loraUARTNUM, BUF_SIZE, BUF_SIZE, 20, &uartQueue, 0);
    if (err == ESP_FAIL) {
        return err;
    }

    return err;
}

esp_err_t init_lora_mesh(config_rf1276_t *loraParameters,
                         QueueHandle_t *loraQueue, uart_port_t uart_num) {

    uint8_t sendFrame[18];
    uint8_t auxFrameV20[18];
    uint8_t recvFrame[18];
    sendFrame[0] = 0x01;
    sendFrame[1] = 0x00;
    sendFrame[2] = 0x01;
    sendFrame[3] = 0x0d;
    sendFrame[4] = 0xa5;
    sendFrame[5] = 0xa5;
    doubleword_t frequency;
    doubleword_t networkID;
    doubleword_t nodeID;
    frequency.doubleword_ =
        (uint32_t)(loraParameters->freq * 1000000000 / 61035);
    sendFrame[6]          = frequency.high_word_.low_byte_;
    sendFrame[7]          = frequency.low_word_.high_byte_;
    sendFrame[8]          = frequency.low_word_.low_byte_;
    sendFrame[9]          = loraParameters->power;
    sendFrame[10]         = loraParameters->routing_time;
    networkID.doubleword_ = (uint32_t)loraParameters->network_id;
    sendFrame[11]         = networkID.low_word_.high_byte_;
    sendFrame[12]         = networkID.low_word_.low_byte_;
    nodeID.doubleword_    = (uint32_t)loraParameters->node_id;
    sendFrame[13]         = nodeID.low_word_.high_byte_;
    sendFrame[14]         = nodeID.low_word_.low_byte_;
    sendFrame[15]         = loraParameters->baud_rate;
    sendFrame[16]         = loraParameters->port_check;
    sendFrame[17]         = CHECK_SUM(sendFrame, sizeof(sendFrame) - 1);

    uart_write_bytes(uart_num, (const char *)sendFrame, sizeof(sendFrame));

    uint8_t readBytes = uart_read_bytes(uart_num, recvFrame, sizeof(recvFrame),
                                        pdMS_TO_TICKS(1000));

    uint8_t checkSum = CHECK_SUM(recvFrame, sizeof(recvFrame));
    if (!readBytes && checkSum)
        return ESP_FAIL;

    sendFrame[2]  = 0x81;
    sendFrame[3]  = 0x0c;
    sendFrame[17] = CHECK_SUM(sendFrame, sizeof(sendFrame) - 1);

    memcpy(auxFrameV20, sendFrame, sizeof(sendFrame));
    auxFrameV20[2]  = 0x81;
    auxFrameV20[3]  = 0x0d; // Version 4.0
    auxFrameV20[17] = CHECK_SUM(auxFrameV20, sizeof(sendFrame) - 1);

    if ((memcmp(sendFrame, recvFrame, sizeof(sendFrame)) != 0) &&
        (memcmp(auxFrameV20, recvFrame, sizeof(sendFrame)) != 0))
        return ESP_FAIL;
    mainQueue = loraQueue;
    uart_read_bytes(uart_num, recvFrame, 33, pdMS_TO_TICKS(4000));
    ESP_LOGI(RF1276, "%s", recvFrame); // Version
    if (xTaskCreate(uart_event_task, "uart_event_task", 2048 * 2, NULL,
                    configMAX_PRIORITIES - 1, NULL) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

int lora_send(lora_mesh_t *sendFrame) {

    uint8_t lenSendData     = 5 + sendFrame->header_.load_len_;
    uint8_t *serialSendData = (uint8_t *)malloc(lenSendData);
    int result;

    serialSendData[0] = sendFrame->header_.frame_type_;
    serialSendData[1] = sendFrame->header_.frame_number_;
    serialSendData[2] = sendFrame->header_.command_type_;
    serialSendData[3] = sendFrame->header_.load_len_;
    serialSendData[4] =
        sendFrame->load_.transmit_load_.dest_address_.high_byte_;
    serialSendData[5] = sendFrame->load_.transmit_load_.dest_address_.low_byte_;
    serialSendData[6] = sendFrame->load_.transmit_load_.ack_request_;
    serialSendData[7] = sendFrame->load_.transmit_load_.sending_radius_;
    serialSendData[8] = sendFrame->load_.transmit_load_.routing_type_;
    serialSendData[9] = sendFrame->load_.transmit_load_.data_len_;
    memcpy(serialSendData + 10, sendFrame->load_.transmit_load_.data_,
           serialSendData[9]);
    serialSendData[10 + serialSendData[9]] =
        CHECK_SUM(serialSendData, lenSendData - 1);
#ifdef DEBUG_LORA
    for (int i = 0; i < lenSendData; i++)
        ESP_LOGI(RF1276, "LoRa send frame %x", serialSendData[i]);
#endif
    result = uart_write_bytes(UART_RF1276, (const char *)serialSendData,
                              lenSendData);

    free(serialSendData);
    return result;
}
