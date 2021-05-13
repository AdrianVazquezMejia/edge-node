#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#include "cypher.h"
#include "esp_err.h"

esp_err_t cfb8encrypt(unsigned char *raw_data, size_t raw_data_len,
                      unsigned char *encrypted_output) {
#if defined(MBEDTLS_CIPHER_MODE_CFB)

    unsigned char iv[] = {0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                          0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    return mbedtls_aes_crypt_cfb8(&aes, MBEDTLS_AES_ENCRYPT, raw_data_len, iv,
                                  raw_data, encrypted_output);
#endif
    return ESP_FAIL;
}

esp_err_t cfb8decrypt(unsigned char *encrypted_data, size_t encrypted_data_len,
                      unsigned char *decrypted_output) {
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    unsigned char iv1[] = {0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    return mbedtls_aes_crypt_cfb8(&aes, MBEDTLS_AES_DECRYPT, encrypted_data_len,
                                  iv1, encrypted_data, decrypted_output);
#endif
    return ESP_FAIL;
}
