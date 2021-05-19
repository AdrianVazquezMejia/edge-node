/*
 * led.h
 *
 *  Created on: Dec 13, 2020
 *      Author: adrian-estelio
 */
/**@file
 * @brief Functions to drive the LED indicator
 * */
#ifndef MAIN_INCLUDE_LED_H_
#define MAIN_INCLUDE_LED_H_
/**
 * @brief Function to blink the led on start up. Three short blinks.
 * */
void led_startup(void);
/**
 * @brief Function for a single blink.
 * */
void led_blink(void);

#endif /* MAIN_INCLUDE_LED_H_ */
