/*
 * lora_mesh.c
 *
 *  Created on: Apr 23, 2021
 *      Author: adrian-estelio
 */
#include "lora_mesh.h"
#include "string.h"
void prepare_to_send(lora_mesh_t *loraFrame, mb_response_t *modbus_response) {
    loraFrame->header_.frame_type_   = APPLICATION_DATA;
    loraFrame->header_.frame_number_ = 0x00;
    loraFrame->header_.command_type_ = SENDING_DATA;
    loraFrame->header_.load_len_     = 0x06 + modbus_response->len;

    loraFrame->load_.transmit_load_.dest_address_.high_byte_ =
        loraFrame->load_.recv_load_.source_address_.high_byte_;
    loraFrame->load_.transmit_load_.dest_address_.low_byte_ =
        loraFrame->load_.recv_load_.source_address_.low_byte_;
    loraFrame->load_.transmit_load_.ack_request_    = 0x00;
    loraFrame->load_.transmit_load_.sending_radius_ = 0x07;
    loraFrame->load_.transmit_load_.routing_type_   = 0x01;
    loraFrame->load_.transmit_load_.data_len_       = modbus_response->len;
    memcpy(loraFrame->load_.transmit_load_.data_, modbus_response->frame,
           modbus_response->len);
}
