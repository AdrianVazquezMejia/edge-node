#include "include/rf1276.h"

int write_config_rf1276(struct config_rf1276 *config,
                        struct trama_write_config *trama) {

    ident_t aux_node;
    freq_t aux_freq;
    uint8_t aux_baudRate;
    uint8_t check_sum;

    if (config->freq < 380 || config->freq > 510) {
#ifdef DEBUG_RF1276
        printf("Frecuencia fuera de rango [380 - 510] MHz\r\n");
#endif
        return -7;
    }

    aux_freq.valor = config->freq * 1000000000 / 61035;

    if (config->power < 1 || config->power > 7) {
#ifdef DEBUG_RF1276
        printf("Power fuera de rango [1-7]\r\n");
#endif
        return -10;
    }

    if (config->routing_time < 1 || config->routing_time > 24) {
#ifdef DEBUG_RF1276
        printf("Routing Time fuera de rango [1-24]horas\r\n");
#endif
        return -11;
    }

    if (config->network_id < 1) {
#ifdef DEBUG_RF1276
        printf("Network ID fuera de rango [1-65535]\r\n");
#endif
        return -12;
    }

    if (config->node_id < 1) {
#ifdef DEBUG_RF1276
        printf("Node ID fuera de rango [1-65535]\r\n");
#endif
        return -14;
    }

    switch (config->baud_rate) {
    case 1200:
        aux_baudRate = 0x00;
        break;

    case 2400:
        aux_baudRate = 0x01;
        break;

    case 4800:
        aux_baudRate = 0x02;
        break;

    case 9600:
        aux_baudRate = 0x03;
        break;

    case 19200:
        aux_baudRate = 0x04;
        break;

    case 38400:
        aux_baudRate = 0x05;
        break;

    case 57600:
        aux_baudRate = 0x06;
        break;

    case 115200:
        aux_baudRate = 0x07;
        break;

    default:
#ifdef DEBUG_RF1276
        printf("Baud Rate invalido [1200, 2400, 4800, 9600, 19200, 57600, "
               "115200]\r\n");
#endif
        return -15;
    }

    if (config->port_check > 3) {
#ifdef DEBUG_RF1276
        printf("Port Check fuera de rango [0-3]\r\n");
#endif
        return -16;
    }

    trama->trama_tx[0] = 0x01;
    trama->trama_tx[1] = 0x00;
    trama->trama_tx[2] = 0x01;
    trama->trama_tx[3] = 0x0d;
    trama->trama_tx[4] = 0xa5;
    trama->trama_tx[5] = 0xa5;

    // Frecuencia 433MHz 0x6C, 0x40, 0x12
    trama->trama_tx[6] = aux_freq.freq_high.byte_low;
    trama->trama_tx[7] = aux_freq.freq_low.byte_high;
    trama->trama_tx[8] = aux_freq.freq_low.byte_low;

    trama->trama_tx[9]  = config->power;
    trama->trama_tx[10] = config->routing_time - 1;

    aux_node.valor = config->network_id;

    trama->trama_tx[11] = aux_node.byte_high;
    trama->trama_tx[12] = aux_node.byte_low;

    aux_node.valor = config->node_id;

    trama->trama_tx[13] = aux_node.byte_high;
    trama->trama_tx[14] = aux_node.byte_low;
    trama->trama_tx[15] = aux_baudRate;
    trama->trama_tx[16] = config->port_check;

    check_sum = trama->trama_tx[0];
    for (int i = 1; i < 16; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }

    trama->trama_tx[17] = check_sum;

#ifdef DEBUG_RF1276
    printf("Trama Write Config armada\r\n");
#endif

    return 0;
}

int check_write_config_rf1276(struct trama_write_config *trama) {

    freq_t aux_freq;
    uint8_t check_sum;
    ident_t aux_id;

    if (trama->trama_tx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config en byte 1, debe ser 0x01\r\n");
#endif
        return -1;
    }

    if (trama->trama_tx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config en byte 2, debe ser 0x00\r\n");
#endif
        return -2;
    }

    if (trama->trama_tx[2] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config en byte 3, debe ser 0x01\r\n");
#endif
        return -3;
    }

    if (trama->trama_tx[3] != 0x0d) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config en byte 4, debe ser 0x0d\r\n");
#endif
        return -4;
    }

    if (trama->trama_tx[4] != 0xa5) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config en byte 5, debe ser 0xa5\r\n");
#endif
        return -5;
    }

    if (trama->trama_tx[5] != 0xa5) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config en byte 6, debe ser 0xa5\r\n");
#endif
        return -6;
    }

    aux_freq.freq_high.byte_low = trama->trama_tx[6];
    aux_freq.freq_low.byte_high = trama->trama_tx[7];
    aux_freq.freq_low.byte_low  = trama->trama_tx[8];

    aux_freq.valor = aux_freq.valor * 61035.0 / 1000000000;

    if (aux_freq.valor < 380 || aux_freq.valor > 510) {
#ifdef DEBUG_RF1276
        printf("Frecuencia fuera de rango [380 - 510] MHz\r\n");
#endif
        return -7;
    }

    if (trama->trama_tx[9] < 1 || trama->trama_tx[9] > 7) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config, el Power Transmittion debe "
               "estar entre [1-7]\r\n");
