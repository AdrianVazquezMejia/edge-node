/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */
/**
 *@mainpage Edge Node
 *@author Adrian Vazquez
 *@version 4.0
 *
 *This is the main page for the project.
 *
 *1. The purpose is to document the code of the modules involved
 *2. Visit the gitlab repository to have better look of the code
 *3. Link https://gitlab.com/electr-nica/desarrollohardware/edge-node
 */
#include "CRC.h"
#include "config_rtu.h"
#include "cypher.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rf1276.h"
#include "esp_task_wdt.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "global_variables.h"
#include "led.h"
#include "lora_mesh.h"
#include "modbus_master.h"
#include "modbus_slave.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ota_update.h"
#include "string.h"
#include <stdio.h>

#define PULSES_KW         225
#define LIMIT_ERROR_COUNT 10

#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024

#define TIME_SCAN  2000
#define MAX_SLAVES 255

#define TWDT_TIMEOUT_S 20
#define TWDT_RESET     5000
#define MODBUS_TIMEOUT 100 // in ticks == 1 s

#define BUF_LORA_SIZE 5
#ifdef CONFIG_PRODUCTION
#define PULSE_GPIO 35
#else
#define PULSE_GPIO 0
#endif

#define CLOSE_RELAY 18
#define OPEN_RELAY  23
#define PULSE_TIME  1000 // ms
static char *TAG      = "INFO";
static char *TAG_UART = "MODBUS";
static char *TAG_LORA = "LORA";

#define CHECK_ERROR_CODE(returned, expected)                                   \
    ({                                                                         \
        if (returned != expected) {                                            \
            ESP_LOGE(TAG, "TWDT ERROR\n");                                     \
            abort();                                                           \
        }                                                                      \
    })

static uint16_t *modbus_registers[4];
static uint16_t inputRegister[512] = {0};

nvs_address_t pulse_address;
QueueHandle_t lora_queue;

// key length 32 bytes for 256 bit encrypting, it can be 16 or 24 bytes for 128
// and 192 bits encrypting mode

