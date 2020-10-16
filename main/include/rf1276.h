#ifndef RF1276_MODULE
#define RF1276_MODULE

#include <stdbool.h>
#include <stdint.h>

#ifdef DEBUG_RF1276
#include <stdio.h>
#include <string.h>
#endif

typedef union {
    struct {
        uint8_t byte_low;
        uint8_t byte_high;
    };
    uint16_t valor;
} ident_t;

typedef union {
    struct {
        ident_t freq_low;
        ident_t freq_high;
    };
    uint32_t valor;
} freq_t;

struct config_rf1276 {
    uint32_t baud_rate;
    uint16_t network_id;
    uint16_t node_id;
    uint8_t power;
    uint8_t routing_time;
    float freq;
    uint8_t port_check;
};

struct send_data_struct {
    uint16_t node_id;
    uint8_t power;
    uint8_t data[117];
    uint8_t tamano;
    uint8_t routing_type;
    uint8_t respond;
    ident_t repeaters[8];
    uint8_t saltos;
};

struct trama_write_config {
    uint8_t trama_tx[18];
    uint8_t trama_rx[18];
};

struct repeaters {
    ident_t repeaterID[9];
    uint8_t signal[9];
};

struct trama_read_path {
    uint8_t trama_tx[5];
    uint8_t trama_rx[32];
    struct repeaters repeater;
};

struct node_rout {
    ident_t jump[8];
    ident_t destiny_node;
    uint8_t hop_counts;
    uint8_t rout_information[8];
    freq_t time[8];
};

struct routing {
    uint8_t paths_count;
    struct {
        ident_t jump[8];
        ident_t target_node;
        uint8_t hop_counts;
        uint8_t rout_information[8];
        freq_t time[8];
    } paths[60];
};

struct trama_read_routing {
    uint8_t trama_tx[5];
    uint8_t trama_rx[550];
    struct routing routing;
};

struct trama_read_config {
    uint8_t trama_tx[18];
    uint8_t trama_rx[18];
    struct config_rf1276 result_read_config;
};

struct trama_read_version {
    uint8_t trama_tx[5];
    uint8_t trama_rx[39];
    float version;
};

struct trama_reset {
    uint8_t trama_tx[5];
    uint8_t trama_rx[6];
};

struct trama_send_data {
    uint8_t trama_tx[128];
    uint8_t trama_rx[8];
    uint8_t result_send_data;
};

struct trama_receive {
    uint8_t trama_rx[126];
    struct {
        uint8_t data[117];
        uint8_t data_length;
        uint8_t signal;
        ident_t source_node;
    } respond;
};

/************* Ejemplo Write Config Frame **********/

//	struct trama_write_config trama;
//
//	struct config_rf1276 config_rf1276 = {
//		.baud_rate = 115200,
//		.network_id = 1,
//		.node_id = 1,
//		.power = 7,
//		.routing_time = 1,
//		.freq = 433.0,
//		.port_check = 0
//	};
//
//	write_config_rf1276(&config_rf1276, &trama);
//

int write_config_rf1276(struct config_rf1276 *config,
                        struct trama_write_config *trama);

/************* Ejemplo CheckWrite Config Frame **********/

//	struct trama_write_config trama = {
//		.trama_tx =
//{0x01,0x00,0x01,0x0D,0xA5,0xA5,0x6c,0x40,0x12,0x07,0x17,0x00,0x01,0x00,0x01,0x03,0x00,0x20},
//		.trama_rx =
//{0x01,0x00,0x81,0x0D,0xA5,0xA5,0x6c,0x40,0x12,0x07,0x17,0x00,0x01,0x00,0x01,0x03,0x00,0xa0}
//	};
//
//	check_write_config_rf1276(&trama);

int check_write_config_rf1276(struct trama_write_config *trama);

/************ Ejemplo Send Data Frame **************/

//	struct trama_send_data trama;
//
//	struct send_data_struct datos = {
//			.node_id = 1,
//			.power = 7,
//			.data = {0x10,0x20,0x30},
//			.tamano = 3,
//			.routing_type = 1
//	};
//
//	send_data_rf1276(&datos, &trama);

int send_data_rf1276(struct send_data_struct *config,
                     struct trama_send_data *trama);

/************ Ejemplo Check Send Data Frame **************/

//	struct trama_send_data trama = {
//			.trama_tx = {0x05, 0x00, 0x01, 0x09, 0x00, 0x03, 0x00,
// 0x07, 0x01, 0x03, 0x10, 0x20, 0x30, 0x0b}, 			.trama_rx
// ={0x05, 0x00, 0x81, 0x03, 0x00, 0x03, 0x00, 0x84}
//	};
//
//	check_send_data_rf1276(&trama);

int check_send_data_rf1276(struct trama_send_data *trama);