#endif
        return -10;
    }

    if (trama->trama_tx[10] > 23) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config, el Routing time debe estar "
               "entre [1-24]\r\n");
#endif
        return -11;
    }

    aux_id.byte_high = trama->trama_tx[11];
    aux_id.byte_low  = trama->trama_tx[12];

    if (aux_id.valor < 1) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config, el Network ID debe estar entre "
               "[1- 65535]\r\n");
#endif
        return -12;
    }

    aux_id.byte_high = trama->trama_tx[13];
    aux_id.byte_low  = trama->trama_tx[14];

    if (aux_id.valor < 1) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config, el Node ID debe estar entre "
               "[1- 65535]\r\n");
#endif
        return -14;
    }

    if (trama->trama_tx[15] > 0x07) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config, el Baud Rate debe estar entre "
               "[1200- 115200]\r\n");
#endif
        return -16;
    }

    if (trama->trama_tx[16] > 0x03) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config, Port Check no esperado\r\n");
#endif
        return -17;
    }

    check_sum = trama->trama_tx[0];

    for (int i = 1; i < 17; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }

    if (check_sum != trama->trama_tx[17]) {
#ifdef DEBUG_RF1276
        printf("Error CRC en trama tx Write Config\r\n");
#endif
        return -18;
    }

    if (trama->trama_rx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config en byte 1, debe ser 0x01\r\n");
#endif
        return 1;
    }

    if (trama->trama_rx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config en byte 2, debe ser 0x00\r\n");
#endif
        return 2;
    }

    if (trama->trama_rx[2] != 0x81) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config en byte 3, debe ser 0x81\r\n");
#endif
        return 3;
    }

    if (!(trama->trama_rx[3] == 0x0d || trama->trama_rx[3] == 0x0c)) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config en byte 4, debe ser 0x0d para "
               "4.0 y 0x0c para 4.2 \r\n");
#endif
        return 4;
    }

    if (trama->trama_rx[4] != 0xa5) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config en byte 5, debe ser 0xa5\r\n");
#endif
        return 5;
    }

    if (trama->trama_rx[5] != 0xa5) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config en byte 6, debe ser 0xa5\r\n");
#endif
        return 6;
    }

    if (trama->trama_rx[6] != trama->trama_tx[6] ||
        trama->trama_rx[7] != trama->trama_tx[7] ||
        trama->trama_rx[8] != trama->trama_tx[8]) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config, la frecuencia no coincide\r\n");
#endif
        return 7;
    }

    if (trama->trama_rx[9] != trama->trama_tx[9]) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config, el power transmiting no "
               "coincide\r\n");
#endif
        return 10;
    }

    if (trama->trama_rx[10] != trama->trama_tx[10]) {
#ifdef DEBUG_RF1276
        printf(
            "Error en trama rx Write Config, el routing time no coincide\r\n");
#endif
        return 11;
    }

    if (trama->trama_rx[11] != trama->trama_tx[11] ||
        trama->trama_rx[12] != trama->trama_tx[12]) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config, el network id no coincide\r\n");
#endif
        return 12;
    }

    if (trama->trama_rx[13] != trama->trama_tx[13] ||
        trama->trama_rx[14] != trama->trama_tx[14]) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config, el node id no coincide\r\n");
#endif
        return 14;
    }

    if (trama->trama_rx[15] != trama->trama_tx[15]) {
#ifdef DEBUG_RF1276
        printf("Error en trama rx Write Config, el baud rate no coincide\r\n");
#endif
        return 16;
    }

    if (trama->trama_rx[16] != trama->trama_tx[16]) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Write Config, Port Check no coincide\r\n");
#endif
        return 17;
    }

    check_sum = trama->trama_rx[0];
    for (int j = 1; j < 17; j++) {
        check_sum = check_sum ^ trama->trama_rx[j];
    }
    if (check_sum != trama->trama_rx[17]) {
#ifdef DEBUG_RF1276
        printf("Error CRC en trama rx\r\n");
#endif
        return 18;
    }

#ifdef DEBUG_RF1276
    printf("Trama Write Config verificada\r\n");
#endif

    return 0;
}