unsigned char key[] = {0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                       0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

void task_pulse(void *arg) {

    ESP_LOGI(TAG, "Pulse counter task started");
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    uint32_t pulses = 0;
    esp_err_t err;
    pulse_isr_init(PULSE_GPIO);
    pulse_address.partition = 0;
    smph_pulse_handler      = xSemaphoreCreateBinary();
    err                     = get_initial_pulse(&pulses, &pulse_address);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FLASH ERROR");
        vTaskDelete(NULL);
    }
    register_save(pulses, inputRegister);
    while (1) {
        if (xSemaphoreTake(smph_pulse_handler, pdMS_TO_TICKS(TWDT_RESET)) ==
            pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(10));
            if (gpio_get_level(PULSE_GPIO) == 1) {
                led_blink();
                pulses++;
                err = put_nvs(pulses, &pulse_address);
                if (err != ESP_OK)
                    ESP_LOGE(TAG, "FLASH ERROR");
                register_save(pulses, inputRegister);
                ESP_LOGI(TAG, "Pulse number %d", pulses);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
    }
}
void task_modbus_slave(void *arg) {

    ESP_LOGI(TAG, "Slave task started");
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    QueueHandle_t uart_queue;
    uart_event_t event;
    uint8_t *received_buffer = (uint8_t *)malloc(RX_BUF_SIZE);
    mb_response_t modbus_response;
    modbus_registers[1] = &inputRegister[0];
    uart_init(&uart_queue);
    uint error_count = 0;
    while (1) {
        if (xQueueReceive(uart_queue, (void *)&event,
                          pdMS_TO_TICKS(TWDT_RESET)) == pdTRUE) {
            gpio_set_level(OPEN_RELAY, 0);
            gpio_set_level(CLOSE_RELAY, 0);
            if (error_count == LIMIT_ERROR_COUNT)
                esp_restart();
            bzero(received_buffer, RX_BUF_SIZE);
            switch (event.type) {
            case UART_DATA:
                if (uart_read_bytes(UART_NUM_1, received_buffer, event.size,
                                    portMAX_DELAY) == ESP_FAIL) {
                    ESP_LOGE(TAG_UART, "Error while reading UART data");
                    break;
                }
                ESP_LOGI(TAG_UART, "Received data is:");
                ESP_LOG_BUFFER_HEX(TAG_UART, received_buffer, event.size);

                if (CRC16(received_buffer, event.size) == 0) {
                    if (received_buffer[0] == NODE_ID) {
                        error_count = 0;
                        led_blink();
                        modbus_slave_functions(&modbus_response,
                                               received_buffer, event.size,
                                               modbus_registers);
                        if (uart_write_bytes(
                                UART_NUM_1, (const char *)modbus_response.frame,
                                modbus_response.len) == ESP_FAIL) {
                            ESP_LOGE(TAG, "Error writig UART data");
                        }
                        ESP_LOGI(TAG_UART, "Response sent is:");
                        ESP_LOG_BUFFER_HEX(TAG_UART, modbus_response.frame,
                                           modbus_response.len);
                    }
                } else {
                    error_count = 0;
                    ESP_LOGE(TAG_UART, " CRC ERROR: %d",
                             CRC16(received_buffer, event.size));
                    uart_flush(UART_NUM_1);
                }
                break;
            case UART_FIFO_OVF:
                error_count = 0;
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart_queue);
                break;
            case UART_BUFFER_FULL:
                error_count = 0;
                ESP_LOGI(TAG, "ring buffer full");
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart_queue);
                break;
            case UART_FRAME_ERR:
                error_count = 0;
                ESP_LOGI(TAG, "uart frame error");
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart_queue);
                uart_init(&uart_queue);
                break;
            default:
                error_count = 0;
                ESP_LOGE(TAG_UART, "UART event %d", event.type);
            }
        }
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
    }
    free(received_buffer);
    received_buffer = NULL;
}

void task_modbus_master(void *arg) {
    ESP_LOGI(TAG, "Modbus Master Task initialized");
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    QueueHandle_t uart_queue;
    uart_event_t event;
    uint8_t *slave_response = (uint8_t *)malloc(RX_BUF_SIZE);
    modbus_registers[1]     = &inputRegister[0];
    uart_init(&uart_queue);
    uint16_t quantity = 2;
    bool slaves[MAX_SLAVES + 1];
    init_slaves(slaves);
    uint8_t curr_slave = 1;
    if (SLAVES == 0) {
        CHECK_ERROR_CODE(esp_task_wdt_delete(NULL), ESP_OK);
        vTaskDelete(NULL);
    }
    while (1) {
        read_input_register(curr_slave, (uint16_t)curr_slave, quantity);
        if (modbus_coils[0]) {
            ESP_LOGE(TAG, "Write coils");
        }
        if (xQueuePeek(uart_queue, (void *)&event, (portTickType)10)) {
            if (event.type == UART_BREAK) {
                ESP_LOGI(TAG, "Cleaned");
                xQueueReset(uart_queue);
            }
        }

        if (xQueueReceive(uart_queue, (void *)&event,
                          (portTickType)MODBUS_TIMEOUT)) {
            switch (event.type) {
            case UART_DATA:
                if (uart_read_bytes(UART_NUM_1, slave_response, event.size,
                                    (portTickType)MODBUS_TIMEOUT) == ESP_FAIL) {
                    ESP_LOGE(TAG_UART, "Error while reading UART data");
                    break;
                }
                ESP_LOGI(TAG_UART, "Response received is:");
                ESP_LOG_BUFFER_HEX(TAG_UART, slave_response, event.size);
                if (CRC16(slave_response, event.size) == 0) {
                    led_blink();
                    ESP_LOGI(TAG, "Modbus frame verified");
                    if (check_exceptions(slave_response) == ESP_FAIL)
                        break;
                    save_register(slave_response, event.size, modbus_registers);
                } else {
                    ESP_LOGI(TAG, "Frame not verified CRC : %d",
                             CRC16(slave_response, event.size));
                    uart_flush(UART_NUM_1);
                }
                break;
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart_queue);
                break;
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart_queue);
                uart_init(&uart_queue);
                break;
            default:
                ESP_LOGE(TAG, "uart event type: %d", event.type);
                break;
            }
        } else {
            ESP_LOGI(TAG, "Timeout");
            vTaskDelay(TIME_SCAN / portTICK_PERIOD_MS);
        }
        curr_slave++;
        if (curr_slave > SLAVES)
            curr_slave = 1;
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
    }
}

