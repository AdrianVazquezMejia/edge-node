// only CBC requires that input length shall be multiple of 16
#include "mbedtls/aes.h"

int64_t initTime, encryptTime, decryptTime;

mbedtls_aes_context aes;

void cfb8encrypt(unsigned char * raw_data, size_t raw_data_len, unsigned char * encrypted_output);
void cfb8decrypt(unsigned char * encrypted_data, size_t encrypted_data_len, unsigned char * decrypted_output);
