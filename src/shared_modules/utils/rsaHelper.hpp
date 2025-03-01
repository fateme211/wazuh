/*
 * Wazuh shared modules utils
 * Copyright (C) 2015, Wazuh Inc.
 * January 24, 2024.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef _RSA_HELPER_HPP
#define _RSA_HELPER_HPP

#include "defer.hpp"
#include <array>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <stdexcept>
#include <string>
#include <vector>

constexpr int RSA_PRIVATE {0};
constexpr int RSA_PUBLIC {1};
constexpr int RSA_CERT {2};

namespace Utils
{
    /**
     * Extracts the public key from a X.509 certificate
     *
     * @param rsaPublicKey  The RSA structure for the public key
     * @param certFile      The file pointer to the certificate
     */
    static void getPubKeyFromCert(RSA*& rsaPublicKey, FILE* certFile)
    {
        // Read the X.509 certificate from the file
        X509* x509Certificate = PEM_read_X509(certFile, NULL, NULL, NULL);

        if (!x509Certificate)
        {
            throw std::runtime_error("Error reading X.509 certificate");
        }

        // Defered free
        DEFER([x509Certificate]() { X509_free(x509Certificate); });

        // Extract the public key from the X.509 certificate
        EVP_PKEY* evpPublicKey = X509_get_pubkey(x509Certificate);

        if (!evpPublicKey)
        {
            throw std::runtime_error("Error reading public key");
        }
        DEFER([evpPublicKey]() { EVP_PKEY_free(evpPublicKey); });

        // Check the type of key
        if (EVP_PKEY_base_id(evpPublicKey) == EVP_PKEY_RSA)
        {
            // Extract RSA structure from the EVP_PKEY
            rsaPublicKey = EVP_PKEY_get1_RSA(evpPublicKey);

            if (!rsaPublicKey)
            {
                throw std::runtime_error("Error extracting RSA public key from EVP_PKEY");
            }
        }
        else
        {
            throw std::runtime_error("Unsupported key type");
        }
    }

    /**
     * Creates the RSA structure from a certificate or key file
     *
     * @param rsaPublicKey  The RSA structure for the public key
     * @param filePath  The path to the file key string to encrypt the value
     * @param type      The type of file (RSA_PRIVATE, RSA_PUBLIC, RSA_CERT)
     */
    static void createRSA(RSA*& rsaKey, const std::string& filePath, const int type)
    {

        FILE* keyFile = fopen(filePath.c_str(), "r");
        if (!keyFile)
        {
            throw std::runtime_error("Failed to open RSA file");
        }

        // Defered close
        DEFER([keyFile]() { fclose(keyFile); });

        switch (type)
        {
            case RSA_PRIVATE:
                rsaKey = PEM_read_RSAPrivateKey(keyFile, NULL, NULL, NULL);
                if (!rsaKey)
                {
                    throw std::runtime_error("Error reading RSA private key");
                }
                break;
            case RSA_PUBLIC:
                rsaKey = PEM_read_RSA_PUBKEY(keyFile, NULL, NULL, NULL);
                if (!rsaKey)
                {
                    throw std::runtime_error("Error reading RSA public key");
                }
                break;
            case RSA_CERT: getPubKeyFromCert(rsaKey, keyFile); break;
            default: break;
        }
    }

    /**
     * Encrypts the input vector with the provided key
     *
     * @param filePath  The path to the file key string to encrypt the value
     * @param input     The entry to be encrypted
     * @param output    The resulting encrypted value
     * @param cert      If the public key is in a certificate
     * @return          The size of the encrypted output, -1 if error
     */
    int rsaEncrypt(const std::string& filePath, const std::string& input, std::string& output, const bool cert = false)
    {

        RSA* rsa = nullptr;

        createRSA(rsa, filePath, cert ? RSA_CERT : RSA_PUBLIC);

        // Allocate memory for the encryptedValue
        std::vector<unsigned char> encryptedValue(RSA_size(rsa));

        // Defered free
        DEFER([rsa]() { RSA_free(rsa); });

        const auto encryptedLen = RSA_public_encrypt(
            input.length(), (const unsigned char*)input.data(), encryptedValue.data(), rsa, RSA_PKCS1_PADDING);

        if (encryptedLen < 0)
        {
            throw std::runtime_error("RSA encryption failed");
        }

        output = std::string(encryptedValue.begin(), encryptedValue.end());

        return encryptedLen;
    }

    /**
     * Decrypts the input vector with the provided key
     *
     * @param filePath  The path to the file key string to decrypt the value
     * @param input  The entry to be decrypted
     * @param output The resulting decrypted value
     * @return       The size of the decrypted output, -1 if error
     */
    int rsaDecrypt(const std::string& filePath, const std::string& input, std::string& output)
    {

        RSA* rsa = nullptr;

        createRSA(rsa, filePath, RSA_PRIVATE);

        std::string decryptedText(RSA_size(rsa), 0); // Initialize with zeros

        // Defered free
        DEFER([rsa]() { RSA_free(rsa); });

        // Decrypt the ciphertext using RSA private key
        const auto decryptedLen = RSA_private_decrypt(256,
                                                      reinterpret_cast<const unsigned char*>(input.data()),
                                                      reinterpret_cast<unsigned char*>(&decryptedText[0]),
                                                      rsa,
                                                      RSA_PKCS1_PADDING);

        if (decryptedLen < 0)
        {
            throw std::runtime_error("RSA decryption failed");
        }

        // Display the decrypted plaintext
        output = decryptedText.substr(0, decryptedLen);

        return decryptedLen;
    }
} // namespace Utils

#endif // _RSA_HELPER_HPP