int send_data_rf1276(struct send_data_struct *config,
                     struct trama_send_data *trama) {

    ident_t aux_nodo;
    uint8_t check_sum;

    if (config->tamano < 1 || config->tamano > 117) {
#ifdef DEBUG_RF1276
        printf("Payload fuera de rango [1-117]\r\n");
#endif
        return -10;
    }

    if (config->power < 1 || config->power > 7) {
#ifdef DEBUG_RF1276
        printf("Power fuera de rango [1-7]\r\n");
#endif
        return -8;
    }

    if (config->node_id < 1) {
#ifdef DEBUG_RF1276
        printf("Node ID fuera de rango [1-65535]\r\n");
#endif
        return -5;
    }

    if (config->routing_type > 3) {
#ifdef DEBUG_RF1276
        printf("Routing type fuera de rango [0-3]\r\n");
#endif
        return -9;
    }

    trama->trama_tx[0] = 0x05;
    check_sum          = trama->trama_tx[0];

    trama->trama_tx[1] = 0x00;
    trama->trama_tx[2] = 0x01;

    if (config->routing_type == 3) {
        trama->trama_tx[3] = 0x07 + config->tamano + config->saltos * 2;

        aux_nodo.valor = config->node_id;

        if (config->saltos < 1) {
#ifdef DEBUG_RF1276
            printf("Saltos debe ser mayor a 0\r\n");
#endif
            return -10;
        }

        if (config->repeaters[0].valor < 1) {
#ifdef DEBUG_RF1276
            printf(
                "Node ID Invalido por favor verifique que sea mayor a 0\r\n");
#endif
            return -11;
        }

        trama->trama_tx[4] = aux_nodo.byte_high;
        trama->trama_tx[5] = aux_nodo.byte_low;
        trama->trama_tx[6] = 0x00;
        trama->trama_tx[7] = config->power;
        trama->trama_tx[8] = config->routing_type;
        trama->trama_tx[9] = config->saltos;

        for (int i = 0; i < config->saltos; i++) {
            trama->trama_tx[i * 2 + 10] = config->repeaters[i].byte_high;
            trama->trama_tx[i * 2 + 11] = config->repeaters[i].byte_low;
        }

        trama->trama_tx[10 + config->saltos * 2] = config->tamano;

        for (int i = 0; i < config->tamano + 1; i++) {
            if (i == config->tamano) {
                for (int j = 1; j < 11 + config->tamano + config->saltos * 2;
                     j++) {
                    check_sum = check_sum ^ trama->trama_tx[j];
                }
                trama->trama_tx[11 + config->tamano + config->saltos * 2] =
                    check_sum;
                continue;
            }
            trama->trama_tx[11 + config->saltos * 2 + i] = config->data[i];
        }

    } else {
        trama->trama_tx[3] = 0x06 + config->tamano;

        aux_nodo.valor = config->node_id;

        trama->trama_tx[4] = aux_nodo.byte_high;
        trama->trama_tx[5] = aux_nodo.byte_low;
        trama->trama_tx[6] = 0x00;
        trama->trama_tx[7] = config->power;
        trama->trama_tx[8] = config->routing_type;
        trama->trama_tx[9] = config->tamano;

        for (int i = 0; i < config->tamano + 1; i++) {
            if (i == config->tamano) {
                for (int j = 1; j < config->tamano + 10; j++) {
                    check_sum = check_sum ^ trama->trama_tx[j];
                }
                trama->trama_tx[10 + i] = check_sum;
                continue;
            }
            trama->trama_tx[10 + i] = config->data[i];
        }
    }

#ifdef DEBUG_RF1276
    printf("Trama Send Data armada\r\n");
#endif

    return 0;
}

int check_send_data_rf1276(struct trama_send_data *trama) {

    uint8_t check_sum;
    check_sum = trama->trama_rx[0];

    if (trama->trama_rx[0] != 0x05) {
#ifdef DEBUG_RF1276
        printf("Trama incorrecta en byte 1\r\n");
#endif
        return 1;
    }

    if (trama->trama_rx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Trama incorrecta en byte 2\r\n");
#endif
        return 2;
    }

    if (trama->trama_rx[2] != 0x81) {
#ifdef DEBUG_RF1276
        printf("Trama incorrecta en byte 3\r\n");
#endif
        return 3;
    }

    if (trama->trama_rx[3] != 0x03) {
#ifdef DEBUG_RF1276
        printf("Trama incorrecta en byte numero 4\r\n");
#endif
        return 4;
    }

    if (trama->trama_rx[4] != trama->trama_tx[4] ||
        trama->trama_rx[5] != trama->trama_tx[5]) {
#ifdef DEBUG_RF1276
        printf("Nodo destino erroneo\r\n");
#endif
        return 5;
    }

    for (int i = 1; i < 7; i++) {
        check_sum = check_sum ^ trama->trama_rx[i];
    }
    if (check_sum != trama->trama_rx[7]) {
#ifdef DEBUG_RF1276
        printf("Error de CRC Local\r\n");
#endif
        return 8;
    }

    switch (trama->trama_rx[6]) {

    case 0:
        trama->result_send_data = 0x00;
#ifdef DEBUG_RF1276
        printf("Mensaje recibido por nodo %02x%02x\r\n", trama->trama_rx[4],
               trama->trama_rx[5]);
#endif
        break;

    case 0xe1:
#ifdef DEBUG_RF1276
        printf("Error CRC\r\n");
#endif
        trama->result_send_data = 0xe1;
        break;

    case 0xe4:
#ifdef DEBUG_RF1276
        printf("Falla de seguridad\r\n");
#endif
        trama->result_send_data = 0xe4;
        break;

    case 0xe5:
#ifdef DEBUG_RF1276
        printf("Sobre escritura de MAC\r\n");
#endif
        trama->result_send_data = 0xe5;
        break;

    case 0xe6:
#ifdef DEBUG_RF1276
        printf("Parametros invalidos\r\n");
#endif
        trama->result_send_data = 0xe6;
        break;

    case 0xe7:
#ifdef DEBUG_RF1276
        printf("No ACK para el siguiente salto\r\n");
#endif
        trama->result_send_data = 0xe7;
        break;

    case 0xea:
#ifdef DEBUG_RF1276
        printf("Red ocupada\r\n");
#endif
        trama->result_send_data = 0xea;
        break;

    case 0xc1:
#ifdef DEBUG_RF1276
        printf("Red ID invalida\r\n");
#endif
        trama->result_send_data = 0xc1;
        break;

    case 0xc2:
#ifdef DEBUG_RF1276
        printf("Peticion invalida\r\n");
#endif
        trama->result_send_data = 0xc2;
        break;

    case 0xc7:
#ifdef DEBUG_RF1276
        printf("No router\r\n");
#endif
        trama->result_send_data = 0xc7;
        break;

    case 0xd1:
#ifdef DEBUG_RF1276
        printf("Buffer full\r\n");
#endif
        trama->result_send_data = 0xd1;
        break;

    case 0xd2:
#ifdef DEBUG_RF1276
        printf("No ACK en la capa de aplicacion\r\n");
#endif
        trama->result_send_data = 0xd2;
        break;

    case 0xd3:
#ifdef DEBUG_RF1276
        printf("Data de aplicacion sobre pasada\r\n");
#endif
        trama->result_send_data = 0xd3;
        break;

    default:
        return 7;
        break;
    }

    return trama->result_send_data;
}

