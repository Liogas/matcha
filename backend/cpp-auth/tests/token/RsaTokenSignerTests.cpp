#include <gtest/gtest.h>

#include <auth/token/RsaTokenSigner.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{
    std::string readFile(const std::string &path)
    {
        std::ifstream file(path);

        if (!file)
            throw std::runtime_error(
                "Unable to open file: " + path
            );

        return {
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };
    }

    EVP_PKEY *loadPublicKey(const std::string &path)
    {
        const auto pem = readFile(path);

        BIO *bio = BIO_new_mem_buf(
            pem.data(),
            static_cast<int>(pem.size())
        );

        if (!bio)
            return nullptr;

        EVP_PKEY *key = PEM_read_bio_PUBKEY(
            bio,
            nullptr,
            nullptr,
            nullptr
        );

        BIO_free(bio);

        return key;
    }

    bool verifySignature(
        EVP_PKEY *publicKey,
        const std::string &data,
        const std::string &signature
    )
    {
        EVP_MD_CTX *context = EVP_MD_CTX_new();

        if (!context)
            return false;

        const auto initResult = EVP_DigestVerifyInit(
            context,
            nullptr,
            EVP_sha256(),
            nullptr,
            publicKey
        );

        if (initResult != 1)
        {
            EVP_MD_CTX_free(context);
            return false;
        }

        const auto updateResult = EVP_DigestVerifyUpdate(
            context,
            data.data(),
            data.size()
        );

        if (updateResult != 1)
        {
            EVP_MD_CTX_free(context);
            return false;
        }

        const auto result = EVP_DigestVerifyFinal(
            context,
            reinterpret_cast<const unsigned char *>(signature.data()),
            signature.size()
        );

        EVP_MD_CTX_free(context);

        return result == 1;
    }
}

TEST(RsaTokenSignerTest, SignsDataThatCanBeVerifiedWithPublicKey)
{
    const auto privateKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_private_test.pem";

    const auto publicKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_public_test.pem";

    auth::RsaTokenSigner signer(privateKeyPath);

    const std::string data = "hello";

    const auto signature = signer.sign(data);

    ASSERT_FALSE(signature.empty());

    EVP_PKEY *publicKey = loadPublicKey(publicKeyPath);

    ASSERT_NE(publicKey, nullptr);

    EXPECT_TRUE(
        verifySignature(
            publicKey,
            data,
            signature
        )
    );

    EVP_PKEY_free(publicKey);
}

TEST(RsaTokenSignerTest, SignatureIsInvalidForModifiedData)
{
    const auto privateKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_private_test.pem";

    const auto publicKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_public_test.pem";

    auth::RsaTokenSigner signer(privateKeyPath);

    const std::string data = "hello";

    const auto signature = signer.sign(data);

    ASSERT_FALSE(signature.empty());

    EVP_PKEY *publicKey = loadPublicKey(publicKeyPath);

    ASSERT_NE(publicKey, nullptr);

    EXPECT_FALSE(
        verifySignature(
            publicKey,
            "hello-modified",
            signature
        )
    );

    EVP_PKEY_free(publicKey);
}