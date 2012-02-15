/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_HARDWARE_KEYMASTER_H
#define ANDROID_HARDWARE_KEYMASTER_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>

__BEGIN_DECLS

/**
 * The id of this module
 */
#define KEYSTORE_HARDWARE_MODULE_ID "keystore"

#define KEYSTORE_KEYMASTER "keymaster"

struct keystore_module {
    struct hw_module_t common;
};

/**
 * Key algorithm for imported keypairs.
 */
typedef enum {
    ALGORITHM_RSA,
} keymaster_keypair_algorithm_t;

/**
 * The parameters that can be set for a given keymaster implementation.
 */
struct keymaster_device {
    struct hw_device_t common;

    void* context;

    /**
     * Generates a public and private key. The key-blob returned is opaque
     * and will subsequently provided for signing and verification.
     *
     * Returns: 0 on success or an error code less than 0.
     */
    int (*generate_rsa_keypair)(const struct keymaster_device* dev,
            int modulus_size, unsigned long public_exponent,
            uint8_t** keyBlob, size_t* keyBlobLength);

    /**
     * Imports a public and private key pair. The imported keys should be in
     * DER format. The key-blob returned is opaque and can be subsequently
     * provided for signing and verification.
     *
     * Returns: 0 on success or an error code less than 0.
     */
    int (*import_keypair)(const struct keymaster_device* dev,
            keymaster_keypair_algorithm_t algorithm,
            uint8_t* privateKey, size_t* privateKeyLength,
            uint8_t* publicKey, size_t* publicKeyLength,
            uint8_t** keyBlob, size_t* keyBlobLength);

    /**
     * Signs data using a key-blob generated before.
     *
     * Returns: 0 on success or an error code less than 0.
     */
    int (*sign_data)(const struct keymaster_device* dev,
            const uint8_t* keyBlob, const size_t keyBlobLength,
            const uint8_t* data, const size_t dataLength,
            uint8_t** signedData, size_t* signedDataLength);

    /**
     * Verifies data signed with a key-blob.
     *
     * Returns: 0 on successful verification or an error code less than 0.
     */
    int (*verify_data)(const struct keymaster_device* dev,
            const uint8_t* keyBlob, const size_t keyBlobLength,
            const uint8_t* signedData, const size_t signedDataLength,
            const uint8_t* signature, const size_t signatureLength);
};
typedef struct keymaster_device keymaster_device_t;

__END_DECLS

#endif  // ANDROID_HARDWARE_KEYMASTER_H

