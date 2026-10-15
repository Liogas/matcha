#include <crow.h>
#include <crow/middlewares/cors.h>
#include <libpq-fe.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "database/Database.hpp"
#include "database/DatabaseInitializer.hpp"
#include "repositories/UserRepository.hpp"
#include "security/PasswordHasher.hpp"
#include "http/AuthRoutes.hpp"

#include <auth/auth/Authenticator.hpp>
#include <auth/http/CppHttpClient.hpp>
#include <auth/http/HttpClientConfig.hpp>
#include <auth/jwks/HttpJwksConfig.hpp>
#include <auth/jwks/HttpJwksProvider.hpp>
#include <auth/jwks/JwksCache.hpp>
#include <auth/jwks/JsonJwksParser.hpp>
#include <auth/time/SystemClock.hpp>
#include <auth/time/SystemWallClock.hpp>
#include <auth/token/TokenValidator.hpp>
#include <auth/token/RsaTokenSignatureVerifier.hpp>
#include <auth/token/TokenGenerator.hpp>
#include <auth/token/RsaTokenSigner.hpp>

namespace
{
    std::string requiredEnvironmentVariable(const char *name)
    {
        const char *value = std::getenv(name);
        if (value == nullptr || *value == '\0')
            throw std::runtime_error(
                std::string("Required environment variable is not set: ") +
                name
            );
        return value;
    }
}

int main()
try
{
    const std::string issuer =
        requiredEnvironmentVariable("JWT_ISSUER");
    const std::string audience =
        requiredEnvironmentVariable("JWT_AUDIENCE");
    const std::string jwksUrl =
        requiredEnvironmentVariable("JWT_JWKS_URL");
    const std::string privateKeyPath =
        requiredEnvironmentVariable("JWT_PRIVATE_KEY_PATH");
    const std::string keyId =
        requiredEnvironmentVariable("JWT_KEY_ID");

    Database database;
    DatabaseInitializer databaseInitializer(database);

    if (!databaseInitializer.run())
    {
        std::cerr << "[ERROR BDD] Database initialization failed"
                  << std::endl;

        return 1;
    }

    UserRepository userRepository(database);
    auth::PasswordHasher passwordHasher;

    auth::AuthService authService(
        userRepository,
        passwordHasher
    );

    /*
     * JWKS
     */

    const auth::HttpClientConfig httpClientConfig{
        std::nullopt
    };

    const auth::HttpJwksConfig jwksConfig{
        jwksUrl,
        std::chrono::seconds{300},
        std::chrono::seconds{30}
    };

    auth::CppHttpClient httpClient(
        httpClientConfig
    );

    auth::JsonJwksParser jwksParser;

    auth::SystemClock clock;

    auth::JwksCache jwksCache(
        jwksConfig.cacheTtl,
        clock
    );

    auth::HttpJwksProvider jwksProvider(
        jwksConfig,
        httpClient,
        jwksParser,
        jwksCache,
        clock
    );

    /*
     * Token validation
     */

    const auth::TokenValidatorConfig tokenValidatorConfig{
        issuer,
        audience,
        "RS256"
    };

    auth::SystemWallClock wallClock;
    auth::RsaTokenSignatureVerifier signatureVerifier;
    auth::TokenValidator tokenValidator(
        tokenValidatorConfig,
        jwksProvider,
        signatureVerifier,
        wallClock
    );

    auth::Authenticator authenticator(
        tokenValidator
    );

    auth::RsaTokenSigner tokenSigner(
        privateKeyPath,
        keyId
    );
    auth::TokenGenerator tokenGenerator(
        {issuer, audience, std::chrono::minutes{15}},
        tokenSigner,
        wallClock
    );

    /*
     * HTTP server
     */

    crow::SimpleApp app;

    registerAuthRoutes(
        app,
        authService,
        authenticator,
        tokenGenerator
    );

    app
        .port(18080)
        .multithreaded()
        .run();
}
catch (const std::exception &error)
{
    std::cerr << "[STARTUP ERROR] " << error.what() << std::endl;
    return 1;
}