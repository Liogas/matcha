# Audit du backend Matcha

> Compte rendu établi le 24/10/2026 à partir du sujet `fr.subject.pdf` (v6.0) et du code présent
> dans `backend/`. Aucun fichier de code n'a été modifié.
>
> Décisions d'architecture validées pour ce document :
> - les bibliothèques réutilisables vivent dans un dossier **`backend/libs/`** du dépôt (monorepo), chacune
>   installable via CMake et extractible plus tard dans son propre dépôt ;
> - chaque bibliothèque a un **cœur indépendant de Crow** et, si besoin, un **adaptateur Crow** séparé
>   (`cpp-xxx` + `cpp-xxx-crow`) ;
> - périmètre : **backend uniquement** (le front n'est pas audité).

---

## Sommaire

1. [Résumé](#1-résumé)
2. [Couverture du sujet : état fonctionnalité par fonctionnalité](#2-couverture-du-sujet)
3. [Fonctionnalités manquantes : détail, notions et bibliothèque associée](#3-fonctionnalités-manquantes)
4. [Bugs et failles relevés dans l'existant](#4-bugs-et-failles-relevés-dans-lexistant)
5. [Architecture : ce qui est bien, ce qui freine la maintenance](#5-architecture)
6. [Bibliothèques : bilan de l'existant et feuille de route](#6-bibliothèques)
7. [Plan d'action priorisé](#7-plan-daction-priorisé)

---

## 1. Résumé

**Points forts (à mettre en avant en entretien)**
- La sécurité de l'authentification est soignée : Argon2id (libsodium), JWT RS256 avec JWKS, refresh token opaque
  haché en base, rotation avec détection de réutilisation, cookie `HttpOnly`/`SameSite=Strict`, contrôle d'`Origin`,
  limiteur de connexions Redis (script Lua atomique), réponses qui ne révèlent pas si un email existe, cooldowns
  sur les renvois d'emails.
- `cpp-auth` est une vraie bibliothèque : interfaces (`JwksProvider`, `ITokenSignatureVerifier`, `IClock`…),
  injection de dépendances, package CMake installable, test « consommateur » du package, et beaucoup de tests.
- Les requêtes SQL sont **toutes paramétrées** (`PQexecParams`), les contraintes d'intégrité sont en base
  (`CHECK`, index uniques sur `LOWER(email)`, paire de match canonique `first < second`).
- Les uploads sont ré-encodés en WebP par libvips (taille et dimensions bornées), ce qui neutralise les fichiers
  piégés. Les clés de stockage sont générées côté serveur.
- La messagerie est déjà pensée comme un module générique (conversations/participants/messages), sans dépendance
  au domaine Matcha.

**Ce qui manque le plus** (bloquant pour la soutenance) : tags d'intérêt, géolocalisation, note de popularité,
suggestions avec matching intelligent, recherche avancée avec tri et filtres, historique des visites, blocage,
signalement, présence en ligne et dernière connexion, notifications temps réel, login par **nom d'utilisateur**,
modification de l'email, du nom et du prénom, 500 profils en base, vérification qu'on a une photo avant de liker,
refus des mots anglais courants comme mot de passe.

**Ce qui freine la maintenance** : une seule connexion PostgreSQL protégée par un mutex (pas de pool, pas de
transactions utilisables depuis le code applicatif), la logique métier placée dans les handlers HTTP,
l'authentification dupliquée dans chaque route, une gestion d'erreurs hétérogène (exceptions, `bool`, structs
`Result`), du code hérité (`users`, `UserRepository`) encore compilé et testé.

---

## 2. Couverture du sujet

Légende : ✅ fait · 🟡 partiel · ❌ absent

### IV.1 Inscription et connexion

| Exigence | État | Où / remarque |
|---|---|---|
| Inscription email, username, nom, prénom, mot de passe | ✅ | `src/http/AuthEmailRoutes.cpp` (`handleRegisterAccount`) |
| Mot de passe sécurisé, **mots anglais courants refusés** | 🟡 | `src/http/AuthRouteSupport.cpp:10` : 7 mots de passe en dur seulement. Aucun dictionnaire de mots anglais. |
| Email avec lien unique de vérification | ✅ | Token opaque haché, 24 h, page de confirmation par clic explicite |
| **Connexion par nom d'utilisateur** + mot de passe | ❌ | `src/http/AuthSessionRoutes.cpp:34` : la connexion se fait par **email**. Le sujet exige le nom d'utilisateur. |
| Réinitialisation du mot de passe par email | ✅ | `src/http/AuthPasswordResetRoutes.cpp` |
| Déconnexion en un clic | ✅ | `POST /api/auth/logout` et `/logout-all` |

### IV.2 Profil utilisateur

| Exigence | État | Où / remarque |
|---|---|---|
| Genre, préférences sexuelles, biographie | 🟡 | `PATCH /api/profile`. L'orientation accepte **n'importe quelle chaîne** (`ProfileRoutes.cpp:239`), la biographie n'a **pas de longueur maximale**, le genre accepte tout `[A-Za-z_]+`. |
| **Tags d'intérêt réutilisables** | ❌ | Les tables `matcha_interests` et `matcha_profile_interests` existent mais aucun repository ni aucune route ne les utilise. |
| Jusqu'à 5 photos, dont une photo de profil | ✅ | `ProfilePhotoService` + contraintes SQL `position 1..5` et une seule photo principale |
| Modifier **nom, prénom, email** | ❌ | Aucune route. Changer l'email doit déclencher une nouvelle vérification. |
| Voir **qui a consulté** son profil | ❌ | Aucune table de visites |
| Voir qui l'a liké | 🟡 | `GET /api/likes/received` ne renvoie que des **ids**, il faut ensuite charger chaque profil un par un (problème N+1 côté client). |
| **Note de popularité** publique | ❌ | Ni colonne ni calcul |
| **Localisation GPS** (avec consentement) ou saisie manuelle, modifiable | 🟡 | Seuls `city` et `location_source` existent. Pas de latitude/longitude, donc aucun calcul de distance possible. |

### IV.3 Navigation (suggestions)

| Exigence | État | Où / remarque |
|---|---|---|
| Liste de profils suggérés | 🟡 | `GET /api/profiles` liste les profils complets triés par `created_at` (`ProfileRepository.cpp:73`) |
| Respect de l'orientation (bisexuel par défaut) | 🟡 | Le défaut `bisexual` est en base, mais le filtrage n'est **pas appliqué** |
| Score : proximité, tags communs, popularité | ❌ | — |
| Priorité à la même zone géographique | ❌ | — |
| Tri par âge, distance, popularité, tags communs | ❌ | — |
| Filtres âge, distance, popularité, tags | ❌ | — |
| Exclusion des utilisateurs bloqués | ❌ | — |

### IV.4 Recherche avancée

| Exigence | État |
|---|---|
| Tranche d'âge, plage de popularité, localisation, tags, avec tri et filtres | ❌ |

### IV.5 Consultation de profil

| Exigence | État | Où / remarque |
|---|---|---|
| Voir un profil sans email ni mot de passe | ✅ | `GET /api/profiles/<id>` |
| **Enregistrer la visite** | ❌ | — |
| Liker la photo de profil | ✅ | `POST /api/profiles/<id>/like` |
| **Interdit si l'utilisateur courant n'a pas de photo de profil** | ❌ | `LikeRoutes.cpp:15` vérifie la cible, **pas l'utilisateur courant** |
| Retirer un like (unlike), qui désactive le chat | ✅ | `DELETE …/like`, qui supprime le match et la conversation |
| Voir la popularité d'un autre utilisateur | ❌ | — |
| **En ligne / date de dernière connexion** | ❌ | — |
| **Signaler un faux compte** | ❌ | — |
| **Bloquer** (absent des recherches, plus de notifications ni de chat) | ❌ | — |
| Voir si la personne m'a liké ou si nous sommes connectés | 🟡 | Possible côté client en croisant `/api/likes/received` et `/api/matches`, mais le profil ne renvoie pas de champ `relationship` |

### IV.6 Chat

| Exigence | État | Où / remarque |
|---|---|---|
| Chat temps réel entre utilisateurs connectés | ✅ | WebSocket `/api/messaging/ws` et routes HTTP pour l'historique |
| Indicateur de nouveau message sur toutes les pages | 🟡 | `unread_count` existe par conversation. Il manque un compteur global poussé en temps réel. |
| Chat impossible si bloqué | ❌ | Le blocage n'existe pas |

### IV.7 Notifications

| Exigence | État |
|---|---|
| Like reçu, profil consulté, message reçu, like retourné (match), unlike par un utilisateur connecté | ❌ (seul le message est poussé, via le WebSocket de messagerie, sans être stocké comme notification) |
| Compteur de notifications non lues sur toutes les pages | ❌ |

### Instructions générales

| Exigence | État | Remarque |
|---|---|---|
| 500 profils distincts minimum | ❌ | Le seed en crée 10 (`backend/seeds/development_profiles.sql`) |
| Requêtes SQL écrites à la main, pas d'ORM | ✅ | — |
| Identifiants dans un `.env` exclu de Git | 🟡 | `.env` est bien ignoré, mais `docker-compose.yml` contient `POSTGRES_PASSWORD: admin` et `DB_PASSWORD: admin` **en clair et versionnés** (le commentaire « NE PAS LAISSER LES VALEURS BRUTS » le signale déjà). Ce point peut faire échouer la soutenance. |
| Validation de tous les formulaires | 🟡 | Voir orientation, biographie et ville plus haut |
| Aucune erreur ni warning | ⚠️ | Le build n'utilise pas `-Wall -Wextra -Werror` : impossible de le garantir |

---

## 3. Fonctionnalités manquantes

Pour chaque fonctionnalité : **ce qu'il faut faire**, les **notions et technologies utiles**, et la **bibliothèque**
(nouvelle ou existante) qui peut en porter la partie générique.

> Principe de découpage : tout ce qui est **générique** (sans le mot « Matcha » ni « like ») va dans une
> bibliothèque sous `libs/`. Tout ce qui est **métier** (orientation, matching, popularité) reste dans
> `src/domain` ou `src/services`.

### 3.1 Connexion par nom d'utilisateur

- **À faire** : accepter `{ "username", "password" }` sur `/api/auth/login`. Ajouter
  `findByLogin(identifier)` au repository : soit une méthode dédiée `findByUsername`, soit une interface
  `IUserLookupRepository::findByIdentifier`. Clé du limiteur Redis = username normalisé.
- **Notions** : *user enumeration*. Garder un temps de réponse constant, par exemple en vérifiant un hash factice
  quand l'utilisateur n'existe pas (aujourd'hui, `AuthService::login` répond plus vite sur un compte inexistant).
- **Bibliothèque** : **`cpp-auth`**. Faire évoluer `AuthService::login` pour qu'il prenne un *identifier*
  abstrait plutôt qu'un email codé en dur, et y ajouter la vérification factice à temps constant.

### 3.2 Mot de passe : refus des mots anglais courants

- **À faire** : charger au démarrage une liste versionnée, par exemple les 10 000 mots anglais les plus courants
  et les mots de passe les plus fréquents (SecLists, liste NCSC « 100k most common »). Refuser un mot de passe
  qui *est* un mot courant, ou qui n'en est qu'une variante triviale (mot + chiffres, *leet speak*).
- **Notions** : recommandations NIST SP 800-63B (longueur minimale plutôt que règles de composition, liste de
  blocage), estimation d'entropie à la manière de **zxcvbn**, et éventuellement l'API *k-anonymity* de
  HaveIBeenPwned.
- **Bibliothèque** : **nouvelle `cpp-validation`** (ou un module `password-policy` de `cpp-auth`), avec une
  interface `IPasswordPolicy` et une implémentation `DictionaryPasswordPolicy(path)`. `AuthService::isValidPwd`
  existe déjà dans `cpp-auth` et ferait doublon avec `validPassword` côté backend : il faut garder une seule
  politique, injectée.

### 3.3 Tags d'intérêt réutilisables

- **À faire** :
  - `GET /api/interests?q=veg` : autocomplétion parmi les tags existants (préfixe, `LIMIT 10`) ;
  - `PUT /api/profile/interests` avec `{ "interests": ["vegan","geek"] }` : remplacement complet dans
    **une transaction** (`INSERT … ON CONFLICT DO NOTHING` sur `matcha_interests`, puis remplacement des liens) ;
  - normalisation : minuscules, `#` retiré, longueur 1 à 40, au maximum ~10 tags par profil ;
  - renvoyer les tags dans `GET /api/profile` et `GET /api/profiles/<id>`.
- **Notions** : relation N-N, `ON CONFLICT`, `unnest($1::text[])` pour passer un tableau en **un seul**
  paramètre, index `pg_trgm` si la recherche floue devient utile.
- **Bibliothèque** : métier, à garder dans `src/`. En revanche, il faudra pouvoir passer des **paramètres de
  type tableau** et ouvrir une **transaction** : c'est le rôle de **`cpp-pg`** (voir 6.2).

### 3.4 Modifier nom, prénom et email

- **À faire** :
  - `PATCH /api/profile/identity` pour `first_name` et `last_name` (réutiliser `validPersonName`) ;
  - `POST /api/account/email-change` avec `{ new_email, password }` : redemander le mot de passe, envoyer un lien
    à la **nouvelle** adresse et une alerte à l'**ancienne**, et ne basculer qu'après confirmation. Nouvelle table
    `email_change_tokens` (même modèle que `password_reset_tokens`).
- **Notions** : réauthentification pour les actions sensibles, *account takeover*.
- **Bibliothèque** : **`cpp-auth`**, avec un module « tokens à usage unique » générique : génération, hash,
  expiration, consommation. Aujourd'hui, `OpaqueToken` et `EmailVerificationToken` sont dans le backend et font
  la même chose. Voir 6.1.

### 3.5 Géolocalisation

- **À faire** :
  - migration : colonnes `latitude DOUBLE PRECISION`, `longitude DOUBLE PRECISION`, `neighborhood TEXT`, avec un
    index ;
  - `PUT /api/profile/location` avec `{ source: "gps", lat, lon }` ou `{ source: "manual", city }` ;
  - **GPS** : le navigateur fournit les coordonnées (`navigator.geolocation`) et le backend les **arrondit** à
    environ 1 km (2 décimales) avant stockage, ce qui correspond à la précision « quartier » demandée par le sujet ;
  - **manuel** : géocodage de la ville avec Nominatim (OpenStreetMap), sous forme d'appel HTTP *server-side* avec
    cache. Dans un environnement fermé, on peut aussi charger une table `cities` (GeoNames `cities15000`) en base
    et ne faire aucun appel externe, ce qui est plus robuste en soutenance ;
  - **consentement** : `location_consent_at` existe déjà, il faut le remplir et permettre de le retirer.
- **Notions** : formule de Haversine, *bounding box* pour pré-filtrer avec un index, PostGIS (`geography`,
  `ST_DWithin`) ou les extensions `cube` + `earthdistance` (plus légères), geohash, RGPD (minimisation, droit de
  retrait).
- **Bibliothèque** : **nouvelle `cpp-geo`** :
  - `GeoPoint`, `haversineKm(a, b)`, `boundingBox(center, radiusKm)`, `roundToPrecision(point, km)` ;
  - interface `IGeocoder` avec deux implémentations, `NominatimGeocoder` (HTTP) et `TableGeocoder` (base locale) ;
  - aucune dépendance à Crow ni à PostgreSQL, donc testable à 100 %.

### 3.6 Note de popularité (*fame rating*)

- **À faire** : définir une formule **documentée et cohérente** (le sujet laisse le choix). Par exemple :

  ```
  fame = clamp(0, 100,
           10 * log2(1 + likes_reçus)
         + 15 * log2(1 + matchs)
         +  3 * log2(1 + visites_uniques_30j)
         - 20 * signalements_validés
         + 5  si profil complet avec 5 photos)
  ```

  La stocker dans `matcha_profiles.fame_rating` et la recalculer **à l'événement** (like, unlike, visite,
  signalement) plutôt qu'à chaque lecture, sinon le tri devient coûteux.
- **Notions** : dénormalisation contrôlée, *trigger* SQL contre recalcul applicatif, échelle logarithmique pour
  éviter qu'un profil très populaire écrase tous les autres.
- **Bibliothèque** : métier, à garder dans `src/domain/FameCalculator`, en **fonction pure** testable à part.
  Le déclenchement s'appuie sur le bus d'événements (voir 5.3).

### 3.7 Suggestions avec matching intelligent

- **À faire** : `GET /api/suggestions?sort=score|age|distance|fame|tags&order=asc|desc&age_min&age_max&distance_max&fame_min&fame_max&tags=a,b&cursor=`.
  - **Compatibilité d'orientation dans les deux sens** : A doit chercher le genre de B, et B le genre de A.
    Une orientation non renseignée vaut « bisexuel ». La règle doit vivre dans **une** fonction métier, avec une
    table de vérité testée.
  - **Exclusions** : soi-même, profils incomplets, profils bloqués (dans les deux sens), éventuellement les
    profils déjà likés ou déjà vus.
  - **Score** : `w_d * proximité + w_t * tags_communs + w_f * popularité`, avec les poids en configuration. La
    priorité géographique peut passer par un bonus fort sous X km ou par un tri par tranche de distance.
  - Le calcul se fait **en SQL** : pré-filtre par *bounding box*, puis score calculé dans la requête, puis
    `ORDER BY`/`LIMIT`.
- **Notions** : SQL dynamique **sûr** (les colonnes de tri viennent d'une *whitelist* et ne sont jamais
  concaténées depuis la requête HTTP), pagination par curseur (*keyset*) plutôt que `OFFSET`, `EXPLAIN ANALYZE`,
  index composites.
- **Bibliothèque** :
  - **`cpp-pg`** fournit un `QueryBuilder` minimal (clauses `WHERE` dynamiques avec placeholders `$n`
    numérotés automatiquement, `ORDER BY` restreint à une *whitelist*). Le sujet l'encourage explicitement :
    « vous pouvez créer votre propre bibliothèque pour simplifier la gestion des requêtes ».
  - **`cpp-http-crow`** fournit un parseur de paramètres de requête typés : `int` borné, liste, énumération.
    Aujourd'hui, chaque route refait `std::from_chars` à la main (`ProfileRoutes.cpp`, `MessagingRoutes.cpp`).

### 3.8 Recherche avancée

- **À faire** : même moteur que les suggestions, avec des critères explicites et sans score imposé. Il vaut mieux
  **une seule** requête paramétrée par un objet `ProfileQuery { filters, sort, page }` que deux implémentations
  qui divergeront.
- **Bibliothèque** : **`cpp-pg`** (`QueryBuilder`).

### 3.9 Historique des visites

- **À faire** :
  - table `profile_views(viewer_id, viewed_id, viewed_at)`, avec une déduplication par exemple par fenêtre
    d'une heure (`ON CONFLICT` sur `(viewer, viewed, date_trunc('hour', viewed_at))` ou contrôle applicatif) ;
  - enregistrement dans `GET /api/profiles/<id>`, en pratique **via un événement `ProfileViewed`** ;
  - `GET /api/profile/visitors` (qui m'a vu) et `GET /api/profile/history` (qui j'ai vu), qui renvoient des
    **profils résumés** (pas seulement des ids) et paginés ;
  - ne pas enregistrer les visites des utilisateurs bloqués.
- **Bibliothèque** : métier, dans `src/`. La notification « profil consulté » passe par `cpp-notify`.

### 3.10 Blocage et signalement de faux compte

- **À faire** :
  - tables `user_blocks(blocker_id, blocked_id, created_at)` et
    `user_reports(reporter_id, reported_id, reason, created_at, status)` ;
  - `POST/DELETE /api/profiles/<id>/block` et `POST /api/profiles/<id>/report` ;
  - **appliquer le blocage partout** : suggestions, recherche, consultation (404), likes, notifications, envoi de
    messages en HTTP **et en WebSocket**, liste des conversations. C'est une règle transversale : une fonction ou
    vue SQL unique `NOT EXISTS (SELECT 1 FROM user_blocks …)`, réutilisée partout, évite d'en oublier une ;
  - bloquer supprime aussi le like et le match existants.
- **Notions** : *policy* centralisée, vue SQL `visible_profiles_for($viewer)`, tests d'autorisation (un test par
  point d'entrée).
- **Bibliothèque** : métier. Le module messagerie générique doit seulement accepter une **interface**
  `IConversationPolicy::canSend(sender, conversation)`, que Matcha implémente avec ses règles de blocage et de
  match.

### 3.11 Présence en ligne et dernière connexion

- **À faire** :
  - **en ligne** : au moins une connexion WebSocket ouverte. Compteur par compte dans le hub (déjà présent :
    `_connections`). Pour plusieurs instances, une clé Redis `presence:{id}` avec TTL rafraîchie par
    *heartbeat* ;
  - **dernière connexion** : colonne `last_seen_at`, mise à jour à la fermeture du WebSocket et à intervalle
    raisonnable pendant l'activité (pas à chaque requête) ;
  - exposer `online` et `last_seen_at` dans le profil public.
- **Notions** : *heartbeat*/ping-pong WebSocket, TTL Redis, *debounce* des écritures.
- **Bibliothèque** : **nouvelle `cpp-realtime`** avec un `PresenceTracker` (interface, une implémentation en
  mémoire et une implémentation Redis).

### 3.12 Notifications temps réel

- **À faire** :
  - table `notifications(id, recipient_id, actor_id, type, payload JSONB, created_at, read_at)` avec un index
    `(recipient_id, read_at, id DESC)` ;
  - types : `like_received`, `profile_viewed`, `message_received`, `match`, `unlike_from_match` ;
  - `GET /api/notifications?cursor=`, `GET /api/notifications/unread-count`, `POST /api/notifications/read` ;
  - **pousser** sur le WebSocket un événement `{"type":"notification", …}` et un `unread_count` global ;
  - règles : rien si l'acteur est bloqué ; rien de la part d'un utilisateur qu'on a *unliké* (le sujet le
    précise) ; pas de doublon sur les visites répétées.
  - Un **seul** WebSocket par client, `/api/ws`, qui multiplexe `message`, `notification` et `presence`.
    Aujourd'hui il est nommé `/api/messaging/ws` et réservé aux messages.
- **Notions** : *outbox pattern* (écrire la notification dans la même transaction que l'action, puis la pousser),
  *fan-out*, Redis Pub/Sub si plusieurs instances, délai maximum de 10 s exigé par le sujet.
- **Bibliothèque** :
  - **`cpp-realtime`** : `Hub` (topics par compte), sérialisation d'enveloppe `{type, data}`, adaptateur Crow
    `cpp-realtime-crow`. On y extrait `MessagingWebSocketHub` ;
  - **nouvelle `cpp-notify`** : `NotificationStore` (interface, implémentation PostgreSQL via `cpp-pg`) et
    `NotificationDispatcher` (stocke puis pousse via `cpp-realtime`). Elle ne connaît que des
    `type`/`payload` opaques, Matcha définit les types.

### 3.13 Vérifier qu'on a une photo avant de liker

- **À faire** : dans `POST /api/profiles/<id>/like`, refuser avec `403` et un message explicite si
  `!profileRepository.hasPrimaryPhoto(current)`. Une ligne, mais elle est exigée par le sujet.

### 3.14 Relation dans le profil consulté

- **À faire** : ajouter à `GET /api/profiles/<id>` un bloc
  `relationship: { i_liked, liked_me, connected, blocked }`, `fame_rating`, `online`, `last_seen_at`,
  `distance_km`, `interests`, `common_interests`. Le front n'a plus qu'un appel à faire.

### 3.15 500 profils

- **À faire** : un **générateur** plutôt qu'un fichier SQL figé. Un script ou un binaire `seed_generator` qui
  crée N comptes à partir d'un *seed* aléatoire fixé (donc reproductible) : noms, genres et orientations
  équilibrés, coordonnées réparties autour de villes réelles, 1 à 5 photos (générées ou issues d'un jeu libre
  comme randomuser.me ou thispersondoesnotexist), tags tirés d'un vocabulaire, likes et visites croisés pour
  donner du relief à la popularité.
- **Notions** : `generate_series`, `COPY` pour l'insertion massive, déterminisme (seed), données factices
  (domaine `example.test`, déjà utilisé).
- **Bibliothèque** : utilitaire de projet, mais le chargement via `COPY` mérite une méthode
  `cpp-pg::Connection::copyIn`.

---

## 4. Bugs et failles relevés dans l'existant

Classés par gravité. Les points déjà notés dans `notes.txt` sont signalés par 📝.

### Critiques (risque d'échec à la soutenance)

1. **Identifiants PostgreSQL en clair dans `docker-compose.yml`** (`admin`/`admin`), et port `5432` exposé sur
   toutes les interfaces (`"5432:5432"`). Il faut les passer en variables `.env` et en *secret*, et lier le port
   à `127.0.0.1` ou ne pas l'exposer du tout.
2. **Le backend ne démarre pas tout seul** : `backend/docker-entrypoint.sh` fait `exec sleep infinity` et le build
   est commenté dans le `Dockerfile`. Un correcteur qui lance `docker compose up` n'aura pas d'API. Il faut un
   build multi-étapes (étape de build, puis image d'exécution légère) et un entrypoint qui lance le binaire.
3. **Injection HTML possible dans la page de vérification d'email** 📝 : `AuthEmailRoutes.cpp:41` concatène
   `token` dans l'attribut `value`. `hashEmailVerificationToken` valide le format avant, ce qui protège
   aujourd'hui, mais la sûreté repose sur un effet de bord. Il faut échapper explicitement (fonction
   `htmlEscapeAttribute`) ou supprimer cette page, puisque le front React gère déjà `/verify-email`.

### Importants

4. **`QueryResult::getTimestamp` ne lit jamais les colonnes du schéma** : il n'accepte que `TIMESTAMPOID`
   (`QueryResult.cpp:220`) alors que **toutes** les colonnes sont `TIMESTAMPTZ` (`TIMESTAMPTZOID`). Il renverra
   toujours `nullopt` dès qu'on l'utilisera, pour `last_seen_at` ou les dates de notifications par exemple. Il
   n'est pas couvert par un test.
5. **Une seule connexion PostgreSQL et un mutex global** (`Database.hpp`) :
   - toutes les requêtes du serveur multithreadé passent l'une après l'autre ;
   - **les transactions sont inutilisables** : `execute("BEGIN")` puis `execute(sql)` relâchent le mutex entre
     les deux, donc un autre thread peut glisser une requête *dans* la transaction.
     `DatabaseInitializer::runMigrations` le fait, sans risque au démarrage, mais c'est un piège pour toute
     future transaction ;
   - si la connexion tombe, aucune reconnexion ;
   - le constructeur n'échoue pas après ses tentatives (il logue et continue avec `_connection == nullptr`), et
     la boucle `for (i = 1; i < max)` fait 11 tentatives alors que le message en annonce 12
     (`Database.cpp:30`).

   C'est la raison principale de créer **`cpp-pg`** (pool et transactions RAII).
6. **Concurrence sur le like mutuel** : si A et B se likent **au même instant**, chaque CTE de
   `LikeRepository::like` ne voit pas l'insertion non validée de l'autre, et le match n'est **jamais créé**. Deux
   solutions : un verrou consultatif `pg_advisory_xact_lock(least, greatest)` dans une transaction, ou une
   réconciliation (créer le match aussi lors de la lecture). C'est la même famille de problème que la création
   de conversation hors transaction dans `LikeRoutes.cpp`.
7. **Like et conversation non atomiques** : `like()` puis `ensureDirectConversation()` sont deux requêtes
   séparées. Si la seconde échoue, le match existe sans conversation, ce qui explique la « resynchronisation »
   au démarrage dans `main.cpp`. Même chose pour `unlike()` puis `removeDirectConversation()`. Une transaction
   ou un événement `MatchCreated` géré dans la même transaction règle le problème.
8. **`authenticatedAccountId` transforme `VerificationUnavailable` en `401`** (`AuthRouteSupport.cpp:152`) 📝 :
   si le JWKS est momentanément inaccessible, le front croit que la session est expirée et déconnecte
   l'utilisateur. Il faut renvoyer `503`.
9. **Le backend valide ses propres JWT en se téléchargeant lui-même** en HTTP (`JWT_JWKS_URL=http://backend:18080/...`).
   C'est circulaire (au démarrage, le serveur n'écoute pas encore), et ça impose `allowInsecureHttp=true`. Quand
   l'émetteur et le vérificateur sont le même processus, il faut un **`StaticJwksProvider`** construit depuis la
   clé publique de `RsaTokenSigner`, et garder `HttpJwksProvider` pour les services tiers.
10. **Confiance aveugle dans `X-Real-IP`** (`AuthRouteSupport.cpp:166`) 📝 : le limiteur de connexions peut être
    contourné si le backend est joignable sans passer par Nginx. Il faut ne lire l'en-tête que si
    `remote_ip_address` appartient à une liste de proxies de confiance (configuration `TRUSTED_PROXIES`).
11. **Rotation du refresh token avant la signature du JWT** 📝 (`handleRefreshSession`) : si la signature
    échoue, la session devient irrécupérable. Il faut générer le JWT d'abord.
12. **Token JWT dans l'URL du WebSocket** (`?access_token=`) : il finit dans les logs d'accès Nginx. Deux
    alternatives : un ticket WebSocket à usage unique et courte durée (`POST /api/ws-ticket` qui renvoie un
    jeton opaque valable 30 s), ou un premier message d'authentification après l'ouverture.
13. **Validation incomplète du profil** :
    - `sexual_orientation` accepte n'importe quelle chaîne (seule la vacuité est testée) ;
    - `biography` n'a **pas de longueur maximale** ;
    - `gender` accepte tout `[A-Za-z_]+`.

    Il faut des énumérations côté C++ **et** des `CHECK` en base, plus une limite de biographie (par exemple
    500 caractères UTF-8).
14. **Regex de ville avec des caractères non ASCII dans `std::regex`** (`ProfileRoutes.cpp`) : `std::regex`
    travaille en octets, donc `[À-Ö]` en UTF-8 est interprété comme une plage d'octets imprévisible. En plus,
    les regex sont recompilées à chaque requête. Il faut valider avec `utf8CharacterCount` et une liste de
    catégories, ou des regex `static const` sans plage non ASCII. La regex SQL `[[:alpha:]]` et la regex C++ ne
    sont d'ailleurs pas équivalentes.

### Mineurs / qualité

15. Code hérité encore compilé : table `users`, `UserRepository`, `user_repository_integration_tests` et
    `auth_integration_tests` testent l'ancien chemin. Le README parle d'une bascule « à venir ». Il faut le
    supprimer (migration `DROP TABLE users`) pour que le code reflète le comportement réel.
16. Trou de numérotation dans les migrations (pas de `006_…`). Sans conséquence technique, mais à documenter ou
    à combler.
17. `GET /api/likes`, `/api/likes/received` et `/api/matches` renvoient des ids non paginés : le client doit
    faire N appels pour afficher des cartes.
18. `unlike` supprime la conversation et tout l'historique (`ON DELETE CASCADE`). Le sujet dit « chat
    désactivé », pas « supprimé ». Il faut choisir et documenter (un `archived_at` permettrait de garder
    l'historique en lecture seule).
19. Aucun drapeau d'avertissement du compilateur (`-Wall -Wextra -Wpedantic -Werror`) ni de cible
    *sanitizer* déclarée dans `CMakeLists.txt` (un dossier `build-sanitize` existe pourtant). C'est
    incompatible avec la règle « aucun warning » si on ne peut pas le prouver.
20. Les envois d'email sont synchrones dans le handler HTTP 📝 : SMTP lent égale requête lente. Prévoir une
    *outbox* ou une file.
21. `/api/auth/me` ne renvoie que les claims du JWT. Un `GET /api/me` qui renvoie le profil complet et l'état
    d'onboarding (profil complet ? localisation ?) simplifierait le front.
22. Pas d'endpoint de santé (`/health`, `/ready`) pour le `healthcheck` Docker, pas d'arrêt propre (SIGTERM).
23. La documentation de l'API est dispersée dans le README. Un fichier **OpenAPI** (`docs/openapi.yaml`)
    servirait à la fois de contrat front/back et de démonstration en entretien.

---

## 5. Architecture

### 5.1 Ce qui est bien

- **Composition root** dans `main.cpp` : toutes les dépendances sont construites et injectées au même endroit.
- **Interfaces** là où ça compte : `IImageStorage`, `IUserLookupRepository`, `IPasswordHasher`, `JwksProvider`,
  `IClock`.
- **Séparation compte et profil** (`auth_accounts` / `matcha_profiles`) : excellente décision pour sortir
  l'authentification dans une bibliothèque.
- **Migrations versionnées** avec verrou consultatif.
- Messagerie **indépendante du domaine**.

### 5.2 Ce qui freine la maintenance et l'évolution

| Problème | Exemple | Conséquence | Piste |
|---|---|---|---|
| Logique métier dans les handlers HTTP | `LikeRoutes.cpp` décide de créer la conversation ; `ProfileRoutes.cpp` valide et calcule la complétude | Impossible à tester sans HTTP, duplication garantie quand on ajoutera notifications et blocage | Couche **services / cas d'usage** : `LikeService::like(actor, target)` renvoie un `Result` |
| Authentification répétée dans chaque route | `authenticatedAccountId` + `unauthorized()` copiés partout | Oubli facile, et mauvais code d'erreur (point 4.8) | **Middleware Crow** `RequireAuth` qui remplit un contexte `ctx.accountId` |
| Gestion d'erreurs hétérogène | `bool`, `std::optional`, `throw std::runtime_error`, structs `…Result{Status}` | Le handler doit connaître 3 styles ; les erreurs SQL deviennent des `500` génériques | **`std::expected<T, Error>`** (C++23 déjà activé) partout côté repository et service ; une table `Error → HTTP status` dans l'adaptateur |
| Règle de complétude du profil copiée 3 fois en SQL | `findProfile`, `listCompletedProfiles`, `updateProfile`, `refreshCompletion` | Une modification doit être faite à 4 endroits | Colonne générée, vue SQL `complete_profiles`, ou fonction SQL `is_profile_complete(account_id)` |
| Deux bibliothèques JSON | `crow::json` dans le backend, `nlohmann::json` dans `cpp-auth` et dans `vcpkg.json` | Deux API à connaître | Garder **nlohmann** dans les cœurs de bibliothèques (indépendant du framework), `crow::json` seulement dans les adaptateurs |
| Configuration lue dans `main.cpp` et dans `Database()` | `std::getenv` à deux endroits | Configuration dispersée, difficile à tester | Struct `AppConfig` chargée **une fois**, validée, injectée |
| Logs `std::cerr` | partout | Pas de niveaux, pas de corrélation, risque de logguer des données sensibles | Petit `ILogger` (ou **spdlog**) avec niveaux et `request_id` |
| Tests | Rien sur `ProfileRepository`, `LikeRepository`, `MessagingRepository` ni sur les routes | Les fonctionnalités Matcha ne sont pas protégées contre les régressions | Tests d'intégration repository sur une base de test, tests de services avec des faux, quelques tests HTTP de bout en bout |

### 5.3 Organisation cible proposée

```
backend/
├── libs/                         # réutilisable, sans "matcha" dans le code
│   ├── cpp-auth/                 # (existant, déplacé)
│   ├── cpp-auth-crow/            # middleware RequireAuth, routes JWKS
│   ├── cpp-pg/                   # pool, transactions, QueryBuilder, migrations
│   ├── cpp-realtime/             # Hub, Presence, enveloppes d'événements
│   ├── cpp-realtime-crow/        # adaptateur WebSocket Crow
│   ├── cpp-messaging/            # (extrait de src/messaging)
│   ├── cpp-notify/               # stockage et diffusion de notifications
│   ├── cpp-media/                # ImageProcessor, IImageStorage, LocalImageStorage
│   ├── cpp-mail/                 # IEmailSender, SMTP, templates, outbox
│   ├── cpp-geo/                  # Haversine, bbox, arrondi, IGeocoder
│   ├── cpp-validation/           # UTF-8, email, username, politique de mot de passe
│   └── cpp-core/                 # Config, Logger, Error/Result, Clock
├── src/
│   ├── app/                      # main, composition root, AppConfig
│   ├── domain/                   # règles pures : Orientation, FameCalculator, MatchingScore
│   ├── services/                 # cas d'usage : LikeService, ProfileService, SearchService…
│   ├── repositories/             # SQL Matcha (utilise cpp-pg)
│   ├── events/                   # LikeCreated, MatchCreated, ProfileViewed… et leurs handlers
│   └── http/                     # handlers fins : parse → service → réponse
├── migrations/
└── tests/
```

**Flux type** (exemple : un like) :

```
HTTP POST /like ─► RequireAuth (cpp-auth-crow) ─► LikeHandler (parse)
   ─► LikeService::like(actor, target)            [transaction cpp-pg]
         ├─ policy : actor a une photo, pas de blocage, target complète
         ├─ LikeRepository::insert
         ├─ si réciproque : MatchRepository::insert + Messaging::openConversation
         └─ publie LikeCreated / MatchCreated dans l'outbox (même transaction)
   ─► après commit : EventDispatcher ─► FameUpdater, NotificationDispatcher ─► Hub WebSocket
```

Ajouter une fonctionnalité (par exemple « super like ») revient alors à ajouter **un cas d'usage, un événement et
un handler**, sans toucher aux autres modules. C'est l'argument « facile à maintenir et à faire évoluer » qu'on
peut présenter en entretien.

---

## 6. Bibliothèques

### 6.1 `cpp-auth` (existante) : bilan et pistes

**Contenu actuel** : génération et validation de JWT RS256, JWKS (parseur, cache, provider HTTP), client HTTP,
parseur d'URL, `AuthService` (inscription et login), `SessionManager` avec store en mémoire, horloges
injectables. Tests nombreux, package CMake installable.

**Constats**

| # | Constat | Proposition |
|---|---|---|
| 1 | `SessionManager`/`InMemorySessionStore` ne sont **pas utilisés** par le backend : les sessions et la rotation des refresh tokens sont réimplémentées dans `AccountRepository` | Remonter la logique *refresh token* (rotation, détection de réutilisation, expiration idle/absolue) dans `cpp-auth` sous forme de `RefreshTokenService` + interface `IRefreshTokenStore`. Matcha ne fournit que l'implémentation PostgreSQL. C'est **la** partie la plus réutilisable du projet, et elle est aujourd'hui enfermée dans le backend. |
| 2 | `PasswordHasher` (Argon2id/libsodium), `OpaqueToken`, `EmailVerificationToken` sont dans le backend | Ajouter un module optionnel `cpp-auth::sodium` : `Argon2idPasswordHasher` et `OpaqueTokenGenerator` (aléatoire, hash SHA-256, validation de format). Fusionner les deux types de tokens. |
| 3 | `AuthService` mélange inscription (`IUserRepository`) et login (`IUserLookupRepository`) avec deux constructeurs et un pointeur nullable | Séparer `LoginService` et `RegistrationService`. Login par *identifier* générique (email **ou** username), vérification factice à temps constant. |
| 4 | Double validation : `AuthService::isValidEmail/isValidPwd` **et** `auth_routes::validEmail/validPassword` | Une seule `IPasswordPolicy`/`IEmailValidator` injectée, fournie par `cpp-validation`. |
| 5 | Validation de ses propres tokens via HTTP | `StaticJwksProvider(publicJwk)` ou `LocalKeyProvider(RsaTokenSigner&)`. |
| 6 | Pas de révocation des access tokens (15 min de validité après un logout) | Option `jti` + `IRevocationList` (implémentation Redis avec TTL égal à la durée de vie restante), ou claim `sid` vérifié contre la session à chaque requête sensible. |
| 7 | `RS256` seulement | Ouvrir à **EdDSA (Ed25519)** ou **ES256** via `ITokenSigner`/`ITokenSignatureVerifier`, déjà abstraits. Les clés sont plus courtes, la signature plus rapide, et c'est un bon sujet de discussion en entretien. |
| 8 | Rotation des clés de signature | Le JWKS supporte plusieurs `kid`. Ajouter au signer la notion de clé active et de clés retirées encore publiées. |
| 9 | `AuthenticatedIdentity` ne contient que `subject`, `issuer`, `audience` | Ajouter `issuedAt`, `expiresAt`, `jti` et des claims personnalisés (`roles`, `sid`) pour les besoins futurs (admin, modération des signalements). |
| 10 | Le rate limiter de connexion est dans le backend et couplé à Redis | Interface `IAttemptLimiter` dans `cpp-auth` (ou `cpp-ratelimit`), avec une implémentation Redis séparée (`cpp-auth-redis`) et une en mémoire pour les tests. |
| 11 | Pas d'adaptateur HTTP | `cpp-auth-crow` : middleware `RequireAuth`, `bearerToken()`, `refreshCookie()`/`requestCookie()`, route JWKS, contrôle d'`Origin`. Tout cela est aujourd'hui dans `AuthRouteSupport.cpp`. |
| 12 | Bonus du sujet : OmniAuth | Module `cpp-auth::oauth2` : flux *Authorization Code + PKCE*, découverte OIDC (`.well-known/openid-configuration`), validation de l'`id_token` **avec le `TokenValidator` + `HttpJwksProvider` existants** (ils sont déjà faits pour ça). C'est la suite la plus naturelle de la bibliothèque, et un bonus 42 au passage. |
| 13 | Documentation | Le README est bon. Il faudrait un **CHANGELOG**, du *semver*, des exemples compilés (`examples/`) et un schéma des composants. |
| 14 | `CPP_AUTH_BUILD_TESTS` vaut `ON` quand la bibliothèque est incluse comme sous-projet | Mettre `OFF` par défaut dans tous les cas : un consommateur ne veut pas compiler les tests d'une dépendance. |

### 6.2 `cpp-pg` (nouvelle, **priorité 1**) : extraite de `src/database`

Le sujet la suggère explicitement, et elle débloque transactions, recherche et notifications.

- **`ConnectionPool`** : N connexions `PGconn`, `acquire()` qui renvoie un `PooledConnection` RAII rendu au pool
  à la destruction, reconnexion automatique (`PQreset`) si `PQstatus != CONNECTION_OK`, délai d'attente
  configurable.
- **`Transaction`** RAII : `auto tx = pool.begin();` … `tx.commit();`, avec `ROLLBACK` automatique dans le
  destructeur. Niveaux d'isolation et `SAVEPOINT` en option.
- **Paramètres typés** : `tx.query(sql, id, name, std::optional<int>{}, std::vector<std::string>{…})` avec
  conversion automatique (`int64`, `bool`, `optional` vers `NULL`, `vector` vers un tableau PostgreSQL,
  `chrono` vers un horodatage ISO), ce qui supprime les `std::to_string` manuels.
- **`Row`/`Result`** : `row.get<std::int64_t>("id")`, accès **par nom de colonne** (aujourd'hui par index, donc
  fragile quand on ajoute une colonne). Gestion de `TIMESTAMPTZ` (corrige le point 4.4).
- **`QueryBuilder`** minimal : `where("age >= ?", 18).whereIf(opt, "fame >= ?", *opt).orderBy(SortKey::Fame, whitelist).limit(n)`,
  qui numérote les `$n`. Ce n'est **pas** un ORM (règle du sujet respectée).
- **`MigrationRunner`** : `DatabaseInitializer` déplacé ici, avec somme de contrôle des fichiers appliqués
  (détecter une migration modifiée après application), mode `--dry-run`.
- **`LISTEN/NOTIFY`** (plus tard) : utile pour `cpp-notify` sans Redis.
- **Tests** : base PostgreSQL éphémère (conteneur), chaque test dans une transaction annulée.
- **Notions à maîtriser** : protocole libpq, *prepared statements* (`PQprepare`), mode pipeline libpq 14+,
  isolation `READ COMMITTED` contre `SERIALIZABLE`, `SKIP LOCKED` pour les files.

### 6.3 `cpp-realtime` + `cpp-realtime-crow` (nouvelle, **priorité 1**) : extraite de `MessagingWebSocket`

- Cœur : `Hub` (sessions par sujet, ici `account:{id}`), `publish(topic, Envelope{type, json})`,
  `PresenceTracker` (en mémoire ou Redis), interface `IBroker` (en mémoire, Redis Pub/Sub pour plusieurs
  instances).
- Adaptateur Crow : authentification à l'ouverture (ticket), *heartbeat*, routage des messages entrants par
  `type` vers des handlers enregistrés.
- **Points à corriger dans l'existant** :
  - `send_text` est appelé **sous le mutex global** du hub, donc un client lent bloque tout le monde : copier la
    liste des connexions puis envoyer hors verrou ;
  - pointeurs bruts `crow::websocket::connection*` et `new std::int64_t` dans `userdata` : préférer une
    structure de session possédée par le hub ;
  - aucune limite de débit au niveau WebSocket en dehors de la contrainte SQL ;
  - aucun message d'erreur renvoyé au client : un message refusé est ignoré silencieusement.

### 6.4 `cpp-messaging` (existante dans `src/messaging`) : à extraire

Elle est déjà générique. Il manque :
- une interface `IConversationPolicy` (règles d'envoi fournies par l'application : match actif, pas de blocage) ;
- une conversation **archivable** plutôt que supprimée ;
- des accusés de lecture par message, plus « en train d'écrire… » (bonus UX) ;
- une dépendance à `cpp-pg` au lieu du `Database` local ;
- le `MessagingWebSocket` déplacé dans `cpp-realtime-crow`, avec un simple handler `type == "message"`.

### 6.5 `cpp-notify` (nouvelle, priorité 1)

`Notification{recipient, actor, type, payload, createdAt, readAt}`, `INotificationStore` (PostgreSQL),
`INotificationFilter` (fourni par Matcha : blocage, unlike), `NotificationDispatcher` (stocke puis publie via
`cpp-realtime`), compteurs non lus, regroupement optionnel (« 3 personnes ont visité votre profil »).

### 6.6 `cpp-media` (existante dans `src/images`) : à extraire

- `ImageProcessor` (libvips) et `IImageStorage` + `LocalImageStorage` sont génériques ; `ProfilePhotoService`
  est métier et reste dans `src/`.
- Pistes : variantes (miniature, moyenne, grande) générées à l'upload ; suppression des métadonnées EXIF (vérifier
  que le ré-encodage WebP les retire bien, la position GPS d'une photo est une donnée sensible) ; implémentation
  `S3ImageStorage` (MinIO en local) ; **URLs signées** à durée limitée pour servir les images sans `Authorization`
  (aujourd'hui `GET /api/profile/photos/<id>` exige un Bearer, ce qui oblige le front à télécharger chaque image
  en `blob`) ; opérations de recadrage et de rotation, qui couvrent le bonus « édition d'image ».

### 6.7 `cpp-mail` (existante dans `src/services/SmtpEmailSender`) : à extraire

- Interface `IEmailSender` (SMTP, implémentation « console » pour les tests), templates texte et HTML avec
  échappement, en-têtes `Message-ID`/`Date`, **outbox** (table `email_outbox` + worker qui réessaie avec
  *backoff*), ce qui règle aussi le point 4.20.
- La garde actuelle (`TLS requis sauf mailpit`) est bonne mais codée en dur sur le nom d'hôte `mailpit` :
  la passer en option de configuration explicite (`allowInsecureForHosts`).

### 6.8 `cpp-geo` (nouvelle, priorité 2)

Voir 3.5. Petite bibliothèque, fonctions pures, très facile à tester et à présenter.

### 6.9 `cpp-validation` (nouvelle, priorité 2) : extraite de `AuthRouteSupport`

`utf8::length/isValid`, `EmailValidator` (syntaxe raisonnable, normalisation), `UsernameValidator`,
`PersonNameValidator`, `PasswordPolicy` (longueur et dictionnaire, voir 3.2), `HtmlEscaper`. Ensuite, un petit
validateur de JSON par schéma (champs requis, types, bornes) pour remplacer les séries de
`body.has(...) && body[...].t() == ...` copiées dans chaque handler.

### 6.10 `cpp-core` (nouvelle, priorité 2)

`Config` (variables d'environnement et fichiers de secrets, avec la logique actuelle de
`requiredSecretFileEnvironmentVariable`), `Logger` (niveaux, `request_id`, masquage des champs sensibles),
`Error`/`Result<T>` sur `std::expected`, `IClock` (aujourd'hui dans `cpp-auth`, à partager).

> **Attention à la sur-ingénierie** : il vaut mieux 5 bibliothèques solides, testées et documentées que 12
> squelettes. Ordre conseillé : `cpp-pg`, puis `cpp-realtime`, puis `cpp-notify`, puis la refonte de `cpp-auth`,
> et le reste au fil de l'eau.

### 6.11 Standard commun à toutes les bibliothèques

Pour qu'elles soient crédibles sur un CV :
- `include/<nom>/…` public, `src/` privé, *namespace* propre, aucune référence à Matcha ;
- `CMakeLists.txt` installable + `<nom>Config.cmake` + `tests/package-consumer` (comme `cpp-auth`) ;
- tests GoogleTest + couverture, avec *sanitizers* (ASan/UBSan) dans une configuration CI ;
- `README` (but, exemple minimal, garanties, limites), `CHANGELOG`, version *semver* ;
- **CI GitHub Actions** : build, tests, clang-tidy, clang-format ;
- dépendances déclarées dans un `vcpkg.json` propre à chaque bibliothèque.

---

## 7. Plan d'action priorisé

### P0 : mandatory, bloquant pour la soutenance

1. Secrets PostgreSQL hors de `docker-compose.yml` ; backend démarré automatiquement (Dockerfile multi-étapes).
2. `cpp-pg` : pool et transactions (prérequis de tout le reste) ; correction de `getTimestamp`.
3. Login par username ; politique de mot de passe avec dictionnaire.
4. Modification du nom, du prénom et de l'email ; validation stricte du genre, de l'orientation et de la
   biographie.
5. Tags (repository, routes, transaction).
6. Géolocalisation (lat/lon arrondies, consentement, ville manuelle géocodée).
7. Visites, blocage, signalement, présence et `last_seen`.
8. Popularité.
9. Suggestions et recherche (compatibilité d'orientation, score, tri, filtres, exclusion des bloqués).
10. Notifications temps réel + WebSocket unifié + compteurs non lus.
11. Like interdit sans photo ; champ `relationship` dans le profil.
12. Générateur de 500 profils.

### P1 : sécurité et robustesse

- Proxies de confiance pour `X-Real-IP` ; ticket WebSocket au lieu du JWT dans l'URL ; `503` sur
  `VerificationUnavailable` ; `StaticJwksProvider` ; JWT généré avant la rotation du refresh token ; échappement
  HTML de la page de vérification ; concurrence du like mutuel ; `-Wall -Wextra -Werror` et *sanitizers* en CI.

### P2 : maintenabilité et portfolio

- Couche services et événements, middleware d'authentification, `std::expected`, suppression du code hérité
  `users`, OpenAPI, tests d'intégration des repositories Matcha, extraction progressive des bibliothèques vers
  `libs/`, CI GitHub Actions.

### P3 : bonus du sujet (seulement si P0 est parfait)

- OAuth2/OIDC dans `cpp-auth` ; édition d'image dans `cpp-media` ; carte interactive (avec `cpp-geo`) ;
  rendez-vous (nouveau module métier) ; appel vidéo (WebRTC, signalisation via `cpp-realtime`).
