/*
 * led.c
 *
 *  Created on: Dec 13, 2020
 *      Author: adrian-estelio
 */
#include "led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED 19
void led_startup(void) {
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(LED, GPIO_PULLDOWN_ONLY);
    for (uint8_t i = 0; i < 3; i++) {
        gpio_set_level(LED, 1);
        vTaskDelay(20);
        gpio_set_level(LED, 0);
        vTaskDelay(20);
    }
}

void led_blink(void) {
    gpio_set_level(LED, 1);
    vTaskDelay(5);
    gpio_set_level(LED, 0);
    vTaskDelay(5);
}
