#include <crow.h>
#include <crow/middlewares/cors.h>
#include <libpq-fe.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <sw/redis++/redis++.h>

#include "database/Database.hpp"
#include "database/DatabaseInitializer.hpp"
#include "repositories/AccountRepository.hpp"
#include "security/PasswordHasher.hpp"
#include "security/RedisLoginAttemptLimiter.hpp"
#include "http/AuthRoutes.hpp"
#include "http/ProfileRoutes.hpp"
#include "http/LikeRoutes.hpp"
#include "images/ImageProcessor.hpp"
#include "images/LocalImageStorage.hpp"
#include "repositories/ProfileRepository.hpp"
#include "repositories/LikeRepository.hpp"
#include "services/ProfilePhotoService.hpp"
#include "messaging/MessagingRepository.hpp"
#include "messaging/MessagingRoutes.hpp"
#include "messaging/MessagingWebSocket.hpp"

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

    std::string optionalEnvironmentVariable(
        const char *name,
        const std::string &defaultValue
    )
    {
        const char *value = std::getenv(name);
        return value == nullptr ? defaultValue : value;
    }

    bool optionalBooleanEnvironmentVariable(
        const char *name,
        bool defaultValue
    )
    {
        const char *value = std::getenv(name);
        if (value == nullptr)
            return defaultValue;

        const std::string setting(value);
        if (setting == "true")
            return true;
        if (setting == "false")
            return false;

        throw std::invalid_argument(
            std::string(name) + " must be either 'true' or 'false'"
        );
    }

    std::string requiredSecretFileEnvironmentVariable(const char *name)
    {
        const std::string path = requiredEnvironmentVariable(name);
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error(
                std::string("Unable to read secret file configured by ") +
                name
            );

        std::string secret{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };
        while (!secret.empty() &&
            (secret.back() == '\n' || secret.back() == '\r'))
            secret.pop_back();
        if (secret.empty())
            throw std::runtime_error(
                std::string("Secret file configured by ") +
                name + " is empty"
            );
        return secret;
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
    const std::string redisPassword =
        requiredSecretFileEnvironmentVariable("REDIS_PASSWORD_FILE");
    const std::string smtpUsername =
        optionalEnvironmentVariable("EMAIL_SMTP_USERNAME", "");
    const std::string smtpPassword =
        smtpUsername.empty()
            ? ""
            : requiredSecretFileEnvironmentVariable(
                "EMAIL_SMTP_PASSWORD_FILE"
            );
    const SmtpEmailSender emailSender({
        requiredEnvironmentVariable("EMAIL_SMTP_URL"),
        requiredEnvironmentVariable("EMAIL_FROM"),
        smtpUsername,
        smtpPassword,
        optionalBooleanEnvironmentVariable("EMAIL_SMTP_TLS", false)
    });
    const std::string emailVerificationUrl =
        requiredEnvironmentVariable("EMAIL_VERIFICATION_URL");
    const std::string passwordResetUrl =
        requiredEnvironmentVariable("EMAIL_PASSWORD_RESET_URL");
    const std::string allowedBrowserOrigin =
        requiredEnvironmentVariable("AUTH_ALLOWED_ORIGIN");
    const bool secureRefreshCookie =
        optionalBooleanEnvironmentVariable(
            "AUTH_REFRESH_COOKIE_SECURE",
            false
        );

    Database database;
    DatabaseInitializer databaseInitializer(database);

    if (!databaseInitializer.run())
    {
        std::cerr << "[ERROR BDD] Database initialization failed"
                  << std::endl;

        return 1;
    }

    AccountRepository accountRepository(database);
    ProfileRepository profileRepository(database);
    LikeRepository likeRepository(database);
    MessagingRepository messagingRepository(database);
    for (const auto &[firstAccountId, secondAccountId] :
        likeRepository.listAllMatches())
    {
        if (!messagingRepository.ensureDirectConversation(
                firstAccountId, secondAccountId))
        {
            throw std::runtime_error(
                "Unable to synchronize a match conversation");
        }
    }
    MessagingWebSocketHub messagingWebSocketHub;
    ImageProcessor imageProcessor(
        10 * 1024 * 1024,
        2048,
        82
    );
    LocalImageStorage imageStorage("/var/lib/matcha/uploads");
    ProfilePhotoService profilePhotoService(
        profileRepository,
        imageProcessor,
        imageStorage
    );
    if (optionalBooleanEnvironmentVariable(
            "MATCHA_CLEANUP_ORPHANED_IMAGES", false))
    {
        profilePhotoService.cleanupOrphanedFiles();
        std::cout << "[INFO STORAGE] Orphaned profile images cleaned"
            << std::endl;
    }
    auth::PasswordHasher passwordHasher;

    auth::AuthService authService(
        accountRepository,
        passwordHasher
    );

    sw::redis::ConnectionOptions redisConnectionOptions;
    redisConnectionOptions.host =
        requiredEnvironmentVariable("REDIS_HOST");
    redisConnectionOptions.port = std::stoi(
        requiredEnvironmentVariable("REDIS_PORT")
    );
    redisConnectionOptions.password = redisPassword;
    redisConnectionOptions.connect_timeout =
        std::chrono::milliseconds{500};
    redisConnectionOptions.socket_timeout =
        std::chrono::milliseconds{1000};

    sw::redis::ConnectionPoolOptions redisPoolOptions;
    redisPoolOptions.size = 16;
    redisPoolOptions.wait_timeout =
        std::chrono::milliseconds{1000};
    sw::redis::Redis redis(
        redisConnectionOptions,
        redisPoolOptions
    );
    redis.ping();
    RedisLoginAttemptLimiter loginAttemptLimiter(
        redis,
        5,
        std::chrono::minutes{15}
    );

    /*
     * JWKS
     */

    auth::HttpClientConfig httpClientConfig{std::nullopt};
    httpClientConfig.allowInsecureHttp =
        optionalBooleanEnvironmentVariable(
            "JWT_ALLOW_INSECURE_HTTP",
            false
        );

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

    AuthApp app;
    auto &cors = app.get_middleware<AuthCorsHandler>();
    cors.allowedOrigin = allowedBrowserOrigin;
    cors
        .global()
        .origin(allowedBrowserOrigin)
        .methods(
            crow::HTTPMethod::Get,
            crow::HTTPMethod::Post,
            crow::HTTPMethod::Put,
            crow::HTTPMethod::Patch,
            crow::HTTPMethod::Delete,
            crow::HTTPMethod::Options
        )
        .headers("Authorization", "Content-Type")
        .allow_credentials();

    registerJwksRoute(app, tokenSigner);
    registerAuthRoutes(
        app,
        authService,
        authenticator,
        tokenGenerator,
        loginAttemptLimiter,
        accountRepository,
        passwordHasher,
        emailSender,
        emailVerificationUrl,
        passwordResetUrl,
        allowedBrowserOrigin,
        secureRefreshCookie
    );
    registerProfileRoutes(
        app,
        authenticator,
        profileRepository,
        profilePhotoService
    );
    registerLikeRoutes(
        app,
        authenticator,
        likeRepository,
        profileRepository,
        messagingRepository
    );
    registerMessagingRoutes(
        app,
        authenticator,
        messagingRepository
    );
    registerMessagingWebSocket(
        app,
        authenticator,
        messagingRepository,
        messagingWebSocketHub
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