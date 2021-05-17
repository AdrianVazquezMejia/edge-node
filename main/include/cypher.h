// only CBC requires that input length shall be multiple of 16
/**
 * @file
 * @brief File containing the cipher capabilities of the node*/
#ifndef MAIN_INCLUDE_CYPHER_H_
#define MAIN_INCLUDE_CYPHER_H_
#include "esp_err.h"
#include "mbedtls/aes.h"

mbedtls_aes_context
    aes; /**< The AES context to use for encryption or decryption. It must be
            initialized and bound to a key. */

/**
 * @brief This function performs an AES-CFB8 encryption
 *        It performs the operation defined in the @p mode
 *        parameter (encrypt/decrypt), on the input data buffer defined
 *        in the @p input parameter.
 *
 *
 * @note  Upon exit, the content of the IV is updated so that you can
 *        call the same function again on the next
 *        block(s) of data and get the same result as if it was
 *        encrypted in one call. This allows a "streaming" usage.
 *        If you need to retain the contents of the
 *        IV, you should either save it manually or use the cipher
 *        module instead.
 *
 * @param[in] raw_data_len  The length of the input data.

 * @param[in] raw_data   The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * @param[out] encrypted_output   The buffer holding the output data.
 *                 It must be writeable and of size \p length Bytes.
 *
 * @return         \c ESP_OK on success.
 */
esp_err_t cfb8encrypt(unsigned char *raw_data, size_t raw_data_len,
                      unsigned char *encrypted_output);

/**
 * @brief This function performs an AES-CFB8 decryption
 *        operation.
 *
 *        It performs the operation defined in the @p mode
 *        parameter (encrypt/decrypt), on the input data buffer defined
 *        in the @p input parameter.
 *
 *
 * @note  Upon exit, the content of the IV is updated so that you can
 *        call the same function again on the next
 *        block(s) of data and get the same result as if it was
 *        encrypted in one call. This allows a "streaming" usage.
 *        If you need to retain the contents of the
 *        IV, you should either save it manually or use the cipher
 *        module instead.
 *
 * @param[in] encrypted_data_len  The length of the input data.

 * @param[in] encrypted_data   The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * @param[out] decrypted_output   The buffer holding the output data.
 *                 It must be writeable and of size \p length Bytes.
 *
 * @return         \c ESP_OK on success.
 */
esp_err_t cfb8decrypt(unsigned char *encrypted_data, size_t encrypted_data_len,
                      unsigned char *decrypted_output);

#endif /* MAIN_INCLUDE_CYPHER_H_ */
