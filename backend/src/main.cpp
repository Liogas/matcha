#include <crow.h>
#include <crow/middlewares/cors.h>
#include <libpq-fe.h>

#include <chrono>
#include <iostream>
#include <optional>

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
#include <auth/token/TokenValidator.hpp>
#include <auth/token/RsaTokenSignatureVerifier.hpp>

int main()
{
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
        "TON_JWKS_URL",
        std::chrono::seconds{300}
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
        jwksCache
    );

    /*
     * Token validation
     */

    const auth::TokenValidatorConfig tokenValidatorConfig{
        "TON_ISSUER",
        "TON_AUDIENCE",
        "RS256"
    };

    auth::RsaTokenSignatureVerifier signatureVerifier;
    auth::TokenValidator tokenValidator(
        tokenValidatorConfig,
        jwksProvider,
        signatureVerifier
    );

    auth::Authenticator authenticator(
        tokenValidator
    );

    /*
     * HTTP server
     */

    crow::SimpleApp app;

    registerAuthRoutes(
        app,
        authService,
        authenticator
    );

    app
        .port(18080)
        .multithreaded()
        .run();
}