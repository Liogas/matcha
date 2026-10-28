# Plan des guides de la « Semaine des bibliothèques »

Ce fichier donne, pour chaque guide à venir, **ce qu'il couvrira** : objectif, notions,
étapes, fichiers touchés et critère de fin. Le guide complet (commandes exactes, code
ligne par ligne, sorties « Attendu » vérifiées, questions) est écrit **au moment de s'y
mettre**, à partir de ton code réel. Écrit maintenant, il décrirait un code légèrement
différent du tien, et ses « Attendu » seraient faux.

Quand tu arrives à un guide, demande-le simplement (« guide 05 »).

| # | Guide | Jour du planning | État |
|---|---|---|---|
| 01 | [cpp-auth dans `backend/libs/`](01-libs-cpp-auth.md) | lundi | ✅ fait |
| 02 | [Le gabarit `libs/_template`](02-libs-template.md) | lundi | ✅ fait |
| 03 | [Warnings, presets, sanitizers](03-presets-warnings.md) | lundi | ✅ fait |
| 04 | [cpp-core](04-cpp-core.md) | lundi | ✅ fait |
| 05 | [cpp-pg (1/2) : connexion, pool, transactions, lignes typées](05-cpp-pg-1.md) | mardi | ✅ fait |
| 06 | [cpp-pg (2/2) : QueryBuilder, migrations, branchement](06-cpp-pg-2.md) | mercredi | 📄 écrit |
| 07 | cpp-validation et cpp-geo | jeudi | 🗺️ |
| 08 | cpp-realtime et cpp-realtime-crow | vendredi | 🗺️ |
| 09 | cpp-notify, puis cpp-auth (1/2) | samedi | 🗺️ |
| 10 | cpp-auth (2/2), cpp-auth-crow, clôture | dimanche | 🗺️ |

Les deux tâches « Hygiène » du lundi sont **déjà faites** dans le dépôt : secrets en
fichiers (`*_FILE`), port PostgreSQL lié à `127.0.0.1`, Dockerfile multi-étapes avec une
image d'exécution légère. Pas de guide pour elles.

**Conventions communes à tous les guides** (fixées par les guides 02 à 04) :

- chaque lib naît de `libs/new-lib.sh`, est déclarée dans `libs/CMakeLists.txt` **après**
  ses dépendances, et a son option `CPP_<NOM>_BUILD_TESTS` dans le preset `debug` ;
- les erreurs attendues sont des `core::Result<T>`, les logs passent par un
  `core::ILogger &` injecté ;
- une lib est finie quand elle passe la définition de terminé (warnings, `sanitize`,
  package-consumer, README, CHANGELOG) **et** remplace du vrai code du backend.

---

## Guide 05 — cpp-pg (1/2) : connexion, pool, transactions, lignes typées

> 📄 **Écrit** : [05-cpp-pg-1.md](05-cpp-pg-1.md). Décision du banc de test : base
> `postgres-test` en `tmpfs` dans le compose de dev, variables `PG_TEST_*`, tests sautés
> sans `PG_TEST_HOST`, table vidée par `TRUNCATE` avant chaque test (et non une
> transaction annulée : cpp-pg teste justement `commit()` et deux transactions
> parallèles). La fixture « transaction annulée » servira aux repositories, au guide 06.

**Objectif** : remplacer la connexion unique protégée par un mutex (`src/database/`) par
un pool de connexions et des transactions sûres. C'est la priorité 1 de l'audit : les
transactions sont aujourd'hui inutilisables, car le mutex est relâché entre `BEGIN` et la
requête suivante (audit 4.5).

**Dépend de** : cpp-core (`Result`, `Error`, `ConfigReader` pour les paramètres de
connexion, `ILogger` pour remplacer les `[INFO BDD]`).

**Notions** :
- libpq : `PGconn`, `PQexecParams`, paramètres `$1..$n`, formats texte et binaire, OID
  des types, `PQresultStatus`, `PQresultErrorField` ;
- RAII et sémantique de déplacement : un objet propriétaire non copiable mais déplaçable
  (`unique_ptr` avec un *deleter* `PQfinish`, constructeur de déplacement, `= delete`) ;
- `std::condition_variable` : attendre qu'une connexion se libère, avec délai maximum ;
- isolation des transactions : `READ COMMITTED` contre `SERIALIZABLE`, ce que chacun
  garantit ;
- codes `SQLSTATE` et leur traduction en `core::ErrorCode`.

**Étapes prévues** :
1. Générer la lib : `new-lib.sh cpp-pg pg`, `find_dependency(cpp-core)` et
   `find_dependency(PostgreSQL)` dans `Config.cmake.in` (guide 02, Q3).