static void task_lora(void *arg) {
    ESP_LOGI(TAG, "LoRa Task initialized");
    lora_mesh_t *loraFrame = (lora_mesh_t *)malloc(sizeof(lora_mesh_t));
    mb_response_t modbus_response;
    mb_response_t auxFrame;
    modbus_registers[1] = &inputRegister[0];
#ifdef CONFIG_CIPHER
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 256);
#endif
#ifdef CONFIG_MASTER_MODBUS
    vTaskDelay(pdMS_TO_TICKS((SLAVES + 2) * TIME_SCAN));
#endif
    ESP_LOGI(TAG_LORA, "LoRa Task available");
    while (1) {
        if (xQueueReceive(lora_queue, loraFrame, portMAX_DELAY)) {
            gpio_set_level(OPEN_RELAY, 0);
            gpio_set_level(CLOSE_RELAY, 0);
            switch (loraFrame->header_.frame_type_) {
            case INTERNAL_USE:
                switch (loraFrame->header_.command_type_) {
                case ACK_RESET_MODEM:
                    if (loraFrame->load_.local_resp_.result == 0x00)
                        ESP_LOGI(TAG_LORA, "Module Successfully Reset");
                    else {
                        ESP_LOGE(TAG_LORA, "Error in RESET");
                    }
                    break;
                }
                break;
            case APPLICATION_DATA:
                switch (loraFrame->header_.command_type_) {
                case ACK_SEND:
                    if (loraFrame->header_.load_len_ == 0x01) {
                        ESP_LOGE(TAG_LORA, "Error in sending checksum");
                    } else if (loraFrame->header_.load_len_ == 0x03) {
                        uint8_t error_code =
                            loraFrame->load_.local_resp_.result;
                        if (error_code == 0) {
                            ESP_LOGI(TAG_LORA, "SENT SUCCESS");
                        } else {
                            ESP_LOGE(TAG_LORA, "ERROR CODE %2x", error_code);
                        }

                        if (error_code == 0xc1 || error_code == 0xc7 ||
                            error_code == 0xea || error_code == 0xe7) {
                            lora_reset_modem();
                            ESP_LOGI(TAG_LORA, "RESETTING MODEM");
                        }
                    }
                    ESP_LOGI(TAG_LORA, "Ack from dest node");
                    break;

                case RECV_PACKAGE:

                    ESP_LOGI(TAG_LORA, "Data package received");

                    memcpy(auxFrame.frame, loraFrame->load_.recv_load_.data_,
                           loraFrame->load_.recv_load_.data_len_);
                    auxFrame.len = loraFrame->load_.recv_load_.data_len_;

#ifdef CONFIG_CIPHER
                    if (cfb8decrypt(loraFrame->load_.recv_load_.data_,
                                    loraFrame->load_.recv_load_.data_len_,
                                    auxFrame.frame) != ESP_OK) {
                        ESP_LOGE(TAG_LORA, "Decryption Error");
                        break;
                    }
#endif
                    modbus_slave_functions(&modbus_response, auxFrame.frame,
                                           auxFrame.len, modbus_registers);
                    auxFrame = modbus_response;
#ifdef CONFIG_CIPHER
                    bzero(auxFrame.frame, auxFrame.len);
                    if (cfb8encrypt(modbus_response.frame, auxFrame.len,
                                    auxFrame.frame) != ESP_OK) {
                        ESP_LOGE(TAG_LORA, "Encryption Error");
                        break;
                    }
#endif
                    prepare_to_send(loraFrame, &auxFrame);
                    if (lora_send(loraFrame) == ESP_FAIL) {
                        ESP_LOGW(TAG_LORA, "Fail to send");
                    }
                    ESP_LOGI(TAG_LORA, "Answer Sent");
                    break;
                }
            }
        }
        vTaskDelay(10);
    }
    free(loraFrame);
}
#ifdef CONFIG_LORA
static esp_err_t init_lora(void) {
    esp_err_t err;

    lora_queue = xQueueCreate(BUF_LORA_SIZE, sizeof(lora_mesh_t));
#ifdef CONFIG_PRODUCTION
    int UART_TX = 13;
#else
    int UART_TX = 14;
#endif
    int UART_RX = 15;

    uart_lora_t configUART = {.uart_tx   = UART_TX,
                              .uart_rx   = UART_RX,
                              .baud_rate = 9600,
                              .uart_num  = UART_NUM_2};

    config_rf1276_t config_mesh = {.baud_rate    = 0x03,
                                   .network_id   = 1,
                                   .node_id      = NODE_ID,
                                   .power        = 7,
                                   .routing_time = 1,
                                   .freq         = 433.0,
                                   .port_check   = 0};

    err = init_lora_uart(&configUART);
    if (err == ESP_FAIL) {
        ESP_LOGE(TAG_LORA, "UART [%d] fail", configUART.uart_num);
        return err;
    }
    ESP_LOGI(TAG_LORA, "UART [%d] Parameters Set", UART_NUM_2);

    err = init_lora_mesh(&config_mesh, &lora_queue, UART_NUM_2);
    if (err == ESP_FAIL) {
        ESP_LOGE(TAG_LORA, "Lora Mesh set up fail");
        return err;
    }
    ESP_LOGI(TAG_LORA, "Lora Init Success");

    return err;
}
#endif
void app_main() {
    ESP_LOGI(TAG, "MCU initialized, fimware version 1.0.13122020a");
    ESP_LOGI(TAG, "Init Watchdog");
    CHECK_ERROR_CODE(esp_task_wdt_init(TWDT_TIMEOUT_S, true), ESP_OK);
    led_startup();
    check_rtu_config();

#ifdef CONFIG_PULSE_PERIPHERAL
    ESP_LOGI(TAG, "Start peripheral");
    xTaskCreatePinnedToCore(task_pulse, "task_pulse", 1024 * 2, NULL, 10, NULL,
                            0);
#endif

#ifdef CONFIG_SLAVE_MODBUS
#ifndef CONFIG_MASTER_MODBUS
    ESP_LOGI(TAG, "Start Modbus slave task");
    xTaskCreatePinnedToCore(task_modbus_slave, "task_modbus_slave", 2048 * 2,
                            NULL, 10, NULL, 1);
#endif
#endif

#ifdef CONFIG_MASTER_MODBUS
    ESP_LOGI(TAG, "Start Modbus master task");
    xTaskCreatePinnedToCore(task_modbus_master, "task_modbus_master", 2048 * 2,
                            NULL, 10, NULL, 1);
#endif
#ifdef CONFIG_LORA
    ESP_LOGI(TAG, "Start LoRa task");
    ESP_ERROR_CHECK(init_lora());
    xTaskCreatePinnedToCore(task_lora, "task_lora", 2048 * 2, NULL, 10, NULL,
                            1);
#endif

#ifdef CONFIG_OTA
    ESP_LOGI(TAG, "Start OTA task");
    xTaskCreatePinnedToCore(task_ota, "task_ota", 2048 * 2, NULL, 10, NULL, 1);
#endif
}
