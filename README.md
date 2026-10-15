CROW        -> Bibliotheque pour créer un serveur WEB
LIBPQ       -> Bibliotheque pour communiquer avec une BDD POSTGRESQL
GTEST       -> Google Test
LIBSODIUM   -> Hash de mot de passe

## Authentification JWT du backend

Le backend émet des JWT RS256 valides 15 minutes après une connexion réussie.
Il nécessite les variables suivantes dans l’environnement Docker Compose (par
exemple dans un fichier `.env` non versionné) :

- `JWT_ISSUER` : issuer attendu et émis dans les tokens.
- `JWT_AUDIENCE` : audience attendue et émise.
- `JWT_JWKS_URL` : URL HTTPS publique du JWKS contenant la clé RSA publique
  associée au `kid`.
- `JWT_KEY_ID` : identifiant de cette clé dans le JWKS.
- `JWT_PRIVATE_KEY_FILE` : chemin côté hôte du fichier PEM RSA privé d’au
  moins 2048 bits. Compose le monte en lecture seule comme secret.

Ne versionnez ni le fichier `.env` ni la clé privée. La route
`GET /api/auth/me` accepte le token retourné au login dans l’en-tête
`Authorization: Bearer <token>` et vérifie sa signature par le JWKS configuré.
Elle répond `503` si la vérification est temporairement indisponible et `401`
si le token est invalide.