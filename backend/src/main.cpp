#include <crow.h>
#include <crow/middlewares/cors.h>
#include <libpq-fe.h>

#include <chrono>
#include <iostream>
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

#include <core/ConfigReader.hpp>
#include <core/Logger.hpp>

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

int run(core::ILogger &logger)
{
	const core::SystemEnvironment environment;
	core::ConfigReader config(environment);

	const std::string issuer = config.required("JWT_ISSUER");
	const std::string audience = config.required("JWT_AUDIENCE");
	const std::string jwksUrl = config.required("JWT_JWKS_URL");
	const std::string privateKeyPath = config.required("JWT_PRIVATE_KEY_PATH");
	const std::string keyId = config.required("JWT_KEY_ID");
	const bool allowInsecureJwksHttp =
		config.optionalBool("JWT_ALLOW_INSECURE_HTTP", false);
	const std::string redisHost = config.required("REDIS_HOST");
	const int redisPort = config.requiredInt("REDIS_PORT");
	const std::string redisPassword = config.requiredSecret("REDIS_PASSWORD");
	const std::string smtpUrl = config.required("EMAIL_SMTP_URL");
	const std::string emailFrom = config.required("EMAIL_FROM");
	const std::string smtpUsername = config.optional("EMAIL_SMTP_USERNAME", "");
	const std::string smtpPassword = smtpUsername.empty()
										 ? ""
										 : config.requiredSecret("EMAIL_SMTP_PASSWORD");
	const bool smtpTls = config.optionalBool("EMAIL_SMTP_TLS", false);
	const std::string emailVerificationUrl =
		config.required("EMAIL_VERIFICATION_URL");
	const std::string passwordResetUrl =
		config.required("EMAIL_PASSWORD_RESET_URL");
	const std::string allowedBrowserOrigin =
		config.required("AUTH_ALLOWED_ORIGIN");
	const bool secureRefreshCookie =
		config.optionalBool("AUTH_REFRESH_COOKIE_SECURE", false);
	const bool cleanupOrphanedImages =
		config.optionalBool("MATCHA_CLEANUP_ORPHANED_IMAGES", false);

	if (const auto checked = config.finish(); !checked)
	{
		logger.error("Invalid configuration", {{"errors", checked.error().message}});
		return 1;
	}

	const SmtpEmailSender emailSender({smtpUrl,
									   emailFrom,
									   smtpUsername,
									   smtpPassword,
									   smtpTls});

	Database database;
	DatabaseInitializer databaseInitializer(database);

	if (!databaseInitializer.run())
	{
		logger.error("Database initialization failed");
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
		82);
	LocalImageStorage imageStorage("/var/lib/matcha/uploads");
	ProfilePhotoService profilePhotoService(
		profileRepository,
		imageProcessor,
		imageStorage);
	if (cleanupOrphanedImages)
	{
		profilePhotoService.cleanupOrphanedFiles();
		logger.info("Orphaned profile images cleaned");
	}
	auth::PasswordHasher passwordHasher;

	auth::AuthService authService(
		accountRepository,
		passwordHasher);

	sw::redis::ConnectionOptions redisConnectionOptions;
	redisConnectionOptions.host = redisHost;
	redisConnectionOptions.port = redisPort;
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
		redisPoolOptions);
	redis.ping();
	RedisLoginAttemptLimiter loginAttemptLimiter(
		redis,
		5,
		std::chrono::minutes{15});

	/*
	 * JWKS
	 */

	auth::HttpClientConfig httpClientConfig{std::nullopt};
	httpClientConfig.allowInsecureHttp = allowInsecureJwksHttp;

	const auth::HttpJwksConfig jwksConfig{
		jwksUrl,
		std::chrono::seconds{300},
		std::chrono::seconds{30}};

	auth::CppHttpClient httpClient(
		httpClientConfig);

	auth::JsonJwksParser jwksParser;

	auth::SystemClock clock;

	auth::JwksCache jwksCache(
		jwksConfig.cacheTtl,
		clock);

	auth::HttpJwksProvider jwksProvider(
		jwksConfig,
		httpClient,
		jwksParser,
		jwksCache,
		clock);

	/*
	 * Token validation
	 */

	const auth::TokenValidatorConfig tokenValidatorConfig{
		issuer,
		audience,
		"RS256"};

	auth::SystemWallClock wallClock;
	auth::RsaTokenSignatureVerifier signatureVerifier;
	auth::TokenValidator tokenValidator(
		tokenValidatorConfig,
		jwksProvider,
		signatureVerifier,
		wallClock);

	auth::Authenticator authenticator(
		tokenValidator);

	auth::RsaTokenSigner tokenSigner(
		privateKeyPath,
		keyId);
	auth::TokenGenerator tokenGenerator(
		{issuer, audience, std::chrono::minutes{15}},
		tokenSigner,
		wallClock);

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
			crow::HTTPMethod::Options)
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
		secureRefreshCookie);
	registerProfileRoutes(
		app,
		authenticator,
		profileRepository,
		profilePhotoService);
	registerLikeRoutes(
		app,
		authenticator,
		likeRepository,
		profileRepository,
		messagingRepository);
	registerMessagingRoutes(
		app,
		authenticator,
		messagingRepository);
	registerMessagingWebSocket(
		app,
		authenticator,
		messagingRepository,
		messagingWebSocketHub);

	logger.info("Server starting", {{"port", "18080"}});
	app
		.port(18080)
		.multithreaded()
		.run();
	return 0;
}

int main()
{
	core::StreamLogger logger(std::cerr, core::LogLevel::Info);
	try
	{
		return run(logger);
	}
	catch (const std::exception &error)
	{
		logger.error("Startup failed", {{"reason", error.what()}});
		return 1;
	}
}