2. `Connection` : wrapper RAII de `PGconn`, ouverture avec N tentatives, échec renvoyé en
   `Result` au lieu de continuer avec un pointeur nul (`Database.cpp:30`, audit 4.5).
3. `ConnectionPool` : N connexions, `acquire(timeout)` renvoie un `PooledConnection` qui
   se rend tout seul au pool à sa destruction ; `PQreset` si la connexion est cassée.
4. `Transaction` : `auto tx = pool.begin(isolation); … tx.commit();`, `ROLLBACK`
   automatique dans le destructeur, la connexion gardée du `BEGIN` au `COMMIT`.
5. Paramètres typés : `tx.query(sql, args...)` variadique (`int64`, `bool`, `string`,
   `optional` → `NULL`, `vector` → tableau PostgreSQL, `time_point` → ISO 8601).
   Notion : *templates variadiques* et *fold expressions*.
6. `Row` et `Rows` : `row.get<T>("colonne")`, `get<std::optional<T>>`, accès **par nom**
   via `PQfnumber`, gestion de `TIMESTAMPTZ` (`QueryResult.cpp:220`, audit 4.4).
7. Erreurs SQL : `23505` → `Conflict`, `23503`/`23514` → `Invalid`, `40001`/`40P01` →
   `Unavailable`.
8. Banc de test : service `postgres-test` (stockage en `tmpfs`) dans le compose de dev,
   fixture GoogleTest qui ouvre une transaction annulée à la fin de chaque test.

**Décision à prendre en début de guide** : comment les tests de cpp-pg trouvent leur base
(variables `PG_TEST_*` lues avec `ConfigReader`, tests sautés avec `GTEST_SKIP()` si
absentes, pour que la lib reste testable seule).

**Terminé quand** : un test lance deux transactions en parallèle sur deux threads sans
qu'elles se mélangent, un test de pool épuisé expire proprement, et une colonne
`TIMESTAMPTZ` se lit correctement. Tout passe sous `sanitize`.

**Pièges attendus** : un `PGresult` non libéré (fuite vue par LSan, d'où un wrapper
RAII), une transaction détruite pendant une exception, des tests d'intégration qui
s'influencent s'ils ne sont pas isolés.

---

## Guide 06 — cpp-pg (2/2) : QueryBuilder, migrations, branchement

> 📄 **Écrit** : [06-cpp-pg-2.md](06-cpp-pg-2.md). Nouveau format : notions « juste à
> temps », chaque étape part d'une situation qui casse, points d'arrêt par étape.
> Écarts au plan : `pg::Executor` (interface commune) ajouté ; migrations en **une**
> transaction avec `pg_advisory_xact_lock` ; verrou du like par
> `hashtextextended(least:greatest)` (la forme à deux clés est en `INTEGER`) ;
> `SAVEPOINT` par requête dans la fixture des repositories ; `copyIn` reporté.

**Objectif** : finir cpp-pg et l'utiliser partout, jusqu'à **supprimer `src/database/`**.

**Notions** :
- SQL dynamique **sûr** : les valeurs passent toujours en paramètres `$n`, les noms de
  colonnes seulement via une liste blanche ;
- pagination *keyset* (`WHERE (fame, id) < ($1, $2)`) contre `OFFSET`, et pourquoi
  `OFFSET` ralentit et saute des lignes ;
- verrous consultatifs : `pg_advisory_xact_lock` ;
- somme de contrôle d'un fichier (détecter une migration modifiée après coup).

**Étapes prévues** :
1. Nettoyage préalable : migration `DROP TABLE users`, suppression de `UserRepository`,
   `user_repository_integration_tests` et `auth_integration_tests` (audit 4.15). Moins de
   code à migrer.
2. `QueryBuilder` : `where("age >= ?", v)`, `whereIf(cond, …)`, numérotation automatique
   des `$n`, `orderBy(clé, listeBlanche)`, `limit`, `after(…)`. Tests **purs** : on
   compare le SQL et les paramètres produits, sans base.
3. `MigrationRunner` : `DatabaseInitializer` déplacé dans cpp-pg, verrou consultatif
   conservé, somme de contrôle par fichier appliqué, mode `--dry-run`, le trou de la
   migration 006 documenté (audit 4.16).
4. Migrer `AccountRepository`, `LikeRepository`, `ProfileRepository` et
   `MessagingRepository` vers cpp-pg, accès par nom de colonne. Puis supprimer
   `src/database/`.
5. Preuve : le like mutuel atomique. Une transaction avec
   `pg_advisory_xact_lock(least(a,b), greatest(a,b))`, like, match, conversation. Supprimer
   la resynchronisation des conversations au démarrage dans `main.cpp` (audit 4.6, 4.7).
6. Bonus : `copyIn` (`COPY FROM STDIN`) pour le futur générateur de 500 profils.