void read_config_rf1276(struct trama_read_config *trama) {

    uint8_t check_sum;

    trama->trama_tx[0] = 0x01;
    check_sum          = trama->trama_tx[0];
    trama->trama_tx[1] = 0x00;
    trama->trama_tx[2] = 0x02;
    trama->trama_tx[3] = 0x0d;
    trama->trama_tx[4] = 0xa5;
    trama->trama_tx[5] = 0xa5;
    for (int i = 0; i < 11; i++) {
        trama->trama_tx[6 + i] = 0x00;
    }
    for (int i = 1; i < 16; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }

    trama->trama_tx[17] = check_sum;

#ifdef DEBUG_RF1276
    printf("Trama Read Config armada\r\n");
#endif

    return;
}

int check_read_config_rf1276(struct trama_read_config *trama) {

    uint8_t check_sum;
    ident_t aux_id;
    freq_t freq;

    if (trama->trama_tx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Read Config en byte 1, debe ser 0x01\r\n");
#endif
        return -1;
    }

    if (trama->trama_tx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Read Config en byte 2, debe ser 0x00\r\n");
#endif
        return -2;
    }

    if (trama->trama_tx[2] != 0x02) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Read Config en byte 3, debe ser 0x01");
#endif
        return -3;
    }

    if (trama->trama_tx[3] != 0x0d) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Read Config en byte 4, debe ser 0x0d\r\n");
#endif
        return -4;
    }

    if (trama->trama_tx[4] != 0xa5) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Read Config en byte 5, debe ser 0xa5\r\n");
