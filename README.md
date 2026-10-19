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
- `JWT_JWKS_URL` : URL du JWKS contenant la clé RSA publique associée au
  `kid`. HTTPS est requis hors développement local.
- `JWT_ALLOW_INSECURE_HTTP` : option facultative, `false` par défaut ; à
  mettre à `true` uniquement pour le développement local sans TLS.
- `JWT_KEY_ID` : identifiant de cette clé dans le JWKS.
- `JWT_PRIVATE_KEY_FILE` : chemin côté hôte du fichier PEM RSA privé d’au
  moins 2048 bits. Compose le monte en lecture seule comme secret.

Le backend publie la clé publique correspondante sur
`GET /.well-known/jwks.json`. Le document expose uniquement les paramètres
publics RSA (`kid`, `n` et `e`) ; la clé privée n’est jamais incluse. En
production, `JWT_JWKS_URL` doit pointer vers ce endpoint via HTTPS. Dans le
réseau Compose pour le développement local, l’URL peut être
`http://backend:18080/.well-known/jwks.json` avec
`JWT_ALLOW_INSECURE_HTTP=true`.

Ne versionnez ni le fichier `.env` ni la clé privée. La route
`GET /api/auth/me` accepte le token retourné au login dans l’en-tête
`Authorization: Bearer <token>` et vérifie sa signature par le JWKS configuré.
Elle répond `503` si la vérification est temporairement indisponible et `401`
si le token est invalide.

En développement, Nginx est le point d’entrée HTTP sur le port `18080` et
limite les requêtes `POST /api/auth/login` à cinq par minute et par adresse
IP, avec un burst de cinq requêtes. Les requêtes au-delà de cette limite
reçoivent `429 Too Many Requests`. Cette limite est locale à l’instance Nginx ;
elle protège contre les rafales par IP, mais ne remplace pas une limitation
partagée par compte dans l’application lorsque plusieurs instances sont
déployées.

Le backend complète ce garde-fou avec un compteur Redis atomique : cinq
tentatives par combinaison compte + IP sur quinze minutes. Une tentative
supplémentaire reçoit `429` et l’en-tête `Retry-After`. Une connexion réussie
réinitialise le compteur de cette combinaison. Si Redis est indisponible, le
login répond `503` plutôt que de désactiver silencieusement la protection.

Pour le développement local, crée le secret Redis et ajoute son chemin côté
hôte au `.env` non versionné :

```sh
mkdir -p secrets
umask 077
openssl rand -hex 32 > secrets/redis-password
chmod 600 secrets/redis-password
```

Ajoute ensuite `REDIS_PASSWORD_FILE=./secrets/redis-password` dans `.env`.
Compose monte le secret dans les conteneurs ; le mot de passe lui-même ne doit
pas être placé dans `.env` ni versionné. Le service Redis est privé au réseau
Compose et n’active pas la persistance, puisque ses compteurs sont temporaires.
Les tests du limiteur Redis peuvent être lancés après le démarrage des services
avec `docker compose exec -T backend ctest --test-dir /app/build -R RedisLoginAttemptLimiterTest --output-on-failure`.