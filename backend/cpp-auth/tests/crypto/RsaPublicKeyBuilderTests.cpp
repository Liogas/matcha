#include <gtest/gtest.h>
#include <openssl/evp.h>

#include <auth/crypto/RsaPublicKeyBuilder.hpp>
#include <auth/token/RsaTokenSigner.hpp>

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