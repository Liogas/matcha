# Matcha

Application de rencontre : backend C++ (Crow, PostgreSQL, Redis) et frontend
React posé sur une clairière en 3D qui vit au rythme du jour et des saisons.
Pages, décor, composants et architecture du frontend :
[`frontend/README.md`](frontend/README.md).

## Démarrage

Prérequis : Docker (ou Podman) avec Compose, `make` et `openssl`.

```sh
make build     # dev, premier lancement (construit les images)
make up        # dev, lancements suivants
make rebuild   # recompile le backend sans relancer le conteneur
make prod      # image de production (binaire seul, sans code source)
```

Fonctionnement du build multi-étapes et du mode dev :
[`docs/docker-backend.md`](docs/docker-backend.md). Toutes les fiches et guides :
[`docs/`](docs/README.md).

Avant chaque lancement, `make secrets` (appelé automatiquement par `build` et
`up`) exécute `scripts/init-secrets.sh`, qui :

- crée `.env` à partir de `.env.example` s’il n’existe pas, et y ajoute les
  variables apparues depuis dans `.env.example` ;
- génère dans `./secrets/` (droits `600`) les secrets manquants, sans jamais
  écraser un secret existant :
  - `db-password` : mot de passe PostgreSQL ;
  - `redis-password` : mot de passe Redis ;
  - `jwt-private.pem` : clé RSA 2048 bits de signature des JWT.

`.env` ne contient que des réglages et des **chemins** vers ces fichiers ;
Compose les monte dans `/run/secrets/` et aucun mot de passe n’apparaît dans
`docker-compose.yml` ni dans l’environnement des conteneurs. `.env` et
`secrets/` sont ignorés par Git.

PostgreSQL n’initialise son mot de passe qu’à la création du volume
`postgres_data`. Si `secrets/db-password` est régénéré alors que le volume
existe déjà, il faut soit supprimer le volume (`docker compose down -v`, perte
des données), soit appliquer le nouveau mot de passe :

```sh
docker compose exec database sh -c \
  'psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" -c "ALTER USER \"$POSTGRES_USER\" PASSWORD '"'"'$(cat /run/secrets/db_password)'"'"'"'
```

En production, le mot de passe SMTP suit le même principe
(`EMAIL_SMTP_PASSWORD_FILE`).

## Bibliothèques du backend

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

