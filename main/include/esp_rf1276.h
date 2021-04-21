#ifndef ESP_RF1276
#define ESP_RF1276

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "rf1276.h"
#include <esp_err.h>
#include <string.h>

EventGroupHandle_t eventos_rf1276;

#define BUF_SIZE    (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define UART_RF1276 UART_NUM_2

/**
 * @brief Estructura de configuración del UART
 */

typedef struct uart_lora {
    uint8_t uart_tx;
    uint8_t uart_rx;
    int baud_rate;
    uart_port_t uart_num;
} uart_lora_t;

/**
 * @brief Configura los parámetros del radio RF1276MN
 *
 * @param config - En esta estructura se pasan los parámetros a configurar.
 *
 * @return
 * 		- 0, Se ha configurado el radio.
 *
 * @note Una vez que se ejecuta la función internamente se espera hasta que el
 *radio se reinicie y envie por serial la versión del firmware del radio.
 *
 * Example usage:
 *
 * @code{c}
 * struct config_rf1276 config = {
 *		.baud_rate = 9600,
 *		.network_id = 1,
 *		.node_id = 1,
 *		.power = 7,
 *		.routing_time = 1,
 *		.freq = 433.0,
 *		.port_check =0
 *	};
 *
 *	write_config_esp_rf1276(&config);
 */

int write_config_esp_rf1276(struct config_rf1276 *config);

/**
 * @brief Lectura de los parámetros de configuración del radio RF1276MN
 *
 * @param result_read_config - En esta estructura se obtienen los parámetros de
 *configuración.
 *
 * @return
 * 		- 0, Se ha configurado el radio.
 *
 * Example usage:
 *
 * @code{c}
 * struct config_rf1276 config;
 *
 *	read_config_esp_rf1276(&config);
 */

int read_config_esp_rf1276(struct config_rf1276 *result_read_config);

/**
 * @brief Reinicio del radio RF1276MN
 *
 * @return
 * 		- 0, Se ha reiniciado el radio.
 *
 * Example usage:
 *
 * @code{c}
 *
 *	reset_esp_rf1276(&config);
 */

int reset_esp_rf1276(void);

/**
 * @brief Envío de datos a través del radio RF1276MN.
 *
 * @param data - Estructura para envío de datos.
 *
 * @return
 * 		- 0x00, Se ha enviado el paquete.
 * 		- 0xe1, Error CRC.
 * 		- 0xe4, Falla de seguridad.
 * 		- 0xe5, Sobre escritura de MAC.
 * 		- 0xe6, Parámetros invalidos.
 * 		- 0xe7, No hubo ACK para el siguiente salto.
 * 		- 0xea, Red ocupada.
 * 		- 0xc1, Red ID invalida.
 * 		- 0xc2, Petición invalida.
 * 		- 0xc7, No router.
 * 		- 0xd1, Buffer full.
 * 		- 0xd2, No ACK en la capa de aplicación.
 * 		- 0xd3, Data de aplicación sobre pasada.
 *
 * Example usage:
 *
 * @code{c}
 *
 *	struct send_data_struct data = {
 *   .node_id = 1,
 *   .power = 7,
 *   .data = {0x10,0x20,0x30},
 *   .tamano = 3,
 *   .routing_type = 1
 * };
 *
 * send_data_esp_rf1276(&data);
 */

int send_data_esp_rf1276(struct send_data_struct *data);

/**
 * @brief Lectura de versión del radio RF1276MN
 *
 * @return
 * 		- 0, Se ha hecho la lectura de versión.
 *
 * 	@note La versión se ve por serial.
 *
 * Example usage:
 *
 * @code{c}
 *
 *	read_version_esp_rf1276();
 */

int read_version_esp_rf1276(void);

/**
 * @brief Lectura de Routing Path del radio RF1276MN.
 *
 * @param repetidores - Estructura para la recepción de los nodos en el path y
 *la intensidad de señal de cada uno.
 *
 * @return
 * 		- 0, Se han obtenido el path.
 *
 * Example usage:
 *
 * @code{c}
 *
 * struct repeaters repetidores;
 *
 *	read_routing_path_esp_rf1276(&repetidores);
 */

int read_routing_path_esp_rf1276(struct repeaters *repetidores);

/**
 * @brief Lectura de Routing Table del radio RF1276MN.
 *
 * @note En Desarrollo...
 */

int read_routing_table_esp_rf1276(struct routing *rutas);

/**
 * @brief Inicio del radio RF1276MN.
 *
 * @param config_uart - Estructura para la configuración del uart.
 * @param config_rf1276 - Estructura para la configuración del radio RF1276MN.
 * @param cola - Cola para recepción asincrona de datos de la red.
 *
 * @return
 * 		- ESP_OK, Se han inicializado el radio.
 *
 * Example usage:
 *
 * @code{c}
 *
 * QueueHandle cola;
 *
 * void app_main(){
 *     struct config_uart config = {
 *         .uart_tx = 4,
 *         .uart_rx = 5,
 *         .baud_rate =  9600
 *     };
 *
 *     struct config_rf1276 config_mesh = {
 *         .baud_rate = 9600,
 *         .network_id = 1,
 *         .node_id = 1,
 *        .power = 7,
 *         .routing_time = 1,
 *         .freq = 433.0,
 *         .port_check =0
 *     };
 *
 *    cola = xQueueCreate(1,128);
 *
 *    start_lora_mesh(config_uart, config_mesh, &cola);
 * }
 */

esp_err_t start_lora_mesh(struct uart_lora config_uart,
                          struct config_rf1276 config_mesh,
                          QueueHandle_t *cola);
esp_err_t init_lora_uart(uart_lora_t *uartParameters);

esp_err_t init_lora_mesh(config_rf1276_t *loraParameters,
                         QueueHandle_t *loraQueue, uart_port_t uart_num);
typedef union {
    struct {
        uint8_t low_byte_;
        uint8_t high_byte_;
    };
    uint16_t word_;
} word_t;

typedef union {
    struct {
        word_t low_word_;
        word_t high_word_;
    };
    uint32_t doubleword_;
} doubleword_t;

#endif
