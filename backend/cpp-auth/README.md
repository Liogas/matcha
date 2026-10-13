# cpp-auth

`cpp-auth` is a C++ library used by the Matcha backend. It provides RS256 JWT
generation and validation, and retrieves public signing keys from a JWKS
endpoint. It is not a standalone HTTP service and does not define application
routes.

## Supported token profile

- Signing and validation use **RS256 only**.
- A signing key must be a private RSA key of at least 2048 bits. The key is
  loaded and checked when `RsaTokenSigner` is constructed, then held in memory
  until that signer is destroyed.
- Generated tokens contain `iss`, `sub`, `aud`, `iat`, `exp`, and a `kid` in
  the header. `sub` must be non-empty.
- The generator requires a non-empty issuer and audience and a positive
  lifetime of no more than 3600 seconds.
- The validator requires `exp` and `iat`; `nbf` is optional. The default
  maximum lifetime is one hour and the default clock skew allowance is 60
  seconds. Issuer, audience, timestamps, algorithm, key ID, and signature are
  validated before claims are returned.
- JWT payloads are **signed, not encrypted**. Their claims can be read by
  anyone who has the token; do not put secrets in them.

## Generate a token

The following snippet shows the generator's dependencies. Provide the private
key through deployment configuration or a secret mount; do not commit a
production private key.

```cpp
#include <auth/identity/AuthenticatedIdentity.hpp>
#include <auth/time/SystemWallClock.hpp>
#include <auth/token/RsaTokenSigner.hpp>
#include <auth/token/TokenGenerator.hpp>

#include <chrono>

auth::SystemWallClock wallClock;
auth::RsaTokenSigner signer("/run/secrets/jwt-private.pem", "matcha-key-1");
auth::TokenGenerator generator(
    {"https://matcha.example", "matcha-api", std::chrono::minutes{15}},
    signer,
    wallClock
);

const auth::AuthenticatedIdentity identity{
    "user-123",
    "",
    {}
};
const std::string token = generator.generate(identity);
```

The issuer and audience come from `TokenGeneratorConfig`; the identity's
`subject` supplies the token subject.

## Validate a token from JWKS

```cpp
#include <auth/crypto/RsaPublicKeyBuilder.hpp>
#include <auth/http/CppHttpClient.hpp>
#include <auth/http/HttpClientConfig.hpp>
#include <auth/jwks/HttpJwksConfig.hpp>
#include <auth/jwks/HttpJwksProvider.hpp>
#include <auth/jwks/JwksCache.hpp>
#include <auth/jwks/JsonJwksParser.hpp>
#include <auth/time/SystemClock.hpp>
#include <auth/time/SystemWallClock.hpp>
#include <auth/token/RsaTokenSignatureVerifier.hpp>
#include <auth/token/TokenValidator.hpp>

#include <chrono>
#include <optional>

auth::HttpClientConfig httpConfig{std::nullopt};
auth::CppHttpClient httpClient(httpConfig);
auth::JsonJwksParser jwksParser;
auth::SystemClock cacheClock;
auth::JwksCache jwksCache(std::chrono::minutes{5}, cacheClock);
auth::HttpJwksConfig jwksConfig{
    "https://identity.example/.well-known/jwks.json",
    std::chrono::minutes{5},
    std::chrono::seconds{30}
};
auth::HttpJwksProvider jwksProvider(
    jwksConfig,
    httpClient,
    jwksParser,
    jwksCache,
    cacheClock
);

auth::SystemWallClock wallClock;
auth::RsaTokenSignatureVerifier signatureVerifier;
auth::TokenValidator validator(
    {"https://identity.example", "matcha-api", "RS256"},
    jwksProvider,
    signatureVerifier,
    wallClock
);

const auth::TokenValidationResult result = validator.validate(token);
if (result.status == auth::TokenValidationResult::Status::Valid) {
    const std::string subject = result.claims->subject;
}
```

Treat `InvalidToken` as an authentication failure. `VerificationUnavailable`
means validation could not be completed because key verification was
unavailable; do not treat it as a valid token.

## JWKS and HTTP behavior

- The parser keeps RSA/RS256 keys and ignores keys for other key types or
  explicitly unsupported algorithms. If `alg` is absent on an RSA key,
  `cpp-auth` applies its fixed RS256 policy.
- A malformed supported RSA key, duplicate supported `kid`, or JWKS with no
  supported key makes the document invalid.
- An unknown `kid` can trigger a JWKS refresh, with a default minimum interval
  of 30 seconds between attempts. Cache entries default to five minutes in the
  example above.
- HTTP defaults are 2 seconds to connect, 5 seconds to read/write, and a 64
  KiB maximum response. HTTPS certificate verification is enabled; by default
  system trust roots are used. Set `HttpClientConfig::caCertPath` when a
  private CA must be trusted. Do not disable certificate verification in
  production.
- The URL parser accepts lowercase `http`/`https` URLs with DNS-style host
  names or IPv4 host strings,
  optional numeric ports from 1 through 65535, paths, and queries. It rejects
  user information and IPv6 literals. URL fragments are discarded because
  they are not sent in HTTP requests. Use HTTPS for production JWKS endpoints.

## Tests

The library tests are built as the `cpp-auth_tests` target. From a configured
CMake build directory, build and run them with:

```sh
cmake --build <build-dir> --target cpp-auth_tests matcha_backend -j2
ctest --test-dir <build-dir> --output-on-failure
```
