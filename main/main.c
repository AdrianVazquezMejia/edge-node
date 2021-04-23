/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */

#include "CRC.h"
#include "config_rtu.h"
#include "cypher.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_intr_alloc.h"
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
#include "modbus_lora.h"
#include "modbus_master.h"
#include "modbus_slave.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ota_update.h"
#include "string.h"
#include <stdio.h>
#define PULSES_KW 225

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

#define CHECK_ERROR_CODE(returned, expected)                                   \
    ({                                                                         \
        if (returned != expected) {                                            \
            printf("TWDT ERROR\n");                                            \
            abort();                                                           \
        }                                                                      \
    })
static char *TAG      = "INFO";
static char *TAG_UART = "MODBUS";
static char *TAG_LORA = "LORA";
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
    uint8_t *dtmp       = (uint8_t *)malloc(RX_BUF_SIZE);
    modbus_registers[1] = &inputRegister[0];
    uart_init(&uart_queue);
    while (1) {
        if (xQueueReceive(uart_queue, (void *)&event,
                          pdMS_TO_TICKS(TWDT_RESET)) == pdTRUE) {
            bzero(dtmp, RX_BUF_SIZE);
            switch (event.type) {
            case UART_DATA:
                if (uart_read_bytes(UART_NUM_1, dtmp, event.size,
                                    portMAX_DELAY) == ESP_FAIL) {
                    ESP_LOGE(TAG_UART, "Error while reading UART data");
                    break;
                }
                ESP_LOGI(TAG_UART, "Rec31eived data is:");
                for (int i = 0; i < event.size; i++) {
                    printf("%x ", dtmp[i]);
                }
                printf("\n");
                if (CRC16(dtmp, event.size) == 0) {
                    ESP_LOGI(TAG_UART, "Modbus frame verified");
                    if (dtmp[0] == NODE_ID) {
                        led_blink();
                        ESP_LOGI(TAG, "Frame to this slave");
                        modbus_slave_functions(dtmp, event.size,
                                               modbus_registers);
                    }
                } else {
                    ESP_LOGE(TAG_UART, " CRC ERROR: %d",
                             CRC16(dtmp, event.size));
                    crc_error_response(dtmp);
                    uart_flush(UART_NUM_1);
                }
                break;
            default:
                ESP_LOGE(TAG_UART, "UART event %d", event.type);
            }
        }
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
    }
    free(dtmp);
    dtmp = NULL;
}

void task_modbus_master(void *arg) {
    ESP_LOGI(TAG, "Modbus Master Task initialized");
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    QueueHandle_t uart_queue;
    uart_event_t event;
    uint8_t *dtmp       = (uint8_t *)malloc(RX_BUF_SIZE);
    modbus_registers[1] = &inputRegister[0];
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
                if (uart_read_bytes(UART_NUM_1, dtmp, event.size,
                                    (portTickType)MODBUS_TIMEOUT) == ESP_FAIL) {
                    ESP_LOGE(TAG_UART, "Error while reading UART data");
                    break;
                }
                ESP_LOGI(TAG, "Received data is:");
                for (int i = 0; i < event.size; i++) {
                    printf("%x ", dtmp[i]);
                }
                printf("\n");
                if (CRC16(dtmp, event.size) == 0) {
                    led_blink();
                    ESP_LOGI(TAG, "Modbus frame verified");
                    check_exceptions(dtmp);
                    save_register(dtmp, event.size, modbus_registers);
                } else {
                    ESP_LOGI(TAG, "Frame not verified CRC : %d",
                             CRC16(dtmp, event.size));
                    uart_flush(UART_NUM_1);
                }
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

    while (1) {
        if (xQueueReceive(lora_queue, loraFrame, portMAX_DELAY)) {

            switch (loraFrame->header_.frame_type_) {
            case INTERNAL_USE:
                /// TODO
                break;
            case APPLICATION_DATA:
                switch (loraFrame->header_.command_type_) {
                case ACK_SEND:
                    if (loraFrame->header_.load_len_ == 0x01) {
                        ESP_LOGE(TAG_LORA,
                                 "Lora Can not verify the CRC to send");
                    } else if (loraFrame->header_.load_len_ == 0x03 &&
                               loraFrame->load_.local_resp_.result != 0) {
                        ESP_LOGE(TAG_LORA, "LORA ERROR CODE %2x",
                                 loraFrame->load_.local_resp_.result);
                    }
                    ESP_LOGI(TAG_LORA, "Ack from dest node");
                    break;
                case RECV_PACKAGE:

                    ESP_LOGI(TAG_LORA, "Data package received");

                    memcpy(auxFrame.frame, loraFrame->load_.recv_load_.data_,
                           loraFrame->load_.recv_load_.data_len_);
                    auxFrame.len = loraFrame->load_.recv_load_.data_len_;

#ifdef CONFIG_CIPHER
                    cfb8decrypt(loraFrame->load_.recv_load_.data_,
                                loraFrame->load_.recv_load_.data_len_,
                                auxFrame.frame);
#endif
                    ESP_LOGI(TAG_UART, "Unencrypted R data is:");

                    modbus_response = modbus_lora_functions(
                        auxFrame.frame, auxFrame.len, modbus_registers);
                    auxFrame = modbus_response;
#ifdef CONFIG_CIPHER
                    bzero(auxFrame.frame, auxFrame.len);
                    cfb8encrypt(modbus_response.frame, auxFrame.len,
                                auxFrame.frame);
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
static esp_err_t init_lora(void) {
    esp_err_t err;

    lora_queue = xQueueCreate(BUF_LORA_SIZE, sizeof(lora_mesh_t));
#ifdef CONFIG_PRODUCTION
    int UART_TX = 13;
    int UART_RX = 15;
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
