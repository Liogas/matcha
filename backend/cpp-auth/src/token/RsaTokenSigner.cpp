#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/bn.h>
#include <jwt-cpp/jwt.h> 
#include <openssl/pem.h>

#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <auth/token/RsaTokenSigner.hpp>
#include <auth/crypto/Base64Url.hpp>

namespace auth
{
    namespace
    {
        using BIGNUMPtr =
            std::unique_ptr<BIGNUM, decltype(&BN_free)>;

        std::string encodeBase64Url(const BIGNUM &number)
        {
            const int byteCount = BN_num_bytes(&number);
            if (byteCount <= 0)
                throw std::runtime_error(
                    "Unable to encode an empty RSA public parameter"
                );

            std::vector<unsigned char> bytes(
                static_cast<std::size_t>(byteCount)
            );
            if (BN_bn2bin(&number, bytes.data()) != byteCount)
                throw std::runtime_error(
                    "Unable to encode an RSA public parameter"
                );

            return auth::encodeBase64Url(
                std::string(
                    reinterpret_cast<const char *>(bytes.data()),
                    bytes.size()
                )
            );
        }

        EVP_PKEY *loadPrivateKey(const std::string &path)
        {
            std::ifstream file(path);

            if (!file)
            {
                throw std::runtime_error(
                    "Unable to open private key: " + path
                );
            }

            const std::string pem{
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            };

            if (pem.size() > static_cast<std::size_t>(
                    std::numeric_limits<int>::max()
                ))
                throw std::runtime_error(
                    "Private key file is too large"
                );

            std::unique_ptr<BIO, decltype(&BIO_free)> bio(
                BIO_new_mem_buf(
                    pem.data(),
                    static_cast<int>(pem.size())
                ),
                BIO_free
            );

            if (!bio)
            {
                throw std::runtime_error(
                    "Unable to create BIO for private key"
                );
            }

            std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
                PEM_read_bio_PrivateKey(
                    bio.get(),
                    nullptr,
                    nullptr,
                    nullptr
                ),
                EVP_PKEY_free
            );

            if (!key)
            {
                throw std::runtime_error(
                    "Unable to read private key"
                );
            }

            if (EVP_PKEY_is_a(key.get(), "RSA") != 1 ||
                EVP_PKEY_get_bits(key.get()) < 2048)
                throw std::runtime_error(
                    "Private signing key must be RSA with at least 2048 bits"
                );

            std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>
                keyContext(
                    EVP_PKEY_CTX_new(key.get(), nullptr),
                    EVP_PKEY_CTX_free
                );
            if (!keyContext || EVP_PKEY_private_check(keyContext.get()) != 1)
                throw std::runtime_error(
                    "Private signing key is invalid"
                );

            return key.release();
        }
    }

    RsaTokenSigner::RsaTokenSigner(
        const std::string &privateKeyPath,
        const std::string &keyId
    ):
        _keyId(keyId),
        _privateKey(nullptr, EVP_PKEY_free)
    {
        if (privateKeyPath.empty())
            throw std::invalid_argument(
                "Private key path cannot be empty"
            );
        if (this->_keyId.empty())
            throw std::invalid_argument(
                "Signing key ID cannot be empty"
            );
        this->_privateKey.reset(
            loadPrivateKey(privateKeyPath)
        );
    }

    std::string RsaTokenSigner::sign(
        const std::string &data
    )
    {
        EVP_MD_CTX *context = EVP_MD_CTX_new();

        if (!context)
        {
            throw std::runtime_error(
                "Unable to create EVP_MD_CTX"
            );
        }

        const auto initResult = EVP_DigestSignInit(
            context,
            nullptr,
            EVP_sha256(),
            nullptr,
            this->_privateKey.get()
        );

        if (initResult != 1)
        {
            EVP_MD_CTX_free(context);

            throw std::runtime_error(
                "Unable to initialize RSA-SHA256 signing"
            );
        }

        const auto updateResult = EVP_DigestSignUpdate(
            context,
            data.data(),
            data.size()
        );

        if (updateResult != 1)
        {
            EVP_MD_CTX_free(context);

            throw std::runtime_error(
                "Unable to update RSA-SHA256 signing"
            );
        }

        size_t signatureSize = 0;

        const auto sizeResult = EVP_DigestSignFinal(
            context,
            nullptr,
            &signatureSize
        );

        if (sizeResult != 1)
        {
            EVP_MD_CTX_free(context);

            throw std::runtime_error(
                "Unable to determine signature size"
            );
        }

        std::vector<unsigned char> signature(
            signatureSize
        );

        const auto signResult = EVP_DigestSignFinal(
            context,
            signature.data(),
            &signatureSize
        );

        if (signResult != 1)
        {
            EVP_MD_CTX_free(context);

            throw std::runtime_error(
                "Unable to generate signature"
            );
        }

        EVP_MD_CTX_free(context);

        return auth::encodeBase64Url(
            std::string(
                reinterpret_cast<const char *>(signature.data()),
                signatureSize
            )
        );
    }

    std::string RsaTokenSigner::keyId() const
    {
        return this->_keyId;
    }

    Jwk RsaTokenSigner::publicJwk() const
    {
        BIGNUM *rawModulus = nullptr;
        if (EVP_PKEY_get_bn_param(
                this->_privateKey.get(),
                OSSL_PKEY_PARAM_RSA_N,
                &rawModulus
            ) != 1)
            throw std::runtime_error(
                "Unable to read RSA public modulus"
            );
        BIGNUMPtr modulus(rawModulus, BN_free);

        BIGNUM *rawExponent = nullptr;
        if (EVP_PKEY_get_bn_param(
                this->_privateKey.get(),
                OSSL_PKEY_PARAM_RSA_E,
                &rawExponent
            ) != 1)
            throw std::runtime_error(
                "Unable to read RSA public exponent"
            );
        BIGNUMPtr exponent(rawExponent, BN_free);

        return {
            this->_keyId,
            "RSA",
            "RS256",
            encodeBase64Url(*modulus),
            encodeBase64Url(*exponent)
        };
    }
}