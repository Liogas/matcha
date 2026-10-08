#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bn.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <auth/token/RsaTokenSigner.hpp>

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

TEST(RsaPublicKeyBuilderTest, DecodesJwkModulus)
{
    const std::string n =
        "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw";

    std::cout << "n size = " << n.size() << '\n';
    std::cout << "n mod 4 = " << n.size() % 4 << '\n';

    EXPECT_EQ(n.size(), 342);

    // std::string padded = n;

    // while (padded.size() % 4 != 0)
    //     padded += '=';

    const auto bytes =
    jwt::base::decode<jwt::alphabet::base64url>(
        jwt::base::pad<jwt::alphabet::base64url>(n)
    );

    EXPECT_EQ(bytes.size(), 256);
}


TEST(RsaTokenSignerTest, SignsDataThatCanBeVerifiedWithPublicKey)
{
    const auto privateKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_private_test.pem";

    const auto publicKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_public_test.pem";

    auth::RsaTokenSigner signer(
        privateKeyPath,
        "key-123"
    );

    EXPECT_EQ(signer.keyId(), "key-123");

    const std::string data = "hello";

    const auto signature = signer.sign(data);

    ASSERT_FALSE(signature.empty());

    EVP_PKEY *publicKey = loadPublicKey(publicKeyPath);

    ASSERT_NE(publicKey, nullptr);

    const auto decodedSignature =
        jwt::base::decode<jwt::alphabet::base64url>(
            signature
        );

    EXPECT_TRUE(
        verifySignature(
            publicKey,
            data,
            decodedSignature
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

    auth::RsaTokenSigner signer(
        privateKeyPath,
        "key-123"
    );

    const std::string data = "hello";

    const auto signature = signer.sign(data);

    ASSERT_FALSE(signature.empty());

    EVP_PKEY *publicKey = loadPublicKey(publicKeyPath);

    ASSERT_NE(publicKey, nullptr);

    const auto decodedSignature =
        jwt::base::decode<jwt::alphabet::base64url>(
            signature
        );

    EXPECT_FALSE(
        verifySignature(
            publicKey,
            "hello-modified",
            decodedSignature
        )
    );

    EVP_PKEY_free(publicKey);
}