#endif
        return -5;
    }

    if (trama->trama_tx[5] != 0xa5) {
#ifdef DEBUG_RF1276
        printf("Error en trama tx Read Config en byte 6, debe ser 0xa5\r\n");
#endif
        return -6;
    }

    for (int i = 6; i < 17; i++) {
        if (trama->trama_tx[i] != 0x00) {
#ifdef DEBUG_RF1276
            printf(
                "Error en trama tx Read Config en byte 6, debe ser 0x00\r\n");
#endif
            return -i;
        }
    }

    check_sum = trama->trama_tx[0];

    for (int i = 1; i < 17; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }

    if (check_sum != trama->trama_tx[17]) {
#ifdef DEBUG_RF1276
        printf("Error CRC en trama tx Read Config\r\n");
#endif
        return -18;
    }

    if (trama->trama_rx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Trama rx Read Config  incorrecta en byte 1");
#endif
        return 1;
    }

    if (trama->trama_rx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Trama rx Read Config  incorrecta en byte 2");
#endif
        return 2;
    }

    if (trama->trama_rx[2] != 0x82) {
#ifdef DEBUG_RF1276
        printf("Trama rx Read Config  incorrecta en byte 3");
#endif
        return 3;
    }

    if (trama->trama_rx[3] != 0x0d) {
#ifdef DEBUG_RF1276
        printf("Trama rx Read Config  incorrecta en byte 4");
#endif
        return 4;
    }

    if (trama->trama_rx[4] != 0xa5) {
#ifdef DEBUG_RF1276
        printf("Trama rx Read Config  incorrecta en byte 5");
#endif
        return 5;
    }

    if (trama->trama_rx[5] != 0xa5) {
#ifdef DEBUG_RF1276
        printf("Trama rx Read Config incorrecta en byte 6");
#endif
        return 6;
    }

    freq.freq_high.byte_high = 0x00;
    freq.freq_high.byte_low  = trama->trama_rx[6];
    freq.freq_low.byte_high  = trama->trama_rx[7];
    freq.freq_low.byte_low   = trama->trama_rx[8];

    freq.valor = freq.valor * 61035.0 / 1000000000;

    if (freq.valor < 380 || freq.valor > 510) {
#ifdef DEBUG_RF1276
        printf(
            "Frecuencia fuera de rango en rx Read Config [380 - 510] MHz\r\n");
#endif
        return 7;
    }

    if (trama->trama_rx[9] > 7 || trama->trama_rx[9] < 1) {
#ifdef DEBUG_RF1276
        printf(
            "Power Transmiting fuera de rango en rx Read Config [1 - 7]\r\n");
#endif
        return 10;
    }

    if (trama->trama_rx[10] > 23) {
#ifdef DEBUG_RF1276
        printf("Routing time fuera de rango en rx Read Config [1 - 24]h\r\n");
#endif
        return 11;
    }

    aux_id.byte_high = trama->trama_rx[11];
    aux_id.byte_low  = trama->trama_rx[12];

    if (aux_id.valor < 1) {
#ifdef DEBUG_RF1276
        printf("Network ID fuera de rango en rx Read Config [1 - 65535]\r\n");
#endif
        return 12;
    }

    aux_id.byte_high = trama->trama_rx[13];
    aux_id.byte_low  = trama->trama_rx[14];

    if (aux_id.valor < 1) {
#ifdef DEBUG_RF1276
        printf("Node ID fuera de rango en rx Read Config [1 - 65535]\r\n");
#endif
        return 14;
    }

    if (trama->trama_rx[15] > 0x07) {
#ifdef DEBUG_RF1276
        printf("Baud rate fuera de rango en rx Read Config [1200- 115200]\r\n");
#endif
        return 16;
    }

    if (trama->trama_rx[16] > 0x03) {
#ifdef DEBUG_RF1276
        printf("Port Check fuera de rango en rx Read Config [0 - 3]\r\n");
#endif
        return 17;
    }

    check_sum = trama->trama_rx[0];

    for (int i = 1; i < 17; i++) {
        check_sum = check_sum ^ trama->trama_rx[i];
    }

    if (check_sum != trama->trama_rx[17]) {
#ifdef DEBUG_RF1276
        printf("Error de CRC en trama rx Read config\r\n");
#endif
        return 18;
    }

    trama->result_read_config.baud_rate =
        (trama->trama_rx[15] == 0)
            ? 1200
            : (trama->trama_rx[15] == 1)
                  ? 2400
                  : (trama->trama_rx[15] == 2)
                        ? 4800
                        : (trama->trama_rx[15] == 3)
                              ? 9600
                              : (trama->trama_rx[15] == 4)
                                    ? 19200
                                    : (trama->trama_rx[15] == 5)
                                          ? 38400
                                          : (trama->trama_rx[15] == 6) ? 57600
                                                                       : 115200;
    trama->result_read_config.freq         = freq.valor;
    trama->result_read_config.power        = trama->trama_rx[9];
    trama->result_read_config.routing_time = trama->trama_rx[10];
    trama->result_read_config.network_id =
        trama->trama_rx[11] << 8 | trama->trama_rx[12];
    trama->result_read_config.node_id =
        trama->trama_rx[13] << 8 | trama->trama_rx[14];
    trama->result_read_config.port_check = trama->trama_rx[16];

#ifdef DEBUG_RF1276
    printf("Trama Read Config verificada, resultados en arreglo "
           "result_read_config\r\n");
#endif

    return 0;
}

void reset_rf1276(struct trama_reset *trama) {

    uint8_t check_sum;

    trama->trama_tx[0] = 0x01;
    check_sum          = trama->trama_tx[0];

    trama->trama_tx[1] = 0x00;
    trama->trama_tx[2] = 0x07;
    trama->trama_tx[3] = 0x00;
    for (int i = 1; i < 4; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }
    trama->trama_tx[4] = check_sum;

#ifdef DEBUG_RF1276
    printf("Trama Reset Modem armada\r\n");
#endif

    return;
}

int check_reset_rf1276(struct trama_reset *trama) {

    uint8_t check_sum;

    if (trama->trama_tx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx, el byte 1 debe ser igual a 0x01\r\n");
#endif
        return -1;
    }

    if (trama->trama_tx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx, el byte 2 debe ser igual a 0x00\r\n");
#endif
        return -2;
    }

    if (trama->trama_tx[2] != 0x07) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx, el byte 3 debe ser igual a 0x07\r\n");
#endif
        return -3;
    }

    if (trama->trama_tx[3] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx, el byte 4 debe ser igual a 0x00\r\n");
#endif
        return -4;
    }

    check_sum = trama->trama_tx[0];
    for (int i = 1; i < 4; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }
    if (check_sum != trama->trama_tx[4]) {
#ifdef DEBUG_RF1276
        printf("Error CRC trama Tx\r\n");
#endif
        return -5;
    }

    if (trama->trama_rx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx, el byte 1 debe ser igual a 0x01\r\n");
#endif
        return 1;
    }

    if (trama->trama_rx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx, el byte 2 debe ser igual a 0x00\r\n");
#endif
        return 2;
    }

    if (trama->trama_rx[2] != 0x87) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx, el byte 3 debe ser igual a 0x87\r\n");