Le mot de passe Redis est lu depuis le secret monté par Compose (voir
[Démarrage](#démarrage)).
Le mot de passe lui-même ne doit pas être placé dans `.env` ni versionné. Le service Redis est privé au réseau
Compose et n’active pas la persistance, puisque ses compteurs sont temporaires.
Les tests du limiteur Redis peuvent être lancés après le démarrage des services
en dev avec `make test`, ou seulement ceux-ci avec `make shell` puis `ctest --test-dir build -R RedisLoginAttemptLimiterTest --output-on-failure`.

## Modèle de données compte et profil

La migration `002_create_account_and_profile_schema.sql` prépare la séparation
entre `auth_accounts` (email, hash du mot de passe et validation de l’adresse)
et `matcha_profiles` (identité et données propres à Matcha). Elle ajoute aussi
les tables de tokens de vérification/réinitialisation, sessions et refresh
tokens, ainsi que les intérêts et photos. Les tokens sensibles sont représentés
par leur hash en base.

Cette migration est additive : elle ne supprime pas la table `users` historique
ni ses données. La remise à zéro complète de la base de développement a été
retenue, mais sera effectuée lors de la bascule du backend vers le nouveau
parcours, avec une sauvegarde et une vérification explicite avant exécution.

`AccountRepository::createPendingAccountAndProfile` crée le compte non vérifié,
son profil Matcha incomplet et le token de vérification dans une seule
instruction SQL atomique. Ainsi, si l’email ou le nom d’utilisateur est déjà
pris, aucun compte ni token orphelin n’est conservé. Le token brut est aléatoire
et seul son hash est stocké en base.

La route `POST /api/auth/register` accepte actuellement `email`, `username`,
`first_name`, `last_name` et `password`. Elle normalise l’email en minuscules,
valide les formats convenus, hache le mot de passe avec libsodium, puis crée
le compte non vérifié, son profil incomplet et un token valable 24 heures. Elle
envoie le lien par SMTP et répond `409` si l’email ou le nom d’utilisateur est
déjà pris. Mailpit capture ces emails en local sur `http://localhost:8025`.

Le transport SMTP est configuré avec `EMAIL_SMTP_URL`, `EMAIL_FROM`,
`EMAIL_SMTP_TLS` et, si une authentification est requise,
`EMAIL_SMTP_USERNAME` et `EMAIL_SMTP_PASSWORD_FILE`. Hors Mailpit local, le
transport refuse de démarrer si TLS n’est pas activé. En production, le mot de
passe SMTP doit être monté comme secret et non enregistré en clair dans `.env`.
En développement, Mailpit ne délivre pas les messages à de vrais destinataires.

Le lien ouvre la page React `/verify-email`, puis un clic explicite envoie le
token au endpoint `POST /api/auth/email/verify`. Ce choix évite qu’un robot qui
analyse les liens dans les emails ne consomme automatiquement le token. Le
backend vérifie alors son hash, sa date d’expiration et son état non consommé
avant de marquer le compte vérifié. Une deuxième utilisation est refusée.

Un compte en attente peut demander un nouvel email via
`POST /api/auth/email/resend` avec `{"email":"..."}`. La réponse ne révèle pas
si l’adresse existe ou si elle est déjà vérifiée. Un nouvel email n’est émis
qu’au plus une fois par minute pour le compte, et Nginx limite aussi ce endpoint
à une requête environ toutes les 30 secondes par IP. Chaque renvoi invalide
l’ancien lien. Ces limites réduisent les abus mais ne remplacent pas un système
anti-spam complet pour une mise en production.

Le login utilise maintenant `auth_accounts` via le dépôt de comptes et ne
retourne que les comptes dont l’adresse a été vérifiée. Les emails sont lus
sans distinction de casse. Dans `cpp-auth`, `IUserLookupRepository` sépare la
lecture nécessaire à l’authentification de l’interface de création
`IUserRepository` ; cela permet de connecter le login au nouveau stockage sans
réutiliser l’ancien chemin de création de comptes.
Les comptes conservés uniquement dans l’ancienne table `users` ne peuvent
donc plus ouvrir de session avec ce flux. Le renvoi d’un email de vérification
permet de réessayer après un échec de livraison, une fois la limite de renvoi
écoulée.

### Profils de développement

Le seed `backend/seeds/development_profiles.sql` ajoute dix profils vérifiés
avec des villes, âges, genres, orientations, descriptions et intérêts variés.
Elle est destinée à la base locale de développement et ne doit pas être
appliquée à une base de production. Tous ces comptes utilisent le mot de passe
`MatchaTest123!` ; leurs adresses utilisent le domaine réservé
`example.test`.
Il n’est pas exécuté automatiquement. Pour le charger explicitement, depuis la
racine du projet, utiliser :

```sh
sh backend/seeds/seed-development.sh
```

Après une connexion réussie, le backend crée une session persistée en base :
le JWT d’accès dure 15 minutes et le refresh token opaque est envoyé dans un
cookie `HttpOnly`, `SameSite=Strict`, limité au chemin `/api/auth`. Le serveur
ne conserve que son hash. Une session expire après 7 jours sans activité ou
au plus tard après 30 jours. `POST /api/auth/refresh` fait tourner le refresh

### Likes et matchs

Le backend expose les actions de découverte suivantes pour les profils complets :

```http
POST   /api/profiles/{accountId}/like
DELETE /api/profiles/{accountId}/like
GET    /api/likes
GET    /api/likes/received
GET    /api/matches
```

Toutes ces routes nécessitent un JWT valide. Un utilisateur ne peut pas se
liker lui-même, liker un profil incomplet ou créer un doublon. Un like
réciproque crée automatiquement un match unique, identifié par `match_id`
dans la réponse du `POST`. La suppression d'un like supprime également le
match associé entre les deux comptes.

### Nettoyage des images orphelines

Les opérations de remplacement et de suppression nettoient normalement les
fichiers associés. Pour supprimer explicitement les fichiers présents dans le
volume mais absents des métadonnées PostgreSQL, activez temporairement :

```sh
MATCHA_CLEANUP_ORPHANED_IMAGES=true make up   # ou make prod
```

puis relancez normalement (`make up`) pour désactiver le nettoyage.

Cette opération parcourt uniquement le volume d'uploads dédié et doit être
effectuée après le chargement éventuel des seeds.
token et renvoie un nouvel access token ; la réutilisation d’un refresh token
déjà consommé révoque la session concernée.

### Module de messagerie

La messagerie est isolée dans `backend/src/messaging` et ne dépend pas du
domaine Matcha. Elle manipule uniquement des conversations, des participants
et des messages. Matcha l’intègre en créant une conversation directe lorsqu’un
match est créé et en la supprimant lorsqu’un match est retiré.

Les routes génériques exposées sont :

```http
GET  /api/conversations
GET  /api/conversations/{conversationId}/messages?limit=50&before={messageId}
POST /api/conversations/{conversationId}/messages
POST /api/conversations/{conversationId}/read
```

L’accès est limité aux participants de la conversation. Les messages sont
limités à 2 000 caractères, paginés par curseur et soumis à une limite de
30 messages par minute et par expéditeur.

Les notifications temps réel utilisent également :

```text
WebSocket /api/messaging/ws?access_token={accessToken}
```

Après l’ouverture, le client peut envoyer un objet JSON contenant
`conversation_id` et `body`. Les nouveaux messages sont poussés sous la forme
`{"type":"message", ...}`. Le WebSocket ne remplace pas PostgreSQL ni les
routes HTTP : celles-ci restent nécessaires pour recharger l’historique après
une reconnexion. En production, le transport doit être servi en `wss://`.

`POST /api/auth/logout` révoque la session liée au cookie courant. Le endpoint
`POST /api/auth/logout-all`, authentifié par Bearer JWT, révoque les sessions
persistantes du compte. Les access tokens JWT déjà émis restent toutefois
valides jusqu’à leur expiration (15 minutes) ; ils ne sont pas vérifiés contre
la table des sessions à chaque requête. Pour les appels navigateur aux
endpoints de cookie, le backend exige l’origine configurée par
`AUTH_ALLOWED_ORIGIN` (défaut local `http://localhost:5173`). En production,
configurer `AUTH_REFRESH_COOKIE_SECURE=true` pour n’envoyer le cookie qu’en
HTTPS. Les appels `fetch` du navigateur vers l’API doivent utiliser
`credentials: "include"` pour que le cookie soit envoyé et accepté.

La demande de réinitialisation utilise `POST /api/auth/password-reset/request`
avec `{"email":"..."}`. La réponse reste générique afin de ne pas révéler si
l’adresse correspond à un compte ; l’envoi est limité à une demande par minute
et par compte, ainsi qu’à deux requêtes par minute environ par IP via Nginx.
Le lien React `/reset-password` expire après 30 minutes et demande un nouveau
mot de passe conforme aux mêmes règles que l’inscription. Après son changement,
le token est consommé, les autres tokens de réinitialisation sont invalidés et
toutes les sessions persistantes sont révoquées. Les JWT d’accès déjà émis
restent valides au plus 15 minutes. Le cookie de refresh courant est également
supprimé sur la réponse de confirmation.

La liste de mots de passe courants dans le handler est une petite liste initiale,
pas un dictionnaire de production complet. Elle devra être étendue et versionnée
avant de considérer la règle comme une protection robuste contre les mots de
passe compromis.