/************ Ejemplo Read Config Frame **************/

//	struct trama_read_config trama;
//
//	read_config_rf1276(&trama);
//

void read_config_rf1276(struct trama_read_config *trama);

/************ Ejemplo Check Read Frame **************/

//		struct trama_read_config trama = {
//			.trama_tx =
//{0x01,0x00,0x02,0x0d,0xa5,0xa5,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0e},
//			.trama_rx =
//{0x01,0x00,0x82,0x0d,0xa5,0xa5,0x6c,0x40,0x12,0x07,0x17,0x00,0x01,0x00,0x01,0x03,0x00,0xa3}
//		};
//
//		check_read_config_rf1276(&trama);

int check_read_config_rf1276(struct trama_read_config *trama);

/************ Ejemplo Reset Frame **************/

//	struct trama_reset trama;
//
//	reset_rf1276(&trama);
//

void reset_rf1276(struct trama_reset *trama);

/************ Ejemplo Check Reset Frame **************/

//	struct trama_reset trama = {
//				.trama_tx = {0x01,0x00,0x07,0x00,0x06},
//				.trama_rx = {0x01,0x00,0x87,0x01,0x00,0x87}
//		};
//
//	check_reset_rf1276(&trama);

int check_reset_rf1276(struct trama_reset *trama);

/************ Ejemplo Read Version Frame **************/

//	struct trama_read_version trama;
//
//	read_version_rf1276(&trama);
//

void read_version_rf1276(struct trama_read_version *trama);

/************ Ejemplo Check Read Version Frame **************/

//	struct trama_read_version trama ={
//			.trama_tx = {0x01,0x00,0x06,0x00,0x07},
//			.trama_rx = {0x01,0x00,0x86,0x21,0x39,0x36,0x00,0x00,
//				0x20,0x20,0x4E,0x20,0x38,0x20,0x31,0x20,0x59,0x4C,0x5F,
//				0x38,0x30,0x30,0x4D,0x4E,0x5F,0x31,0x30,0x30,0x4D,0x57,
//				0X20,0x56,0x33,0x2E,0x38,0x0D,0x0A,0x9F}
//	};
//
//	check_read_version_rf1276(&trama);

int check_read_version_rf1276(struct trama_read_version *trama);

/************ Ejemplo Read Routing Table Frame **************/

//	struct trama_read_routing trama;
//
//	read_routing_table_rf1276(&trama);
//

void read_routing_table_rf1276(struct trama_read_routing *trama);

/************ Ejemplo Check Read Routing Table Frame **************/

//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "include/rf1276.h"
// void prueba_routing(void *param){
//
//	int alo;
//
//	uint8_t aux_trama_rx[26] =
//{0x01,0x00,0x83,0x3C,0x00,0x01,0x02,0x1D,0x00,0x03,0x00,0x00,0x02,0x40,0x51,0x01,0x00,
//				0x00,0x03,0x00,0x00,0x03,0x42,0x51,0x01,0x00};
//
//	struct trama_read_routing trama;
//	read_routing_table_rf1276(&trama);
//
//	for(int i = 0 ; i < 550 ; i++){
//		if( i == 549){
//			trama.trama_rx[i] = 0x5c;
//			continue;
//		}
//		if(i > 25){
//			trama.trama_rx[i] = 0xff;
//			continue;
//		}
//		trama.trama_rx[i] = aux_trama_rx[i];
//	}
//
//	alo = check_read_routing_table_rf1276(&trama);
//
//	vTaskDelete(NULL);
//}
//
// void app_main(){
//
//	xTaskCreate(prueba_routing,"routing table", 1024*8,NULL,12,NULL);
//
//}

int check_read_routing_table_rf1276(struct trama_read_routing *trama);

/************ Ejemplo Read Routing Path Frame **************/

//	struct trama_read_path trama;
//
//	read_routing_path_rf1276(&trama);
//

void read_routing_path_rf1276(struct trama_read_path *trama);

/************ Ejemplo Read Routing Table Frame **************/

//	struct trama_read_path trama = {
//			.trama_tx = {0x01,0x00,0x08,0x00,0x09},
//			.trama_rx =
//{0x01,0x00,0x88,0x1B,0x00,0x00,0x01,0x86,0x00,0x03,
//					0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
//					0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xE9}
//	};
//
//	check_read_routing_path_rf1276(&trama);

int check_read_routing_path_rf1276(struct trama_read_path *trama);

/************ Ejemplo Receive Packet **************/

//	struct trama_receive trama = {
//			.trama_rx =
//{0x05,0x00,0x82,0x07,0x00,0x01,0x73,0x03,0x10,0x20,0x30,0xF1}
//	};
//
//	receive_packet_rf1276(&trama);

int receive_packet_rf1276(struct trama_receive *trama);

#endif
