#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/evp.h>

#include <exception>
#include <vector>
#include <string>

#include <auth/crypto/RsaPublicKeyBuilder.hpp>
#include <auth/token/RsaTokenSigner.hpp>

namespace {
    std::string makeEven2048BitModulus()
    {
        std::vector<unsigned char> bytes(256, 0);

        bytes.front() = 0x80; // Garantit exactement 2048 bits.
        bytes.back() = 0x00;  // Garantit un nombre pair.

        std::string encoded(4 * ((bytes.size() + 2) / 3) + 1, '\0');

        const int encodedLength = EVP_EncodeBlock(
            reinterpret_cast<unsigned char *>(encoded.data()),
            bytes.data(),
            static_cast<int>(bytes.size())
        );

        if (encodedLength < 0)
            throw std::runtime_error("Failed to encode test modulus");

        encoded.resize(static_cast<std::size_t>(encodedLength));

        std::replace(encoded.begin(), encoded.end(), '+', '-');
        std::replace(encoded.begin(), encoded.end(), '/', '_');

        while (!encoded.empty() && encoded.back() == '=')
            encoded.pop_back();

        return encoded;
    }
}

TEST(RsaPublicKeyBuilderTest, BuildsPublicKeyFromJwk)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw",
        .e = "AQAB"
    };

    auto *publicKey =
        auth::RsaPublicKeyBuilder::build(jwk);

    ASSERT_NE(publicKey, nullptr);
    EVP_PKEY_free(publicKey);
}


TEST(RsaPublicKeyBuilderTest, RejectsInvalidBase64UrlModulus)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "invalid@@@",
        .e = "AQAB"
    };

    EXPECT_THROW(
        auth::RsaPublicKeyBuilder::build(jwk),
        std::exception
    );
}

TEST(RsaPublicKeyBuilderTest, RejectsInvalidBase64UrlExponent)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw",
        .e = "invalid@@@"
    };

    EXPECT_THROW(
        auth::RsaPublicKeyBuilder::build(jwk),
        std::exception
    );
}

TEST(RsaPublicKeyBuilderTest, RejectsExponentEqualToOne)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw",
        .e = "AQ"
    };

    EXPECT_THROW(
        auth::RsaPublicKeyBuilder::build(jwk),
        std::exception
    );
}

TEST(RsaPublicKeyBuilderTest, RejectsZeroModulus)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "AA",
        .e = "AQAB"
    };

    EXPECT_THROW(
        auth::RsaPublicKeyBuilder::build(jwk),
        std::exception
    );
}

TEST(RsaPublicKeyBuilderTest, RejectsModulusSmallerThan2048Bits)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "AQ",
        .e = "AQAB"
    };

    EXPECT_THROW(
        auth::RsaPublicKeyBuilder::build(jwk),
        std::exception
    );
}

TEST(RsaPublicKeyBuilderTest, RejectsEvenExponent)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw",
        .e = "BA"
    };

    EXPECT_THROW(
        auth::RsaPublicKeyBuilder::build(jwk),
        std::exception
    );
}


TEST(RsaPublicKeyBuilderTest, RejectsEvenModulus)
{
    const std::string evenModulus = makeEven2048BitModulus();

    const auto modulusBytes =
        jwt::base::decode<jwt::alphabet::base64url>(
            jwt::base::pad<jwt::alphabet::base64url>(evenModulus)
        );

    ASSERT_EQ(modulusBytes.size(), 256U);
    EXPECT_EQ(
        static_cast<unsigned char>(modulusBytes.front()) & 0x80U,
        0x80U
    );
    EXPECT_EQ(
        static_cast<unsigned char>(modulusBytes.back()) & 1U,
        0U
    );

    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = evenModulus,
        .e = "AQAB"
    };

    EXPECT_THROW(
        auth::RsaPublicKeyBuilder::build(jwk),
        std::exception
    );
}


TEST(RsaPublicKeyBuilderTest, ReturnsUsablePublicKey)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw",
        .e = "AQAB"
    };

    EVP_PKEY *publicKey = auth::RsaPublicKeyBuilder::build(jwk);
    ASSERT_NE(publicKey, nullptr);

    EXPECT_EQ(EVP_PKEY_get_bits(publicKey), 2048);

    EVP_PKEY_free(publicKey);
}

