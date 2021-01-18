#include <stdio.h>
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_timer.h"
#include "mbedtls/aes.h"
// only CBC requires that input length shall be multiple of 16
#define INPUT_LENGTH 16
unsigned char input[INPUT_LENGTH] = {0};

int64_t initTime, encryptTime, decryptTime;

mbedtls_aes_context aes;

unsigned char encrypt_output[INPUT_LENGTH] = {0};
unsigned char decrypt_output[INPUT_LENGTH] = {0};

// key length 32 bytes for 256 bit encrypting, it can be 16 or 24 bytes for 128
// and 192 bits encrypting mode

unsigned char key[] = {0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					   0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
					   0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					   0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

void cfb8encrypt();
void cfb8decrypt();
