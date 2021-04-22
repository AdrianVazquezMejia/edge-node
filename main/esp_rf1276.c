#include "include/esp_rf1276.h"

static const char *RF1276 = "ESP_RF1276";

QueueHandle_t *cola_data;
QueueHandle_t cola_config;
QueueHandle_t cola_general;
QueueHandle_t cola_tamano;
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
            ESP_LOGW(RF1276, "Unknown Application Data Command Type");

            break;
        }

        break;
    default:
        ESP_LOGW(RF1276, "Unknown Received Frame Type");
        break;
    }
}
static void uart_event_task(void *pvParameters) {
    ESP_LOGI(RF1276, "Configurado Eventos UART%d", UART_RF1276);

    uart_event_t event;
    uint8_t *dtmp          = (uint8_t *)malloc(RD_BUF_SIZE);
    lora_mesh_t *recvFrame = (lora_mesh_t *)malloc(sizeof(lora_mesh_t));
    while (1) {
        if (xQueueReceive(uartQueue, (void *)&event,
                          (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            switch (event.type) {
            case UART_DATA:
                ESP_LOGI(RF1276, "Primero esto : %d", event.size);
                uart_read_bytes(UART_RF1276, dtmp, event.size,
                                pdMS_TO_TICKS(500));
                for (int i = 0; i < event.size; i++)
                    ESP_LOGI(RF1276, "LoRa frame %x", dtmp[i]);
                if (CHECK_SUM(dtmp, event.size) != 0) {
                    ESP_LOGW(RF1276, "Check sum error");
                    break;
                }

                recv_data_prepare(recvFrame, dtmp, event.size);
                xQueueSend(*mainQueue, recvFrame, portMAX_DELAY);
                break;

            default:
                ESP_LOGI(RF1276, "uart event type: %d", event.type);
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

    if (memcmp(sendFrame, recvFrame, sizeof(sendFrame)))
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

void lora_send(lora_mesh_t *sendFrame) {

    uint8_t lenSendData     = 5 + sendFrame->header_.load_len_;
    uint8_t *serialSendData = (uint8_t *)malloc(lenSendData);

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

    for (int i = 0; i < lenSendData; i++)
        ESP_LOGW(RF1276, "LoRa send frame %x", serialSendData[i]);
    uart_write_bytes(UART_RF1276, (const char *)serialSendData, lenSendData);
    free(serialSendData);
}

void loRa_mesh_data_stack(void *param) {
    uint8_t buffer[550];
    uint16_t tamano;

    while (1) {
        xQueueReceive(cola_tamano, &tamano, portMAX_DELAY);
        xQueueReceive(cola_general, buffer, portMAX_DELAY);

        switch (buffer[0]) {

        case 0x01:

            switch (buffer[2]) {

            case 0x81:
                ESP_LOGI(RF1276, "RESPONSE WRITE CONFIG");
                xQueueSend(cola_config, buffer, portMAX_DELAY);
                break;

            case 0x82:
                ESP_LOGI(RF1276, "RESPONSE READ CONFIG");
                xQueueSend(cola_config, buffer, portMAX_DELAY);
                break;

            case 0x83:
                ESP_LOGI(RF1276, "RESPONSE READ ROUTING TABLE");
                xQueueSend(cola_config, buffer, portMAX_DELAY);
                break;

            case 0x86:
                ESP_LOGI(RF1276, "RESPONSE READ VERSION");
                xQueueSend(cola_config, buffer, portMAX_DELAY);
                break;

            case 0x87:
                ESP_LOGI(RF1276, "RESPONSE RESET MODEM");
                xQueueSend(cola_config, buffer, portMAX_DELAY);
                break;

            case 0x88:
                ESP_LOGI(RF1276, "RESPONSE READ ROUTING PATH");
                xQueueSend(cola_config, buffer, portMAX_DELAY);
                break;

            default:
                break;
            }
            break;

        case 0x05:

            switch (buffer[2]) {

            case 0x81:
                ESP_LOGI(RF1276, "MODULE LOCAL RESPONSE");
                xQueueSend(cola_config, buffer, portMAX_DELAY);
                break;

            case 0x82:
                ESP_LOGI(RF1276, "RECEIVED PACKET");
                xQueueSend(*cola_data, buffer, portMAX_DELAY);
                break;

            default:
                break;
            }
            break;

        case 0x39:
            printf("Version: ");
            for (int i = 0; i < tamano; i++) {
                printf("%c", buffer[i]);
            }
            xEventGroupSetBits(eventos_rf1276, WRITE_FLAG);
            break;

        default:
            break;
        }
    }
    vTaskDelete(NULL);
}

esp_err_t start_lora_mesh(uart_lora_t uart_config,
                          struct config_rf1276 config_mesh,
                          QueueHandle_t *cola) {

    esp_err_t error = 0;

    // error = setup_esp_uart(&uart_config);

    if (error != ESP_OK) {
        return ESP_FAIL;
    }

    eventos_rf1276 = xEventGroupCreate();
    cola_config    = xQueueCreate(1, 39);
    cola_general   = xQueueCreate(1, 550);
    cola_tamano    = xQueueCreate(1, 2);

    xTaskCreate(loRa_mesh_data_stack, "LoRa Mesh Stack", 1024 * 4, NULL, 1,
                NULL);
    baud_rate_aux = uart_config.baud_rate;
    error         = write_config_esp_rf1276(&config_mesh);

    if (error != ESP_OK) {
        printf("Error Write Config\r\n");
        return ESP_FAIL;
    }

    cola_data = cola;

    return ESP_OK;
}

int write_config_esp_rf1276(struct config_rf1276 *config) {

    int info;

    struct trama_write_config trama;

    info = write_config_rf1276(config, &trama);

    if (info != 0) {
        return info;
    }

    uart_write_bytes(UART_RF1276, (const char *)trama.trama_tx,
                     sizeof(trama.trama_tx));

    xQueueReceive(cola_config, &(trama.trama_rx), pdMS_TO_TICKS(5000));

    if (trama.trama_rx[0] != 0x01) {
        ESP_LOGW(RF1276, "Baudrate erroneo %d", baud_rate_aux);

        switch (baud_rate_aux) {
        case 1200:
            baud_rate_aux = 2400;
            break;

        case 2400:
            baud_rate_aux = 4800;
            break;

        case 4800:
            baud_rate_aux = 9600;
            break;

        case 9600:
            baud_rate_aux = 19200;
            break;

        case 19200:
            baud_rate_aux = 38400;
            break;

        case 38400:
            baud_rate_aux = 57600;
            break;

        case 57600:
            baud_rate_aux = 115200;
            break;

        case 115200:
            baud_rate_aux = 1200;
            break;

        default:
            baud_rate_aux = 9600;
            break;
        }
        uart_set_baudrate(UART_RF1276, baud_rate_aux);
        struct config_rf1276 config_nuevo = {.baud_rate  = config->baud_rate,
                                             .network_id = config->network_id,
                                             .node_id    = config->node_id,
                                             .freq       = config->freq,
                                             .port_check = config->port_check,
                                             .power      = config->power,
                                             .routing_time =
                                                 config->routing_time};
        ESP_LOGW(RF1276, "Cambiando a %d", baud_rate_aux);
        info = write_config_esp_rf1276(&config_nuevo);
        return info;
    }

    info = check_write_config_rf1276(&trama);

    if (config->baud_rate != baud_rate_aux) {
        ESP_LOGI(RF1276, "Ajustando BaudRate para Radio de %d a %d",
                 baud_rate_aux, config->baud_rate);
        uart_set_baudrate(UART_RF1276, config->baud_rate);
    }

    for (int i = 0; i < 18; i++)
        printf("received trama  %02x\n ", trama.trama_rx[i]);
    if (info != 0) {
        ESP_LOGI(RF1276, "Fallo la escritura de parametros");
        return info;
    }

    ESP_LOGI(RF1276,
             "Configurado, Freq %.2fMHz, Baud Rate %d, Network ID %d, Node ID "
             "%d, Routing time %dh, Power Transmitin %d",
             config->freq, config->baud_rate, config->network_id,
             config->node_id, config->routing_time, config->power);

    xEventGroupWaitBits(eventos_rf1276, WRITE_FLAG, pdTRUE, pdFALSE,
                        portMAX_DELAY);

    return info;
}

int read_config_esp_rf1276(struct config_rf1276 *result_read_config) {

    struct trama_read_config trama;
    int info;

    read_config_rf1276(&trama);

    uart_write_bytes(UART_RF1276, (const char *)trama.trama_tx,
                     sizeof(trama.trama_tx));

    xQueueReceive(cola_config, &(trama.trama_rx), portMAX_DELAY);

    info = check_read_config_rf1276(&trama);

    if (info != 0) {
        ESP_LOGI(RF1276, "Fallo la lectura de parametros");
        return info;
    }

    *result_read_config = trama.result_read_config;

    ESP_LOGI(RF1276,
             "BaudRate %d, Freq %.2f MHz,Network ID %d, Node ID %d, Routing "
             "Time %dh, Power Transmiting %d, Port Check %s",
             result_read_config->baud_rate, result_read_config->freq,
             result_read_config->network_id, result_read_config->node_id,
             result_read_config->routing_time + 1, result_read_config->power,
             (result_read_config->port_check == 0)
                 ? "NO"
                 : (result_read_config->port_check == 1) ? "ODD" : "EVEN");

    return info;
}

int send_data_esp_rf1276(struct send_data_struct *data) {

    int info;
    struct trama_send_data trama;

    info = send_data_rf1276(data, &trama);

    uart_write_bytes(UART_RF1276, (const char *)trama.trama_tx,
                     trama.trama_tx[3] + 5);
    for (int i = 0; i < trama.trama_tx[3] + 5; i++)
        printf("t[%d]: %02x\n", i, trama.trama_tx[i]);
    xQueueReceive(cola_config, &(trama.trama_rx), portMAX_DELAY);

    info = check_send_data_rf1276(&trama);

    if (info != 0) {

        switch (info) {

        case 0xe1:

            ESP_LOGW(RF1276, "Error CRC");
            break;

        case 0xe4:
            ESP_LOGW(RF1276, "Falla de seguridad");
            break;

        case 0xe5:
            ESP_LOGW(RF1276, "Sobre escritura de MAC");
            break;

        case 0xe6:
            ESP_LOGW(RF1276, "Parametros invalidos");
            break;

        case 0xe7:
            ESP_LOGW(RF1276, "No ACK para el siguiente salto");
            break;

        case 0xea:
            ESP_LOGW(RF1276, "Red ocupada");
            break;

        case 0xc1:
            ESP_LOGW(RF1276, "Red ID invalida");
            break;

        case 0xc2:
            ESP_LOGW(RF1276, "Peticion invalida");
            break;

        case 0xc7:
            ESP_LOGW(RF1276, "No router");
            break;

        case 0xd1:
            ESP_LOGW(RF1276, "Buffer full");
            break;

        case 0xd2:
            ESP_LOGW(RF1276, "No ACK en la capa de aplicacion");
            break;

        case 0xd3:
            ESP_LOGW(RF1276, "Data de aplicacion sobre pasada");
            break;

        default:
            ESP_LOGW(RF1276, "Error no esperado");
            break;
        }
    }

    data->respond = trama.result_send_data;

    if (data->respond == 0) {
        ESP_LOGI(RF1276, "Datos enviados");
    } else {
        ESP_LOGI(RF1276, "Error en tranmision");
    }

    return info;
}

int reset_esp_rf1276(void) {

    int info;
    struct trama_reset trama;

    reset_rf1276(&trama);

    uart_write_bytes(UART_RF1276, (const char *)trama.trama_tx,
                     sizeof(trama.trama_tx));

    xQueueReceive(cola_config, &(trama.trama_rx), portMAX_DELAY);

    info = check_reset_rf1276(&trama);

    if (info != 0) {
        ESP_LOGI(RF1276, "Fallo el reinicio");
        return info;
    }

    ESP_LOGI(RF1276, "Reinicio realizado");

    return info;
}

int read_version_esp_rf1276(void) {

    int info;
    struct trama_read_version trama;

    read_version_rf1276(&trama);

    uart_write_bytes(UART_RF1276, (const char *)trama.trama_tx,
                     sizeof(trama.trama_tx));

    xQueueReceive(cola_config, &(trama.trama_rx), portMAX_DELAY);

    info = check_read_version_rf1276(&trama);

    if (info != 0) {
        ESP_LOGI(RF1276, "Fallo la lectura de version");
        return info;
    }

    ESP_LOGI(RF1276, "Lectura de Version realizada");

    return info;
}

int read_routing_path_esp_rf1276(struct repeaters *repetidores) {

    int info;
    struct trama_read_path trama;

    read_routing_path_rf1276(&trama);

    uart_write_bytes(UART_RF1276, (const char *)trama.trama_tx,
                     sizeof(trama.trama_tx));

    xQueueReceive(cola_config, &(trama.trama_rx), portMAX_DELAY);

    info = check_read_routing_path_rf1276(&trama);

    if (info != 0) {
        ESP_LOGI(RF1276, "Fallo la lectura de Path");
        return info;
    }

    *repetidores = trama.repeater;

    ESP_LOGI(RF1276, "Lectura de Path realizada");

    return info;
}

// En desarrollo
int read_routing_table_esp_rf1276(struct routing *rutas) {

    int info;
    struct trama_read_routing trama;

    read_routing_table_rf1276(&trama);

    uart_write_bytes(UART_RF1276, (const char *)trama.trama_tx,
                     sizeof(trama.trama_tx));

    xQueueReceive(cola_config, &(trama.trama_rx), portMAX_DELAY);

    info = check_read_routing_table_rf1276(&trama);

    *rutas = trama.routing;

    ESP_LOGI(RF1276, "Lectura de Routing Table realizada");

    return info;
}