#endif
        return 3;
    }

    if (trama->trama_rx[3] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx, el byte 4 debe ser igual a 0x01\r\n");
#endif
        return 4;
    }

    if (trama->trama_rx[4] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx, el byte 5 debe ser igual a 0x00\r\n");
#endif
        return 5;
    }

    check_sum = trama->trama_rx[0];
    for (int i = 1; i < 5; i++) {
        check_sum = check_sum ^ trama->trama_rx[i];
    }
    if (check_sum != trama->trama_rx[5]) {
#ifdef DEBUG_RF1276
        printf("Error CRC trama Rx\r\n");
#endif
        return 6;
    }

#ifdef DEBUG_RF1276
    printf("Trama Reset Modem Verificada\r\n");
#endif

    return 0;
}

void read_version_rf1276(struct trama_read_version *trama) {

    uint8_t check_sum;

    trama->trama_tx[0] = 0x01;
    check_sum          = trama->trama_tx[0];

    trama->trama_tx[1] = 0x00;
    trama->trama_tx[2] = 0x06;
    trama->trama_tx[3] = 0x00;
    for (int i = 1; i < 3; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }
    trama->trama_tx[4] = check_sum;

#ifdef DEBUG_RF1276
    printf("Trama Read Version armada\r\n");
#endif

    return;
}

int check_read_version_rf1276(struct trama_read_version *trama) {

    uint8_t check_sum;

    if (trama->trama_tx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en trama Tx Read version en byte 1, debe ser 0x01\r\n");
#endif
        return -1;
    }

    if (trama->trama_tx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en trama Tx Read version en byte 2, debe ser 0x00\r\n");
#endif
        return -2;
    }

    if (trama->trama_tx[2] != 0x06) {
#ifdef DEBUG_RF1276
        printf("Error en trama Tx Read version en byte 3, debe ser 0x06\r\n");
#endif
        return -3;
    }

    if (trama->trama_tx[3] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en trama Tx Read version en byte 4, debe ser 0x00\r\n");
#endif
        return -4;
    }

    if (trama->trama_tx[4] != 0x07) {
#ifdef DEBUG_RF1276
        printf("Error en trama Tx Read version en byte 5, debe ser 0x07\r\n");
#endif
        return -5;
    }

    if (trama->trama_rx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en trama  Rx Read version en byte 1, debe ser 0x01\r\n");
#endif
        return 1;
    }

    if (trama->trama_rx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en trama  Rx Read version en byte 2, debe ser 0x00\r\n");
#endif
        return 2;
    }

    if (trama->trama_rx[2] != 0x86) {
#ifdef DEBUG_RF1276
        printf("Error en trama  Rx Read version en byte 3, debe ser 0x86\r\n");
#endif
        return 3;
    }

    if (trama->trama_rx[3] != 0x21) {
#ifdef DEBUG_RF1276
        printf("Error en trama  Rx Read version en byte 3, debe ser 0x21\r\n");
#endif
        return 4;
    }

    check_sum = trama->trama_rx[0];
    for (int i = 1; i < 38; i++) {
        check_sum = check_sum ^ trama->trama_rx[i];
    }
    if (check_sum != trama->trama_rx[38]) {
#ifdef DEBUG_RF1276
        printf("Error de CRC trama Rx Read version\r\n");
#endif
        return 39;
    }

#ifdef DEBUG_RF1276
    printf("Version: ");
    for (int i = 0; i < trama->trama_rx[3]; i++) {
        printf("%c", (char)trama->trama_rx[4 + i]);
    }
#endif

    return 0;
}

void read_routing_table_rf1276(struct trama_read_routing *trama) {

    uint8_t check_sum;

    trama->trama_tx[0] = 0x01;
    check_sum          = trama->trama_tx[0];

    trama->trama_tx[1] = 0x00;
    trama->trama_tx[2] = 0x03;
    trama->trama_tx[3] = 0x00;
    for (int i = 1; i < 3; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }
    trama->trama_tx[4] = check_sum;

#ifdef DEBUG_RF1276
    printf("Trama Routing Table armada\r\n");
#endif

    return;
}

