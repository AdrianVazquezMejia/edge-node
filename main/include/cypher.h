// only CBC requires that input length shall be multiple of 16
#include "mbedtls/aes.h"

mbedtls_aes_context aes;
#include "esp_err.h"

esp_err_t cfb8encrypt(unsigned char *raw_data, size_t raw_data_len,
                      unsigned char *encrypted_output);
esp_err_t cfb8decrypt(unsigned char *encrypted_data, size_t encrypted_data_len,
                      unsigned char *decrypted_output);
