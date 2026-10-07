#include <auth/token/RsaTokenSigner.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace auth
{
    namespace
    {
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

            BIO *bio = BIO_new_mem_buf(
                pem.data(),
                static_cast<int>(pem.size())
            );

            if (!bio)
            {
                throw std::runtime_error(
                    "Unable to create BIO for private key"
                );
            }

            EVP_PKEY *key = PEM_read_bio_PrivateKey(
                bio,
                nullptr,
                nullptr,
                nullptr
            );

            BIO_free(bio);

            if (!key)
            {
                throw std::runtime_error(
                    "Unable to read private key"
                );
            }

            return key;
        }
    }

    RsaTokenSigner::RsaTokenSigner(
        const std::string &privateKeyPath
    ):
        _privateKeyPath(privateKeyPath)
    {
    }

    std::string RsaTokenSigner::sign(
        const std::string &data
    )
    {
        EVP_PKEY *privateKey =
            loadPrivateKey(_privateKeyPath);

        EVP_MD_CTX *context = EVP_MD_CTX_new();

        if (!context)
        {
            EVP_PKEY_free(privateKey);

            throw std::runtime_error(
                "Unable to create EVP_MD_CTX"
            );
        }

        const auto initResult = EVP_DigestSignInit(
            context,
            nullptr,
            EVP_sha256(),
            nullptr,
            privateKey
        );

        if (initResult != 1)
        {
            EVP_MD_CTX_free(context);
            EVP_PKEY_free(privateKey);

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
            EVP_PKEY_free(privateKey);

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
            EVP_PKEY_free(privateKey);

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
            EVP_PKEY_free(privateKey);

            throw std::runtime_error(
                "Unable to generate signature"
            );
        }

        EVP_MD_CTX_free(context);
        EVP_PKEY_free(privateKey);

        return std::string(
            reinterpret_cast<const char *>(signature.data()),
            signatureSize
        );
    }
}