int check_read_routing_table_rf1276(struct trama_read_routing *trama) {

    static ident_t aux_source_id, aux_target_id, aux_jump_id;
    uint8_t count_path = 0, count_jump = 0, check_sum;

    trama->routing.paths_count = 0x00;
    for (int i = 0; i < 60; i++) {
        trama->routing.paths[i].hop_counts        = 0x00;
        trama->routing.paths[i].target_node.valor = 0x00;
        for (int j = 0; j < 8; j++) {
            trama->routing.paths[i].jump[j].valor       = 0x00;
            trama->routing.paths[i].rout_information[j] = 0x00;
            trama->routing.paths[i].time[j].valor       = 0x00;
        }
    }

    if (trama->trama_tx[0] != 0x01) {
        return -1;
    }

    if (trama->trama_tx[1] != 0x00) {
        return -2;
    }

    if (trama->trama_tx[2] != 0x03) {
        return -3;
    }

    if (trama->trama_tx[3] != 0x00) {
        return -4;
    }

    if (trama->trama_tx[4] != 0x02) {
        return -5;
    }

    if (trama->trama_rx[0] != 0x01) {
        return 1;
    }

    if (trama->trama_rx[1] != 0x00) {
        return 2;
    }

    if (trama->trama_rx[2] != 0x83) {
        return 3;
    }

    if (trama->trama_rx[3] != 0x3c) {
        return 4;
    }

    aux_source_id.byte_high = trama->trama_rx[4];
    aux_source_id.byte_low  = trama->trama_rx[5];

    if (aux_source_id.valor < 1) {
        return 5;
    }

    if (trama->trama_rx[6] != 0x02) {
        return 7;
    }

    if (trama->trama_rx[7] != 0x1d) {
        return 8;
    }

    for (int i = 0; i < 60; i++) {

        if (trama->trama_rx[i * 9 + 8] == 0xff &&
            trama->trama_rx[i * 9 + 9] == 0xff) {
            break;
        }

        aux_target_id.byte_high = trama->trama_rx[i * 9 + 8];
        aux_target_id.byte_low  = trama->trama_rx[i * 9 + 9];

        aux_jump_id.byte_high = trama->trama_rx[i * 9 + 11];
        aux_jump_id.byte_low  = trama->trama_rx[i * 9 + 12];

#ifdef DEBUG_RF1276
        printf("TARGET NODE ID %d, JUMP NODE ID %d\r\n", aux_target_id.valor,
               aux_jump_id.valor);
#endif

        trama->routing.paths[count_path].jump[count_jump].valor =
            aux_jump_id.valor;
        trama->routing.paths[count_path].rout_information[count_jump] =
            trama->trama_rx[i * 9 + 10];
        trama->routing.paths[count_path].time[count_jump].freq_high.byte_high =
            trama->trama_rx[i * 9 + 13];
        trama->routing.paths[count_path].time[count_jump].freq_high.byte_low =
            trama->trama_rx[i * 9 + 14];
        trama->routing.paths[count_path].time[count_jump].freq_low.byte_high =
            trama->trama_rx[i * 9 + 15];
        trama->routing.paths[count_path].time[count_jump].freq_low.byte_low =
            trama->trama_rx[i * 9 + 16];

        if (aux_target_id.valor == aux_jump_id.valor) {
            count_jump++;
            trama->routing.paths[count_path].hop_counts = count_jump;
            trama->routing.paths[count_path].target_node.valor =
                aux_target_id.valor;
            count_path++;
            count_jump                 = 0;
            trama->routing.paths_count = count_path;
        } else {
            count_jump++;
            trama->routing.paths[count_path].hop_counts = count_jump;
        }
    }

    check_sum = trama->trama_rx[0];

    for (int i = 1; i < 549; i++) {
        check_sum = check_sum ^ trama->trama_rx[i];
    }

    if (check_sum != trama->trama_rx[549]) {
#ifdef DEBUG_RF1276
        printf("Error CRC %02x\r\n", check_sum);
#endif
        return 550;
    }

#ifdef DEBUG_RF1276
    printf("NUMERO DE RUTAS %d\r\n", trama->routing.paths_count);

    for (int i = 0; i < trama->routing.paths_count; i++) {
        printf("RUTA %d, Saltos %d\r\n", i + 1,
               trama->routing.paths[i].hop_counts);

        printf("Source Node ID %d --> ", aux_source_id.valor);

        for (int j = 0; j < trama->routing.paths[i].hop_counts; j++) {

            if (j == (trama->routing.paths[i].hop_counts - 1)) {
                printf("Target Node ID %d",
                       trama->routing.paths[i].target_node.valor);
                continue;
            }
            printf("Jump Node ID %d --> ",
                   trama->routing.paths[i].jump[j].valor);
        }
        printf("\r\n");
    }
#endif

    return 0;
}

void read_routing_path_rf1276(struct trama_read_path *trama) {

    uint8_t check_sum;

    trama->trama_tx[0] = 0x01;
    check_sum          = trama->trama_tx[0];

    trama->trama_tx[1] = 0x00;
    trama->trama_tx[2] = 0x08;
    trama->trama_tx[3] = 0x00;
    for (int i = 1; i < 3; i++) {
        check_sum = check_sum ^ trama->trama_tx[i];
    }
    trama->trama_tx[4] = check_sum;

#ifdef DEBUG_RF1276
    printf("Trama Routing Path armada\r\n");
#endif

    return;
}