**Terminé quand** : `src/database/` n'existe plus, tous les tests passent, et un test de
like simultané A ↔ B (deux threads) crée bien **un** match.

**Pièges attendus** : un repository qui garde une `Transaction` plus longtemps que
nécessaire (pool épuisé), un ordre de verrouillage différent entre deux chemins
(*deadlock*, d'où `least`/`greatest`).

---

## Guide 07 — cpp-validation et cpp-geo

**Objectif** : deux bibliothèques **pures** (aucune entrée-sortie, ni Crow, ni
PostgreSQL), testées à près de 100 %. Elles sortent la validation des handlers HTTP et
préparent la géolocalisation.

**Notions** :
- UTF-8 : octets contre points de code, séquences invalides, pourquoi `size()` ne compte
  pas les caractères ;
- politique de mot de passe selon NIST SP 800-63B (longueur et liste de mots interdits
  plutôt que règles de composition), idée de zxcvbn ;
- `std::regex` : coût de construction (d'où `static const`), et pourquoi pas de plages
  non ASCII dedans (audit 4.14) ;
- formule de Haversine, *bounding box*, l'antiméridien ;
- minimisation des données (RGPD) : arrondir une position GPS à 1 km.

**Étapes prévues — cpp-validation** :
1. Module `utf8` : `isValid`, `length` en points de code. Remplace
   `utf8CharacterCount`.
2. Validateurs : `EmailValidator` (syntaxe raisonnable, normalisation),
   `UsernameValidator`, `PersonNameValidator`, `CityValidator`, chacun renvoyant un
   `core::Result<std::string>` (la valeur normalisée, ou l'erreur).
3. `HtmlEscaper` (`escapeText`, `escapeAttribute`), utilisé dans la page de vérification
   d'email (`AuthEmailRoutes.cpp:41`, audit 4.3).
4. `IPasswordPolicy` et `DictionaryPasswordPolicy` : longueur minimale, liste versionnée
   dans `backend/data/` (mots anglais courants et mots de passe fréquents), variantes
   triviales refusées (mot + chiffres, *leet speak*). Remplace les 7 mots en dur
   (`AuthRouteSupport.cpp:10`, sujet 3.2).

**Étapes prévues — cpp-geo** :
1. `GeoPoint`, `haversineKm`, `boundingBox(centre, rayonKm)`,
   `roundToPrecision(point, 1 km)`. Tests : pôles, antiméridien, distance Paris–Lyon
   connue.
2. `IGeocoder` et `TableGeocoder` : fichier GeoNames `cities15000` chargé en mémoire,
   recherche par nom normalisé. Aucun appel réseau, donc fiable en soutenance.
3. Bonus : `NominatimGeocoder`, avec cache, via le client HTTP de cpp-auth.

**Terminé quand** : ni cpp-validation ni cpp-geo ne dépendent de Crow ou de PostgreSQL,
et les handlers d'inscription et de profil utilisent les validateurs (plus de regex dans
les routes).

---

## Guide 08 — cpp-realtime et cpp-realtime-crow

**Objectif** : un seul WebSocket, `/api/ws`, qui transporte les messages, les
notifications et la présence. On corrige au passage le hub actuel, où un client lent
bloque tout le monde (audit 6.3).

**Notions** :
- possession mémoire : `shared_ptr`, `weak_ptr`, qui possède une session WebSocket ;
- contention de verrou : copier la liste sous verrou, envoyer **hors** verrou (la règle
  déjà vue dans `StreamLogger`, guide 04) ;
- *heartbeat* ping/pong et détection d'une connexion morte ;
- *fan-out* et Pub/Sub (pour plusieurs instances un jour).

**Étapes prévues — cœur (sans Crow)** :
1. `Envelope` `{ type, data }` avec nlohmann_json, analyse et sérialisation, erreurs en
   `Result`.
2. `ISession` et `Hub` : sessions par sujet (`account:{id}`), `subscribe`, `unsubscribe`,
   `publish`. Les sessions appartiennent au hub : plus de `new std::int64_t` dans
   `userdata`.
3. `PresenceTracker` (interface + version en mémoire) : compteur de connexions par compte,
   `isOnline`, `lastSeen`, rappel `onOffline` pour écrire `last_seen_at` (écritures
   espacées).
4. `IBroker` et `InMemoryBroker`, prêts pour un `RedisBroker` plus tard.

**Étapes prévues — adaptateur Crow** :
1. Ticket WebSocket : `POST /api/ws-ticket` renvoie un jeton opaque valable 30 s, à usage
   unique. Le JWT ne passe plus dans l'URL (audit 4.12).
2. Route `/api/ws` : heartbeat, routage `type` → handler, message d'erreur renvoyé au
   client, limite de débit simple par session.
3. La messagerie devient un handler `"message"` du hub ; `/api/messaging/ws` reste en
   alias le temps que le front migre.

**Terminé quand** : un test avec deux `FakeSession`, dont une lente, montre que l'autre
reçoit quand même ses messages, et le chat du front fonctionne via `/api/ws`.

**Pièges attendus** : une session détruite pendant un `publish` (d'où `weak_ptr`), un
*data race* que seul TSan verrait (preset `tsan` à envisager : il ne se combine pas avec
ASan, guide 03 §1.6).

---

## Guide 09 — cpp-notify, puis cpp-auth (1/2)

**Objectif** : stocker et pousser les notifications, puis commencer la refonte de
cpp-auth.

**Notions** :
- *outbox pattern* et « publier **après** le commit » : ne jamais notifier une action que
  la base a annulée ;
- énumération d'utilisateurs (*user enumeration*) et vérification à temps constant ;
- JWKS : publier sa clé publique, et pourquoi le backend n'a pas à se télécharger
  lui-même.

**Étapes prévues — cpp-notify** (dépend de cpp-pg et cpp-realtime) :
1. `Notification` `{ recipient, actor, type, payload JSONB, createdAt, readAt }`,
   `INotificationStore` + `PgNotificationStore`. La lib fournit sa migration (table et
   index `(recipient_id, read_at, id DESC)`).
2. `NotificationDispatcher` : `INotificationFilter` fourni par l'application (blocage,
   unlike), stocke, puis publie `{"type":"notification"}` et `unread_count` via le hub,
   après le commit.
3. Lecture : `list(cursor)`, `unreadCount`, `markRead`. Tests avec un `FakeHub`.

**Étapes prévues — cpp-auth (1/2)** (dépend de cpp-validation) :
1. Login par identifiant : email **ou** nom d'utilisateur ; compte inexistant → hash
   factice vérifié quand même, pour un temps de réponse constant
   (`AuthSessionRoutes.cpp:34`, sujet 3.1).
2. Séparer `LoginService` et `RegistrationService`, injecter `IPasswordPolicy` et
   `IEmailValidator` de cpp-validation, supprimer `isValidEmail`/`isValidPwd` en double
   (audit 6.1 #3 et #4).
3. `StaticJwksProvider` construit depuis la clé publique de `RsaTokenSigner` :
   `allowInsecureHttp` disparaît (audit 4.9).

**Terminé quand** : une notification créée en test arrive sur la `FakeSession` du
destinataire avec le bon `unread_count`, et le login par nom d'utilisateur fonctionne.

---

## Guide 10 — cpp-auth (2/2), cpp-auth-crow, clôture

**Objectif** : sortir d'`AuthRouteSupport` tout ce qui est générique, et finir la semaine
avec une authentification propre.

**Notions** :
- middleware Crow et contexte de requête (`ctx`) ;
- proxys de confiance : quand croire `X-Real-IP` ;
- rotation des *refresh tokens* et détection de réutilisation.

**Étapes prévues — cpp-auth (2/2)** :
1. Module sodium : `Argon2idPasswordHasher` et `OpaqueTokenGenerator` (fusion
   d'`OpaqueToken` et `EmailVerificationToken`), plus un jeton à usage unique générique
   (audit 6.1 #2).
2. `IAttemptLimiter` dans cpp-auth, version en mémoire pour les tests ;
   `RedisLoginAttemptLimiter` l'implémente côté backend (audit 6.1 #10).
3. Bonus : `RefreshTokenService` et `IRefreshTokenStore`, remontés depuis
   `AccountRepository` (audit 6.1 #1).

**Étapes prévues — cpp-auth-crow** :
1. Middleware `RequireAuth` : remplit `ctx.accountId` ; `VerificationUnavailable` donne
   `503` (audit 4.8). Remplace `authenticatedAccountId` + `unauthorized()` copiés dans
   chaque route. C'est aussi là que naît le `request_id` des logs (guide 04, §1.5).
2. Utilitaires HTTP : `bearerToken`, cookies, contrôle d'`Origin`, `clientIp()` qui ne
   lit `X-Real-IP` que si l'appelant est dans `TRUSTED_PROXIES` (audit 4.10).
3. Traduction `core::ErrorCode` → statut HTTP, à un seul endroit (guide 04, §4.1).
4. Dans `handleRefreshSession` : signer le JWT **avant** la rotation du refresh token
   (audit 4.11).

**Clôture** : README et CHANGELOG `0.1.0` de chaque lib, vérification de la définition de
terminé, liste de ce qui glisse à la semaine suivante.

**Terminé quand** : plus aucune route n'appelle `authenticatedAccountId` à la main, et un
JWKS indisponible renvoie `503` au lieu de `401`.
