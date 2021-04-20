#include "include/esp_rf1276.h"

static const char *RF1276 = "ESP_RF1276";

QueueHandle_t *cola_data;
QueueHandle_t cola_config;
QueueHandle_t cola_general;
QueueHandle_t cola_tamano;
static QueueHandle_t uart0_queue;
int baud_rate_aux;

#define WRITE_FLAG 0x01

static void uart_event_task(void *pvParameters) {
    ESP_LOGI(RF1276, "Configurado Eventos UART%d", UART_RF1276);

    uart_event_t event;
    // size_t buffered_size;
    // uint8_t buffer_din[RD_BUF_SIZE];
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
    while (1) {
        // Waiting for UART event.
        if (xQueueReceive(uart0_queue, (void *)&event,
                          (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(RF1276, "uart[%d] event:", UART_RF1276);
            switch (event.type) {
            // Event of UART receving data
            /*We'd better handler data event fast, there would be much more data
            events than other types of events. If we take too much time on data
            event, the queue might be full.*/
            case UART_DATA:
                ESP_LOGI(RF1276, "[UART DATA]: %d", event.size);
                uart_read_bytes(UART_RF1276, dtmp, event.size, portMAX_DELAY);
                uint8_t *buffer_din =
                    (uint8_t *)calloc(event.size, sizeof(uint8_t));
                for (int i = 0; i < event.size; i++) {
                    buffer_din[i] = dtmp[i];
                }
                xQueueSend(cola_tamano, &event.size, portMAX_DELAY);
                xQueueSend(cola_general, buffer_din, portMAX_DELAY);
                free(buffer_din);

                //                    uart_write_bytes(pvParameters, (const
                //                    char*) dtmp, event.size);
                break;
            // Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(RF1276, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow
                // control for your application. The ISR has already reset the
                // rx FIFO, As an example, we directly flush the rx buffer here
                // in order to read more data.
                uart_flush_input(UART_RF1276);
                xQueueReset(uart0_queue);
                break;
            // Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(RF1276, "ring buffer full");
                // If buffer full happened, you should consider encreasing your
                // buffer size As an example, we directly flush the rx buffer
                // here in order to read more data.
                uart_flush_input(UART_RF1276);
                xQueueReset(uart0_queue);
                break;
            // Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(RF1276, "uart rx break");
                break;
            // Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(RF1276, "uart parity error");
                break;

            // Others
            default:
                ESP_LOGI(RF1276, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

esp_err_t init_lora_uart(uart_lora_t *uartParameters) {

    esp_err_t err;
    int loraSerialBaudarate = uartParameters->baud_rate;
    int loraUARTTX          = uartParameters->uart_tx;
    int loraUARTRX          = uartParameters->uart_rx;
    QueueHandle_t uart2Queue;
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

    err = uart_driver_install(loraUARTNUM, BUF_SIZE, BUF_SIZE, 20, &uart2Queue,
                              0);
    if (err == ESP_FAIL) {
        return err;
    }
    ESP_LOGI(RF1276, "UART [%d] Parameters Set", loraUARTNUM);

    return err;
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