int check_read_routing_path_rf1276(struct trama_read_path *trama) {

    uint8_t check_sum;

    if (trama->trama_tx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx Read Routing Path, en byte 1, deberia ser "
               "0x01\r\n");
#endif
        return -1;
    }

    if (trama->trama_tx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx Read Routing Path, en byte 2, deberia ser "
               "0x00\r\n");
#endif
        return -2;
    }

    if (trama->trama_tx[2] != 0x08) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx Read Routing Path, en byte 3, deberia ser "
               "0x08\r\n");
#endif
        return -3;
    }

    if (trama->trama_tx[3] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx Read Routing Path, en byte 4, deberia ser "
               "0x00\r\n");
#endif
        return -4;
    }

    if (trama->trama_tx[4] != 0x09) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Tx Read Routing Path, en byte 5, deberia ser "
               "0x09\r\n");
#endif
        return -5;
    }

    if (trama->trama_rx[0] != 0x01) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx Read Routing Path, en byte 1, deberia ser "
               "0x01\r\n");
#endif
        return 1;
    }

    if (trama->trama_rx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx Read Routing Path, en byte 2, deberia ser "
               "0x00\r\n");
#endif
        return 2;
    }

    if (trama->trama_rx[2] != 0x88) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx Read Routing Path, en byte 3, deberia ser "
               "0x88\r\n");
#endif
        return 3;
    }

    if (trama->trama_rx[3] != 0x1b) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx Read Routing Path, en byte 4, deberia ser "
               "0x1b\r\n");
#endif
        return 4;
    }

    if (trama->trama_rx[4] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx Read Routing Path, en byte 5, deberia ser "
               "0x00\r\n");
#endif
        return 5;
    }

    check_sum = trama->trama_rx[0];

    for (int i = 1; i < 31; i++) {
        check_sum = check_sum ^ trama->trama_rx[i];
    }
    if (check_sum != trama->trama_rx[31]) {
#ifdef DEBUG_RF1276
        printf("Error en Trama Rx Read Routing Path CRC\r\n");
#endif
        return 32;
    }

    for (int i = 0; i < 9; i++) {
        trama->repeater.signal[i]               = trama->trama_rx[i * 3 + 4];
        trama->repeater.repeaterID[i].byte_high = trama->trama_rx[i * 3 + 5];
        trama->repeater.repeaterID[i].byte_low  = trama->trama_rx[i * 3 + 6];
    }

#ifdef DEBUG_RF1276
    printf("Trama Routing Path Verificada\r\n");
    for (int i = 0; i < 9; i++) {
        if (i == 0) {
            printf("Source ID%d ", trama->repeater.repeaterID[i].valor);
            continue;
        }
        if (trama->repeater.signal[i] == 0xff ||
            trama->repeater.repeaterID[i].valor == 0xff) {
            break;
        }
        printf("-->");
        printf("Signal Value: %d, Repeater %d ID%d", trama->repeater.signal[i],
               i, trama->repeater.repeaterID[i].valor);
    }
    printf("\r\n");
#endif

    return 0;
}

int receive_packet_rf1276(struct trama_receive *trama) {

    //  uint8_t trama_test[3] = {0x05, 0x00, 0x82};
    uint8_t check_sum;

    if (trama->trama_rx[0] != 0x05) {
#ifdef DEBUG_RF1276
        printf("Error en trama en byte 1, deberia ser 0x05\r\n");
#endif
        return 1;
    }

    if (trama->trama_rx[1] != 0x00) {
#ifdef DEBUG_RF1276
        printf("Error en trama en byte 2, deberia ser 0x00\r\n");
#endif
        return 2;
    }

    if (trama->trama_rx[2] != 0x82) {
#ifdef DEBUG_RF1276
        printf("Error en trama en byte 3, deberia ser 0x82\r\n");
#endif
        return 3;
    }

    check_sum = trama->trama_rx[0];

    for (int i = 1; i < 4 + trama->trama_rx[3]; i++) {
        check_sum = check_sum ^ trama->trama_rx[i];
    }
    if (check_sum != trama->trama_rx[4 + trama->trama_rx[3]]) {
#ifdef DEBUG_RF1276
        printf("Error en trama CRC\r\n");
#endif
        return -1;
    }

    trama->respond.source_node.byte_high = trama->trama_rx[4];
    trama->respond.source_node.byte_low  = trama->trama_rx[5];
    trama->respond.signal                = trama->trama_rx[6];
    trama->respond.data_length           = trama->trama_rx[7];

    for (int i = 0; i < trama->respond.data_length; i++) {
        trama->respond.data[i] = trama->trama_rx[i + 8];
    }

#ifdef DEBUG_RF1276
    printf("Source node %d, Signal %d, Data Length %d, Data ",
           trama->respond.source_node.valor, trama->respond.signal,
           trama->respond.data_length);
    for (int i = 0; i < trama->respond.data_length; i++) {
        printf("0x%02x ", trama->respond.data[i]);
    }
    printf("\r\n");
#endif

    return 0;
}
