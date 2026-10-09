#include <jwt-cpp/jwt.h>
#include <gtest/gtest.h>

#include <auth/token/RsaTokenSigner.hpp>
#include <auth/token/RsaTokenSignatureVerifier.hpp>

TEST(RsaTokenSignatureVerifierTest, VerifiesValidSignature)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw",
        .e = "AQAB"
    };

	const auto privateKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_private_test.pem";

    auth::RsaTokenSigner signer(
        privateKeyPath,
        "test-key"
    );

    const std::string data = "hello.jwt";

    const std::string encodedSignature =
        signer.sign(data);

    const std::string signature =
        jwt::base::decode<jwt::alphabet::base64url>(
            jwt::base::pad<jwt::alphabet::base64url>(encodedSignature)
        );

    auth::RsaTokenSignatureVerifier verifier;

    EXPECT_TRUE(
        verifier.verify(
            data,
            signature,
            jwk
        )
    );
}

TEST(RsaTokenSignatureVerifierTest, RejectsModifiedData)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw",
        .e = "AQAB"
    };

    const auto privateKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_private_test.pem";

    auth::RsaTokenSigner signer(
        privateKeyPath,
        "test-key"
    );

    const std::string data = "hello.jwt";

    const std::string encodedSignature =
        signer.sign(data);

    const std::string signature =
        jwt::base::decode<jwt::alphabet::base64url>(
            jwt::base::pad<jwt::alphabet::base64url>(encodedSignature)
        );

    auth::RsaTokenSignatureVerifier verifier;

    EXPECT_FALSE(
        verifier.verify(
            "hello.modified",
            signature,
            jwk
        )
    );
}

TEST(RsaTokenSignatureVerifierTest, RejectsModifiedSignature)
{
    auth::Jwk jwk{
        .kid = "test-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "n1z9S3kUIxwJ33ZWT2fAGiUuEszJMbpmzWZfRitpxYlrfmD0SLOUhIkygTbCgzThHBNAd_UWLw1vr4vRnxL5_TMUrWZgGPZdlTp84hw0QKuTDo-xLYYSDdS585M7EsHpNU8A7uttAPS4D7910dhGrjVdtOrcldljbafGrNk48TZnCUasfsS4uiPtHhMUQBObXn17lEQqrn-I676Yyx5dXeHvm7tCMbMi44rYhoC40N4W7gGA9UMQLhCiTU7Kj2PHVtdaIRxRYkaqIXytDiYbeHcRcuB0M0m58cPHp64D_4P_wwJwhB00zXfLhE2j47TuS9uAYSqCMtmKbFNN2DXUfw",
        .e = "AQAB"
    };

    const auto privateKeyPath =
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/fixtures/rsa_private_test.pem";

    auth::RsaTokenSigner signer(
        privateKeyPath,
        "test-key"
    );

    const std::string data = "hello.jwt";

    const std::string encodedSignature =
        signer.sign(data);

    const std::string signature =
        jwt::base::decode<jwt::alphabet::base64url>(
            jwt::base::pad<jwt::alphabet::base64url>(encodedSignature)
        );

    std::string invalidSignature = signature;
    invalidSignature[0] =
        invalidSignature[0] == 'A' ? 'B' : 'A';

    auth::RsaTokenSignatureVerifier verifier;

    EXPECT_FALSE(
        verifier.verify(
            data,
            invalidSignature,
            jwk
        )
    );
}

TEST(RsaTokenSignatureVerifierTest, RejectsMalformedJwk)
{
    auth::Jwk jwk{
        .kid = "invalid-key",
        .kty = "RSA",
        .alg = "RS256",
        .n = "invalid-modulus",
        .e = "AQAB"
    };

    auth::RsaTokenSignatureVerifier verifier;

    EXPECT_FALSE(
        verifier.verify(
            "hello.jwt",
            "invalid-signature",
            jwk
        )
    );
}