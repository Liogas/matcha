# Guide 05 : cpp-pg (1/2) : connexion, pool, transactions, lignes typées

> Planning : mardi, « cpp-pg » (pool, transactions, paramètres typés, lecture par nom).
> Durée : environ 4 h, lecture comprise. C'est le plus gros guide jusqu'ici : fais des
> pauses entre les étapes, chacune se termine sur des tests verts.
>
> Prérequis : le guide 04 est terminé (cpp-core : `Result`, `ConfigReader`, `ILogger`).
>
> Même principe : je te dis **exactement** quoi faire et **pourquoi**, c'est toi qui
> tapes. Tout a été vérifié sur une copie de ton dépôt dans l'image `matcha_backend:dev`
> (GCC 12.2, CMake 3.25) avec un PostgreSQL 17 jetable : **chaque étape** compilée et
> testée seule, 225 tests en `debug` et en `sanitize`, la lib installée hors du monorepo
> avec son package-consumer, et le service `postgres-test` lancé par `docker compose`.
>
> Questions de fin au §12, correction au §13.

## Sommaire

0. [Ce qu'on fait, et le résultat attendu](#0-ce-quon-fait-et-le-résultat-attendu)
1. [Les notions à connaître avant de commencer](#1-les-notions-à-connaître-avant-de-commencer)
2. [Étape A : partir propre](#2-étape-a--partir-propre)
3. [Étape B : une base PostgreSQL pour les tests](#3-étape-b--une-base-postgresql-pour-les-tests)
4. [Étape C : générer cpp-pg et déclarer ses dépendances](#4-étape-c--générer-cpp-pg-et-déclarer-ses-dépendances)
5. [Étape D : `Settings` et `SqlState`](#5-étape-d--settings-et-sqlstate)
6. [Étape E : `Convert` et `Params`](#6-étape-e--convert-et-params)
7. [Étape F : `Rows` et `Connection`](#7-étape-f--rows-et-connection)
8. [Étape G : `ConnectionPool` et `Transaction`](#8-étape-g--connectionpool-et-transaction)
9. [Étape H : installation, README, CHANGELOG](#9-étape-h--installation-readme-changelog)
10. [Étape I : vérifier tout le parcours](#10-étape-i--vérifier-tout-le-parcours)
11. [Étape J : commiter](#11-étape-j--commiter)
12. [Questions](#12-questions)
13. [Correction](#13-correction)

---

## 0. Ce qu'on fait, et le résultat attendu

cpp-pg est la **priorité 1** de l'audit (§6.2). Aujourd'hui, tout le backend parle à
PostgreSQL à travers `src/database/` : **une** connexion, protégée par **un** mutex. Ce
guide construit son remplaçant, en couche 1, au-dessus de cpp-core :

| Brique | Rôle | Corrige |
|---|---|---|
| `Connection` | une connexion qui se ferme toute seule, des requêtes à paramètres typés | `Database` qui continue avec un pointeur nul (audit 4.5) |
| `ConnectionPool` | N connexions prêtées une par une, avec un délai d'attente maximum | toutes les requêtes du serveur qui passent l'une après l'autre (audit 4.5) |
| `Transaction` | `BEGIN` … `commit()`, `ROLLBACK` automatique, la même connexion du début à la fin | des transactions **inutilisables** aujourd'hui (audit 4.5) |
| `Rows`, `Row::get<T>("nom")` | lire une colonne **par son nom**, `NULL` en `std::optional`, `TIMESTAMPTZ` en `pg::Timestamp` | l'accès par index, et `getTimestamp` qui ne lit jamais les colonnes du schéma (audit 4.4) |
| `errorCodeFor` | un code SQLSTATE (`23505`…) devient un `core::ErrorCode` | les `sqlState() == "23505"` dispersés dans les repositories |

**Ce guide ne touche pas encore au backend.** `src/database/` et les repositories
continuent de tourner tels quels. Le guide 06 ajoutera le `QueryBuilder` et les
migrations, puis branchera cpp-pg partout et supprimera `src/database/` : c'est là
que cpp-pg « remplacera du vrai code », comme l'exige la définition de terminé.

```
docker-compose.dev.yml          ← service postgres-test (base jetable des tests)
backend/libs/
├── CMakeLists.txt              ← add_subdirectory(cpp-pg) APRÈS cpp-core
└── cpp-pg/
    ├── include/pg/
    │   ├── Version.hpp         (du gabarit)
    │   ├── Settings.hpp        ConnectionSettings, readSettings()
    │   ├── SqlState.hpp        errorCodeFor()
    │   ├── Convert.hpp         toText() / fromText(), pg::Timestamp
    │   ├── Params.hpp          Params, pg::params(args...)
    │   ├── Rows.hpp            Rows, Row::get<T>(), RowError
    │   ├── Connection.hpp      Connection
    │   └── Pool.hpp            ConnectionPool, PooledConnection, Transaction
    ├── src/                    un .cpp par en-tête
    └── tests/
        ├── support/PgTest.*    la base commune des tests qui parlent à PostgreSQL
        └── 7 fichiers de tests
```

À la fin :

| Vérification | Résultat attendu |
|---|---|
| `make test` | 225 tests (193 + 32 de cpp-pg) |
| `make sanitize` | 225 tests |
| tests de cpp-pg **sans** `PG_TEST_HOST` | les tests qui ont besoin d'une base sont **sautés**, pas en échec |
| cpp-pg seule, installée avec cpp-core | 13 fichiers, package-consumer `OK` |
| deux transactions sur deux threads | chacune ne voit que ses lignes |
| pool épuisé | erreur `Unavailable` après le délai, pas de blocage |

---

## 1. Les notions à connaître avant de commencer

### 1.1 Ce qui ne va pas aujourd'hui

Ouvre `backend/src/database/Database.hpp` : une seule connexion (`PGconn *_connection`) et
un seul mutex. Chaque appel à `execute` prend le mutex, envoie **une** requête, et le
relâche. Regarde ce qui se passe si un repository veut faire une transaction pendant
qu'une autre requête HTTP arrive :

```
thread A (like)                     connexion unique              thread B (profil)
───────────────                     ────────────────              ─────────────────
execute("BEGIN")          ──lock──▶  BEGIN            ──unlock
                                                       ◀──lock──  execute("UPDATE profiles…")
                                     UPDATE profiles…  ──unlock      ↑ DANS la transaction de A !
execute("INSERT likes…")  ──lock──▶  INSERT likes…    ──unlock
execute("ROLLBACK")       ──lock──▶  ROLLBACK                     ← annule AUSSI l'UPDATE de B
```

Le mutex protège **chaque requête**, pas la **suite** de requêtes. Une transaction a
besoin de garder la connexion pour elle, du `BEGIN` au `COMMIT`. Avec une seule
connexion, ça voudrait dire bloquer **tout** le serveur pendant ce temps.

Trois autres défauts relevés par l'audit (4.4, 4.5) :

- le constructeur fait 11 tentatives (`for (i = 1; i < max; …)`) en annonçant 12, puis
  **continue** avec `_connection == nullptr` : le serveur démarre sans base ;
- si la connexion tombe, rien ne la rétablit ;
- `QueryResult::getTimestamp` n'accepte que `TIMESTAMP`, alors que toutes les colonnes du
  schéma sont `TIMESTAMPTZ` : il renvoie toujours `nullopt`.

La réponse classique : un **pool** de connexions. Quand une requête ou une transaction
a besoin de la base, elle **emprunte** une connexion, s'en sert seule, puis la **rend**.

### 1.2 libpq en six fonctions

libpq est la bibliothèque C officielle de PostgreSQL. C'est une API C : des pointeurs
opaques, et des fonctions pour les créer et les libérer.

| Fonction | Rôle |
|---|---|
| `PQconnectdbParams(clés, valeurs, 0)` | ouvre une connexion, renvoie un `PGconn *` (jamais nul, sauf mémoire épuisée) |
| `PQstatus(conn)` | `CONNECTION_OK` ou `CONNECTION_BAD` |
| `PQexecParams(conn, sql, n, types, valeurs, longueurs, formats, formatRésultat)` | envoie **une** requête avec ses paramètres `$1..$n`, renvoie un `PGresult *` |
| `PQresultStatus(res)` | `PGRES_TUPLES_OK` (des lignes), `PGRES_COMMAND_OK` (pas de lignes), ou une erreur |
| `PQclear(res)` | libère un résultat |
| `PQfinish(conn)` | ferme et libère une connexion |

Trois choses à retenir sur `PQexecParams` :

- **les valeurs voyagent à part du SQL**. `"SELECT … WHERE email = $1"` et la valeur
  `alice@example.com` partent séparément : PostgreSQL ne les mélange jamais. Une valeur
  `x'); DROP TABLE items; --` reste une **donnée**. C'est la seule vraie protection
  contre l'injection SQL. Un test le vérifiera (§7.6) ;
- **une seule instruction** par appel : `"SELECT 1; DROP TABLE x"` est refusé. Encore une
  porte fermée aux injections ;
- **format texte ou binaire**. En texte, `42` part comme la chaîne `"42"` et revient
  comme `"42"`. En binaire, il faut connaître l'encodage interne de chaque type. On
  choisit le **texte** partout : plus simple, lisible au débogage, et PostgreSQL déduit
  le type de chaque `$n` d'après la requête (`$1::bigint`, ou la colonne comparée).

Et pour les erreurs : `PQresultErrorField(res, PG_DIAG_SQLSTATE)` donne le code à 5
caractères (§1.7), `PG_DIAG_MESSAGE_PRIMARY` le message.

### 1.3 RAII : posséder une ressource C

`PGconn *` et `PGresult *` doivent être libérés **exactement une fois**. Oublier `PQclear`
est une fuite (LSan la verra), le faire deux fois est un *double free* (ASan le verra).
La règle du C++ : chaque ressource a **un propriétaire**, un objet qui la libère dans son
destructeur. C'est le **RAII** (*Resource Acquisition Is Initialization*).

`QueryResult` le fait déjà, à la main : destructeur, constructeur de déplacement,
opérateur de déplacement, copie supprimée. Soit une trentaine de lignes, où une erreur
est vite arrivée. cpp-pg délègue ce travail à `std::unique_ptr` avec un **deleter** :

```cpp
struct Close
{
    void operator()(pg_conn *connection) const noexcept { PQfinish(connection); }
};
std::unique_ptr<pg_conn, Close> _connection;
```

`unique_ptr` sait déjà être **déplacé** (le pointeur change de propriétaire, l'ancien
devient nul) et **pas copié** (deux propriétaires = double libération). Une classe dont
le seul membre « propriétaire » est un `unique_ptr` hérite de ces règles **sans écrire
une seule ligne** : c'est la *règle du zéro*. `Connection` n'a ni destructeur, ni
constructeur de copie ou de déplacement écrits à la main.

Pour un résultat, on veut parfois **plusieurs** propriétaires : une `Row` doit pouvoir
survivre à ses `Rows` (§1.8). On prend alors `std::shared_ptr<pg_result>(res, PQclear)` :
le résultat est libéré quand le **dernier** propriétaire disparaît.

**`struct pg_conn;` au lieu de `#include <libpq-fe.h>`.** Dans libpq,
`PGconn` est un alias de `struct pg_conn`. Nos en-têtes publics **déclarent** juste ce
nom (déclaration anticipée) sans inclure libpq. Un `unique_ptr` vers un type incomplet
fonctionne, tant que le deleter est **défini** dans le `.cpp`. Conséquences :

- un projet qui utilise cpp-pg n'a pas besoin des en-têtes de libpq pour **compiler** :
  on lie libpq en `PRIVATE` ;
- mais il en a besoin pour **l'édition de liens** : la lib est statique, `libcpp-pg.a`
  contient des appels à `PQexecParams` sans leur code. D'où le
  `find_dependency(PostgreSQL)` dans `cpp-pgConfig.cmake.in`, même pour une dépendance
  `PRIVATE`. C'est exactement la question 3 du guide 02.

### 1.4 Le pool : emprunter, rendre, attendre

Un `ConnectionPool` ouvre N connexions au démarrage et les range dans une liste. Trois
opérations :

- **emprunter** (`acquire`) : prendre une connexion de la liste. Si la liste est vide,
  **attendre** qu'on en rende une, mais pas plus de `timeout` ;
- **rendre** : automatique. `acquire` renvoie un `PooledConnection`, dont le
  destructeur remet la connexion dans la liste (RAII, encore) ;
- **réparer** : une connexion rendue cassée (serveur redémarré) est reconnectée avec
  `PQreset`. Une connexion rendue au milieu d'une transaction reçoit un `ROLLBACK`.

**Attendre sans tourner en boucle** : `std::condition_variable`. Un thread qui n'a pas de
connexion **dort** ; celui qui en rend une le **réveille** :

```cpp
std::unique_lock lock(_mutex);
// Dort jusqu'à ce que la liste ne soit plus vide, ou que le délai expire.
// Renvoie false si le délai a expiré et que la condition est toujours fausse.
bool ok = _available.wait_for(lock, timeout, [this] { return !_idle.empty(); });
```

- `wait_for` **relâche** le mutex pendant qu'il dort, et le **reprend** avant de rendre
  la main. Sans ça, personne ne pourrait rendre de connexion ;
- le **prédicat** (la lambda) est obligatoire en pratique : un thread peut se réveiller
  sans raison (*spurious wakeup*), ou un autre thread peut avoir pris la connexion juste
  avant lui. `wait_for` revérifie la condition à chaque réveil ;
- côté rendu : on remet la connexion **sous le verrou**, puis `notify_one()` réveille
  **un** thread en attente.

**Pourquoi un délai maximum** : sans lui, un pool épuisé (une fuite de transactions, un
pic de trafic) bloquerait les threads de Crow **pour toujours**. Avec lui, la requête
HTTP reçoit une erreur `Unavailable` (un futur `503`) et le serveur reste vivant.

**Pourquoi `open` renvoie un `std::unique_ptr<ConnectionPool>`** : chaque
`PooledConnection` garde un pointeur vers son pool, pour savoir où se rendre. Si le pool
était **déplacé** (renvoyé par valeur dans un `Result`), ces pointeurs viseraient
l'ancienne adresse. Un `unique_ptr` fixe l'adresse du pool : on déplace le pointeur,
jamais l'objet. Et le pool doit **vivre plus longtemps** que tout ce qu'il prête : c'est
la question 6 du guide 04, en plus grave (son destructeur le vérifie avec un `assert`).

### 1.5 Les transactions

```sql
BEGIN;                                    -- ouvre la transaction
INSERT INTO likes …;                      -- invisible pour les autres connexions…
INSERT INTO matches …;
COMMIT;                                   -- …jusqu'ici : tout devient visible d'un coup
```

`ROLLBACK` à la place de `COMMIT` annule tout. C'est **tout ou rien** (l'atomicité) :
c'est ce qui manquera au like mutuel jusqu'au guide 06 (audit 4.6, 4.7).

**L'isolation** dit ce qu'une transaction voit des **autres** transactions en cours :

| Niveau | Ce que voit chaque requête | Coût |
|---|---|---|
| `READ COMMITTED` (défaut de PostgreSQL) | tout ce qui était **validé** au début de **cette requête** | aucun : deux `SELECT` de la même transaction peuvent voir des choses différentes |
| `REPEATABLE READ` | une **photo** prise à la première requête de la transaction | une écriture en conflit échoue avec `40001` |
| `SERIALIZABLE` | comme si les transactions passaient **l'une après l'autre** | encore plus d'échecs `40001`, que l'application doit **réessayer** |

Aucun niveau ne montre ce qu'une autre transaction n'a **pas encore validé**. Un test le
vérifiera avec deux threads (§8.5). cpp-pg prend `READ COMMITTED` par défaut, comme
PostgreSQL.

**Deux pièges de PostgreSQL**, que `Transaction` doit gérer :

1. **Une erreur empoisonne la transaction.** Après une requête en erreur (un doublon, par
   exemple), **toutes** les suivantes sont refusées avec
   `current transaction is aborted, commands ignored until end of transaction block`
   (SQLSTATE `25P02`), jusqu'au `ROLLBACK`.
2. **`COMMIT` peut « réussir » sans rien valider.** Sur une transaction empoisonnée,
   PostgreSQL répond à `COMMIT` par… `ROLLBACK`, **sans erreur**. Je l'ai vérifié :
   `PQresultStatus` vaut `PGRES_COMMAND_OK`, et seul `PQcmdStatus` dit `"ROLLBACK"`.
   Un `commit()` naïf dirait « c'est enregistré » alors que rien ne l'est.

### 1.6 Templates variadiques et *fold expressions*

On veut écrire :

```cpp
tx.query("INSERT INTO items (name, score) VALUES ($1, $2)", name, 42);
```

avec **n'importe quel nombre** d'arguments de **n'importe quels types**. C'est un
**template variadique** :

```cpp
template <class... Args>                      // Args : une liste de types (un « pack »)
Params params(const Args &...args)            // args : la liste des valeurs
{
    Params result;
    (result.add(args), ...);                  // fold expression
    return result;
}
```

- `class... Args` : zéro, un ou plusieurs types. Pour `params(name, 42)`, le compilateur
  fabrique `params<std::string, int>(const std::string &, const int &)` ;
- `(result.add(args), ...)` : une *fold expression* (C++17). Le compilateur la
  **déplie** en `(result.add(name), result.add(42))`. L'opérateur `,` garantit l'ordre :
  `$1` puis `$2`.

Ensuite, chaque `add` appelle `toText(value)`, et la **surcharge** choisit la bonne
conversion selon le type : `toText(int)`, `toText(bool)`, `toText(std::optional<T>)`…
Deux pièges, que `Convert.hpp` évite (et que tu reverras en questions) :

**Piège 1 : un littéral chaîne préfère `bool` à `std::string_view`.** `"alice"` est un
`const char[6]`. Pour appeler `toText(std::string_view)`, il faut une conversion
**définie par l'utilisateur** (un constructeur de `string_view`). Pour appeler
`toText(bool)`, une conversion **standard** (pointeur vers booléen) suffit. Et le C++
préfère toujours une conversion standard. Vérifié avec ces deux seules surcharges :

```cpp
std::optional<std::string> toText(bool v);
std::optional<std::string> toText(std::string_view v);
toText("alice");   // → "true", sans aucun warning avec -Wall -Wextra
```

D'où la surcharge `toText(const char *)`, qui est une correspondance **exacte**.

**Piège 2 : un template ne voit que ce qui est déclaré avant lui.** Si
`toText(const std::optional<T> &)` est **défini** avant que
`toText(const std::vector<T> &)` soit **déclaré**, alors
`toText(std::optional<std::vector<int>>)` ne compile pas :

```
error: no matching function for call to 'toText(const std::vector<int>&)'
```

Le nom `toText` à l'intérieur d'un template est cherché à l'endroit où le template est
**écrit** (plus, à l'instanciation, dans les namespaces des arguments, ici `std`, où il
n'y a pas de `toText`). D'où la règle de `Convert.hpp` : **toutes** les surcharges sont
déclarées en haut, les templates sont définis en bas.

### 1.7 Les codes SQLSTATE

Chaque erreur de PostgreSQL porte un code standard à 5 caractères. Les deux premiers
donnent la **classe** :

| Code | Signification | Exemple | `core::ErrorCode` |
|---|---|---|---|
| `23505` | violation d'unicité | email déjà pris | `Conflict` (409) |
| `23xxx` (autres) | autre contrainte : clé étrangère `23503`, `CHECK` `23514`, `NOT NULL` `23502` | score négatif | `Invalid` (400) |
| `22xxx` | donnée invalide : `22P02` (`'abc'` en entier), `22001` (texte trop long) | | `Invalid` |
| `40001`, `40P01` | échec de sérialisation, *deadlock* | deux transactions en conflit | `Unavailable` (503, à réessayer) |
| `08xxx`, `57P01`, `53300` | connexion perdue, serveur arrêté, trop de connexions | redémarrage de la base | `Unavailable` |
| le reste | syntaxe `42601`, table inconnue `42P01`… | `SELEC 1` | `Internal` (500) : c'est un bug |

Le message garde le code pour les logs :
`duplicate key value violates unique constraint "items_name_key" (SQLSTATE 23505)`. Le
nom de la contrainte y est : un repository qui a deux contraintes uniques (email et nom
d'utilisateur) pourra savoir laquelle a sauté.

Quand c'est **libpq** qui échoue (connexion coupée en pleine requête), il n'y a pas de
SQLSTATE : `PQresultErrorField` renvoie `nullptr`. On renvoie alors `Unavailable`.

### 1.8 Lire une ligne : par nom, `NULL`, et des exceptions pour les bugs

```cpp
const auto rows = tx->query("SELECT id, bio, created_at FROM profiles WHERE id = $1", id);
for (const pg::Row row : *rows)
{
    std::int64_t id = row.get<std::int64_t>("id");
    std::optional<std::string> bio = row.get<std::optional<std::string>>("bio");
    pg::Timestamp created = row.get<pg::Timestamp>("created_at");
}
```

- **Par nom** (`PQfnumber`), pas par index : ajouter une colonne au `SELECT` ne décale
  plus rien ;
- **`NULL` se lit seulement dans un `std::optional`**. `get<int>` sur un `NULL` est une
  erreur : le code n'avait pas prévu ce cas ;
- **`if constexpr`** (C++17) : dans `Row::get<T>`, une branche est choisie **à la
  compilation** selon que `T` est un `std::optional` ou non. L'autre branche n'est même
  pas compilée pour ce `T`. Le test « est-ce un `optional` ? » est un petit *trait*
  (`detail::IsOptional<T>`) : un template qui vaut `true` pour `std::optional<X>`,
  `false` sinon (spécialisation partielle).

**Colonne inconnue, `NULL` inattendu, texte illisible : exception `pg::RowError`.** C'est
la règle de la question 1 du guide 04 : une faute de frappe dans `"emial"` ne se
« gère » pas à l'exécution, elle se **corrige**. Un `Result` obligerait chaque appelant
à tester un cas qui n'arrive que si le code est faux. L'exception le signale tout de
suite, avec le nom de la colonne.

**`TIMESTAMPTZ`**. En format texte, PostgreSQL écrit l'heure dans le **fuseau de la
session**, au format de **`DateStyle`**. Avec un fuseau `Europe/Paris`, la même date
pourrait revenir en `2026-10-27 09:30:00+01`. `Connection::open` fixe donc, pour chaque
session, `TimeZone=UTC` et `DateStyle=ISO` (option `options` de libpq). La réponse a
toujours la forme `2026-10-27 08:30:00.123456+00`, que `fromText` sait lire (avec ou sans
fraction, avec ou sans décalage). Dans l'autre sens, `toText(pg::Timestamp)` envoie
`2026-10-27T08:30:00.123456Z` (ISO 8601).

Dans `pg::Timestamp`, le temps est un `std::chrono::sys_time<microseconds>` : la
précision exacte de PostgreSQL. `std::chrono::system_clock::now()` (en nanosecondes) est
accepté aussi, tronqué à la microseconde.

### 1.9 La décision du banc de test

Les tests de cpp-pg ont besoin d'un **vrai** PostgreSQL : on teste justement le dialogue
avec lui. Le plan demandait de décider comment ils trouvent leur base. Décision :

- **une base dédiée**, `postgres-test`, dans le compose de **dev** : jamais la base
  `matcha` (un `TRUNCATE` de test n'a rien à y faire), données en **`tmpfs`** (en
  mémoire, effacées à chaque arrêt), **aucun port publié** ;
- **des variables `PG_TEST_*`** (`HOST`, `PORT`, `NAME`, `USER`, `PASSWORD`) données au
  conteneur backend, lues avec `ConfigReader` et `pg::readSettings` : la même fonction
  que lira le vrai serveur avec le préfixe `DB_` au guide 06 ;
- **sans `PG_TEST_HOST`, ces tests sont sautés** (`GTEST_SKIP()`), pas en échec : la lib
  reste testable par quelqu'un qui n'a pas de PostgreSQL sous la main. Les tests
  « purs » (conversions, SQLSTATE) tournent toujours ;
- **`RESOURCE_LOCK`** : tous les tests de cpp-pg partagent la même table. La propriété
  CTest `RESOURCE_LOCK cpp-pg-database` interdit d'en lancer deux en même temps, même
  avec `ctest -j` ;
- **une table vidée avant chaque test** (`TRUNCATE items RESTART IDENTITY`). Le plan
  parlait de tests enfermés dans une transaction annulée à la fin. C'est la bonne
  technique pour tester les **repositories** (guide 06) : elle ne laisse aucune trace.
  Mais ici, on teste justement `commit()`, et deux transactions **en parallèle** : elles
  doivent pouvoir valider pour de vrai. On vide donc la table au début de chaque test.

### 1.10 Ce qui attendra le guide 06

`QueryBuilder` (SQL dynamique sûr), `MigrationRunner` (le déplacement de
`DatabaseInitializer`), la migration des repositories et la suppression de
`src/database/`. Et hors planning : requêtes préparées (`PQprepare`), format binaire,
`LISTEN/NOTIFY`, mode pipeline. Le README de cpp-pg les liste dans ses **limites**.

---

## 2. Étape A : partir propre

Depuis `~/matcha`, sur la branche `libs/cpp-core` (ce guide y a déjà été commité et
poussé) :

```sh
git switch libs/cpp-core
git status
```

✅ **Attendu** : `nothing to commit, working tree clean`.

```sh
git switch -c libs/cpp-pg
```

La nouvelle branche part de `libs/cpp-core` : cpp-pg a besoin de cpp-core.

---

## 3. Étape B : une base PostgreSQL pour les tests

### 3.1 Le service `postgres-test`

Dans `~/matcha/docker-compose.dev.yml`, deux ajouts. D'abord, dans le service `backend`,
**après** son bloc `volumes:` (même indentation que `volumes:`) :

```yaml
    # Base jetable des tests de cpp-pg (voir postgres-test ci-dessous).
    environment:
      PG_TEST_HOST: postgres-test
      PG_TEST_PORT: 5432
      PG_TEST_NAME: matcha_test
      PG_TEST_USER: matcha_test
      PG_TEST_PASSWORD: matcha_test
    depends_on:
      postgres-test:
        condition: service_healthy
```

Puis un nouveau service, **après** le service `backend` et **avant** la ligne
`volumes:` du bas du fichier (celle qui n'est pas indentée) :

```yaml
  # Base PostgreSQL des tests : données en mémoire (tmpfs), effacées à chaque
  # arrêt du conteneur. Aucun port publié : seul le backend la voit, son mot
  # de passe n'a donc rien de secret.
  postgres-test:
    image: postgres:17
    container_name: matcha_postgres_test
    environment:
      POSTGRES_DB: matcha_test
      POSTGRES_USER: matcha_test
      POSTGRES_PASSWORD: matcha_test
    tmpfs:
      - /var/lib/postgresql/data
    healthcheck:
      test:
        - CMD-SHELL
        - pg_isready -U matcha_test -d matcha_test
      interval: 2s
      timeout: 3s
      retries: 15
```

Ce qu'il faut comprendre :

- **Fusion des deux fichiers.** Le Makefile lance
  `docker compose -f docker-compose.yml -f docker-compose.dev.yml`. Pour un même service,
  les **listes de clés** (`environment`, `depends_on`) du second fichier sont
  **ajoutées** à celles du premier. Le backend garde donc `DB_HOST`, `REDIS_HOST`… et
  reçoit en plus les `PG_TEST_*`. Je l'ai vérifié avec `docker compose … config`.
- **Un mot de passe en clair, ici seulement.** L'audit (point 1) interdit les
  identifiants en clair pour la **vraie** base. Celle-ci ne contient que des données de
  test, effacées à chaque arrêt, et n'est joignable que depuis le réseau interne de
  Docker (pas de `ports:`). Un secret ne protégerait rien.
- **`tmpfs`** : le dossier de données de PostgreSQL est en mémoire. Les tests sont plus
  rapides, et chaque `make up` repart d'une base vide.
- **`depends_on … service_healthy`** : le backend ne démarre qu'une fois
  `pg_isready` content. Sans ça, un `make test` lancé trop tôt échouerait à la
  connexion.
- **Seulement en dev** : `make prod` n'utilise que `docker-compose.yml`. Pas de base de
  test en production.

### 3.2 Relancer l'environnement

Le compose a changé : il faut recréer les conteneurs. Dans le terminal où tourne
`make up`, arrête-le (`Ctrl+C`), puis :

```sh
make down
make up
```

Dans un second terminal, sur l'hôte :

```sh
docker ps --format '{{.Names}} {{.Status}}'
```

✅ **Attendu** (l'ordre et les durées varient) : une ligne de plus que d'habitude.

```
matcha_postgres_test Up 12 seconds (healthy)
```

Puis dans le conteneur (`make shell`) :

```sh
env | grep PG_TEST | sort
```

✅ **Attendu** :

```
PG_TEST_HOST=postgres-test
PG_TEST_NAME=matcha_test
PG_TEST_PASSWORD=matcha_test
PG_TEST_PORT=5432
PG_TEST_USER=matcha_test
```

`postgres-test` est un **nom d'hôte** : sur le réseau du compose, chaque service est
joignable par son nom. On vérifiera la connexion pour de vrai à l'étape F.

---

## 4. Étape C : générer cpp-pg et déclarer ses dépendances

Toutes les commandes de cette étape à l'étape I se tapent **dans le conteneur**
(`/app`), sauf mention contraire.

### 4.1 Générer

```sh
sh libs/new-lib.sh cpp-pg pg
```

✅ **Attendu** :

```
Créé : libs/cpp-pg
Reste à faire : ajouter add_subdirectory(cpp-pg) dans libs/CMakeLists.txt
```

### 4.2 La déclarer, après cpp-core

`backend/libs/CMakeLists.txt` :

```cmake
add_subdirectory(cpp-core)
add_subdirectory(cpp-pg)
add_subdirectory(cpp-auth)
```

Cette fois, l'ordre n'est plus une précaution : cpp-pg va tester
`if(NOT TARGET cpp-core::cpp-core)` (§4.4). Déclarée avant cpp-core, cette cible
n'existerait pas encore.

### 4.3 Activer ses tests en dev

`backend/CMakePresets.json`, preset `debug` (la virgule en fin de ligne précédente est
obligatoire) :

```json
				"CPP_CORE_BUILD_TESTS": "ON",
				"CPP_AUTH_BUILD_TESTS": "ON",
				"CPP_PG_BUILD_TESTS": "ON"
```

### 4.4 Les dépendances, dans `libs/cpp-pg/CMakeLists.txt`

**Premier ajout**, juste après `include(CMakePackageConfigHelpers)` :

```cmake
# DEPENDANCES #
# Dans le monorepo, cpp-core est déjà une cible (déclarée avant dans libs/) ;
# seule, la lib la cherche installée.
if(NOT TARGET cpp-core::cpp-core)
	find_package(cpp-core 0.1 CONFIG REQUIRED)
endif()
find_package(PostgreSQL REQUIRED)
```

- **Dans le monorepo**, `add_subdirectory(cpp-core)` a déjà créé la cible
  `cpp-core::cpp-core` (l'alias du gabarit) : on s'en sert directement, avec les sources
  de cpp-core. Pas d'installation à faire.
- **Seule** (étape H), cpp-pg est le projet principal : la cible n'existe pas, on cherche
  cpp-core **installée**, en version 0.1. C'est le même mécanisme que celui du
  package-consumer.
- `find_package(PostgreSQL REQUIRED)` : sans `CONFIG`, c'est le module `FindPostgreSQL`
  de CMake, comme dans `backend/CMakeLists.txt`. Il crée `PostgreSQL::PostgreSQL`.

**Deuxième ajout**, juste après le bloc `target_compile_definitions(cpp-pg …)` :

```cmake
target_link_libraries(cpp-pg
	PUBLIC cpp-core::cpp-core
	PRIVATE PostgreSQL::PostgreSQL
)
```

- **`PUBLIC cpp-core`** : nos en-têtes publics renvoient des `core::Result` et
  incluent `<core/Result.hpp>`. Qui inclut `<pg/Connection.hpp>` a besoin des en-têtes
  de cpp-core : la dépendance fait partie de l'**interface**.
- **`PRIVATE PostgreSQL`** : aucun en-tête public n'inclut `<libpq-fe.h>` (§1.3). Seuls
  nos `.cpp` en ont besoin.

**Troisième ajout**, tout en bas, remplace la ligne `gtest_discover_tests(cpp-pg_tests)` :

```cmake
    # RESOURCE_LOCK : ctest ne lance jamais deux tests de cpp-pg en même
    # temps, même avec -j : ils partagent la même base de test.
    gtest_discover_tests(cpp-pg_tests PROPERTIES RESOURCE_LOCK cpp-pg-database)
```

### 4.5 `cmake/cpp-pgConfig.cmake.in` et `vcpkg.json`

Sous le commentaire du gabarit, les deux dépendances (guide 02, Q3 : **toutes**, même
`PRIVATE`, car la lib est statique) :

```cmake
include(CMakeFindDependencyMacro)
# Une ligne find_dependency(...) par dépendance de la lib, PUBLIC comme PRIVATE :
# la lib est statique, le projet qui l'utilise devra aussi lier ses dépendances.
find_dependency(cpp-core 0.1)
find_dependency(PostgreSQL)
```

Et `libs/cpp-pg/vcpkg.json`, pour documenter ce qu'il faut installer :

```json
{
    "name": "cpp-pg",
    "version-string": "0.1.0",
    "dependencies": [
        "libpq",
        "gtest"
    ]
}
```

### 4.6 Vérifier

```sh
cmake --preset debug
grep CPP_PG_BUILD_TESTS build/debug/CMakeCache.txt
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R '^Version\.'
```

✅ **Attendu** : `CPP_PG_BUILD_TESTS:BOOL=ON`, puis
`100% tests passed, 0 tests failed out of 2`. **Deux** : le test de version de
cpp-core et celui de cpp-pg portent le même nom, `Version.FollowsSemanticVersioning`.

---

## 5. Étape D : `Settings` et `SqlState`

Deux petites briques **sans base de données**, pour commencer en douceur.

### 5.1 `include/pg/Settings.hpp`

```cpp
#pragma once
#include <core/ConfigReader.hpp>
#include <string>

namespace pg
{
	// Paramètres de connexion. Ils sont passés à libpq un par un
	// (PQconnectdbParams) : rien à échapper, même si le mot de passe contient
	// une espace ou une apostrophe.
	struct ConnectionSettings
	{
		std::string	host;
		std::string	port = "5432";
		std::string	database;
		std::string	user;
		std::string	password;
		std::string	applicationName = "cpp-pg";
		int			connectTimeoutSeconds = 5;
	};

	// Lit <prefix>HOST, <prefix>PORT, <prefix>NAME, <prefix>USER et
	// <prefix>PASSWORD (ou <prefix>PASSWORD_FILE). Les erreurs sont notées
	// dans config, comme pour toute lecture : à vérifier avec config.finish().
	ConnectionSettings	readSettings(core::ConfigReader &config, const std::string &prefix);
}
```

### 5.2 `src/Settings.cpp`

```cpp
#include <pg/Settings.hpp>

pg::ConnectionSettings	pg::readSettings(core::ConfigReader &config, const std::string &prefix)
{
	ConnectionSettings settings;
	settings.host = config.required(prefix + "HOST");
	settings.port = std::to_string(config.requiredInt(prefix + "PORT"));
	settings.database = config.required(prefix + "NAME");
	settings.user = config.required(prefix + "USER");
	settings.password = config.requiredSecret(prefix + "PASSWORD");
	return settings;
}
```

- `std::to_string(config.requiredInt(...))` : le port est **vérifié** comme entier au
  démarrage (`DB_PORT=54x2` est refusé avec les autres erreurs), puis remis en texte,
  car libpq veut du texte ;
- `requiredSecret` : `DB_PASSWORD`, ou le fichier désigné par `DB_PASSWORD_FILE`
  (le secret Docker du vrai backend) ;
- `applicationName` et `connectTimeoutSeconds` gardent leur valeur par défaut. Le nom
  apparaît dans la vue `pg_stat_activity` de PostgreSQL : pratique pour savoir qui est
  connecté.

### 5.3 `include/pg/SqlState.hpp` et `src/SqlState.cpp`

```cpp
#pragma once
#include <core/Error.hpp>
#include <string_view>

namespace pg
{
	// Traduit un code SQLSTATE de PostgreSQL (5 caractères, ex. "23505") en
	// catégorie d'erreur de cpp-core.
	core::ErrorCode	errorCodeFor(std::string_view sqlState) noexcept;
}
```

```cpp
#include <pg/SqlState.hpp>

core::ErrorCode	pg::errorCodeFor(std::string_view sqlState) noexcept
{
	if (sqlState == "23505")					// unique_violation : déjà pris
		return core::ErrorCode::Conflict;
	if (sqlState.starts_with("23")				// autres contraintes (FK, CHECK, NOT NULL)
		|| sqlState.starts_with("22"))			// donnée invalide (texte trop long, "abc" en entier…)
		return core::ErrorCode::Invalid;
	if (sqlState == "40001"						// serialization_failure : à réessayer
		|| sqlState == "40P01"					// deadlock_detected : à réessayer
		|| sqlState.starts_with("08")			// connexion perdue
		|| sqlState == "53300"					// too_many_connections
		|| sqlState == "57P01")					// serveur arrêté par l'administrateur
		return core::ErrorCode::Unavailable;
	return core::ErrorCode::Internal;			// syntaxe, table inconnue… : un bug
}
```

L'**ordre** des `if` compte : `23505` est testé **avant** la règle générale « tout ce
qui commence par `23` ». `starts_with` est une méthode de `std::string_view` depuis
C++20.

### 5.4 Les tests : `tests/SqlStateTests.cpp` et `tests/SettingsTests.cpp`

```cpp
#include <pg/SqlState.hpp>
#include <gtest/gtest.h>

TEST(SqlState, UniqueViolationIsConflict)
{
	EXPECT_EQ(pg::errorCodeFor("23505"), core::ErrorCode::Conflict);
}

TEST(SqlState, BadDataIsInvalid)
{
	EXPECT_EQ(pg::errorCodeFor("23503"), core::ErrorCode::Invalid);	// clé étrangère
	EXPECT_EQ(pg::errorCodeFor("23514"), core::ErrorCode::Invalid);	// CHECK
	EXPECT_EQ(pg::errorCodeFor("22P02"), core::ErrorCode::Invalid);	// 'abc' en entier
}

TEST(SqlState, RetryableIsUnavailable)
{
	EXPECT_EQ(pg::errorCodeFor("40001"), core::ErrorCode::Unavailable);
	EXPECT_EQ(pg::errorCodeFor("40P01"), core::ErrorCode::Unavailable);
	EXPECT_EQ(pg::errorCodeFor("08006"), core::ErrorCode::Unavailable);
}

TEST(SqlState, AnythingElseIsInternal)
{
	EXPECT_EQ(pg::errorCodeFor("42601"), core::ErrorCode::Internal);	// erreur de syntaxe
	EXPECT_EQ(pg::errorCodeFor("42P01"), core::ErrorCode::Internal);	// table inconnue
}
```

```cpp
#include <pg/Settings.hpp>
#include <gtest/gtest.h>

TEST(Settings, ReadsPrefixedVariables)
{
	core::MapEnvironment env;
	env.set("DB_HOST", "database");
	env.set("DB_PORT", "5432");
	env.set("DB_NAME", "matcha");
	env.set("DB_USER", "matcha");
	env.set("DB_PASSWORD", "s3cret");
	core::ConfigReader config(env);
	const pg::ConnectionSettings settings = pg::readSettings(config, "DB_");
	ASSERT_TRUE(config.finish());
	EXPECT_EQ(settings.host, "database");
	EXPECT_EQ(settings.port, "5432");
	EXPECT_EQ(settings.password, "s3cret");
}

TEST(Settings, ReportsMissingVariables)
{
	core::MapEnvironment env;
	env.set("DB_HOST", "database");
	core::ConfigReader config(env);
	pg::readSettings(config, "DB_");
	const auto result = config.finish();
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().message,
		"DB_PORT is required; DB_NAME is required; DB_USER is required; "
		"DB_PASSWORD or DB_PASSWORD_FILE is required");
}
```

Le second test vérifie que `readSettings` **ne s'arrête pas** à la première variable
manquante : c'est `ConfigReader` qui note tout (guide 04, §1.3).

### 5.5 Les déclarer et vérifier

Dans `libs/cpp-pg/CMakeLists.txt` :

```cmake
add_library(cpp-pg STATIC
	src/Version.cpp
	src/Settings.cpp
	src/SqlState.cpp
)
```

```cmake
    add_executable(cpp-pg_tests
        tests/VersionTests.cpp
        tests/SqlStateTests.cpp
        tests/SettingsTests.cpp
    )
```

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R '^(SqlState|Settings)\.'
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 6`.

> ⚠️ Comme au guide 04 (§7.3) : `No tests were found!!!` n'est **pas** une réussite.
> C'est qu'un fichier de tests manque dans `add_executable`.

---

## 6. Étape E : `Convert` et `Params`

Les conversions entre C++ et le format texte de PostgreSQL. Toujours sans base.

### 6.1 `include/pg/Convert.hpp`

```cpp
#pragma once
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pg
{
	// Un instant UTC, à la microseconde près (la précision de TIMESTAMPTZ).
	using Timestamp = std::chrono::sys_time<std::chrono::microseconds>;

	// Une valeur au format texte de PostgreSQL. std::nullopt veut dire NULL.
	using Text = std::optional<std::string>;

	// ---- C++ vers PostgreSQL : la valeur d'un paramètre $n ----
	// Toutes les surcharges sont déclarées avant d'être définies : les modèles
	// (optional, vector) s'appellent entre eux, dans n'importe quel ordre.
	Text	toText(bool value);
	Text	toText(double value);
	Text	toText(const char *value);
	Text	toText(std::string_view value);
	Text	toText(const std::string &value);
	Text	toText(Timestamp value);
	Text	toText(std::chrono::year_month_day value);
	Text	toText(std::nullopt_t);
	template <std::integral T>
	Text	toText(T value);
	template <class Duration>
	Text	toText(std::chrono::sys_time<Duration> value);
	template <class T>
	Text	toText(const std::optional<T> &value);
	template <class T>
	Text	toText(const std::vector<T> &values);

	// ---- PostgreSQL vers C++ : false si le texte ne se lit pas ----
	bool	fromText(std::string_view text, bool &out);
	bool	fromText(std::string_view text, double &out);
	bool	fromText(std::string_view text, std::string &out);
	bool	fromText(std::string_view text, Timestamp &out);
	bool	fromText(std::string_view text, std::chrono::year_month_day &out);
	template <std::integral T>
	bool	fromText(std::string_view text, T &out);

	// Échappe une valeur pour un tableau PostgreSQL : "a\"b" pour a"b.
	std::string	quoteArrayElement(std::string_view value);

	// ---- Définitions des modèles ----

	template <std::integral T>
	Text	toText(T value)
	{
		return std::to_string(value);
	}

	template <class Duration>
	Text	toText(std::chrono::sys_time<Duration> value)
	{
		return toText(std::chrono::floor<std::chrono::microseconds>(value));
	}

	template <class T>
	Text	toText(const std::optional<T> &value)
	{
		if (!value)
			return std::nullopt;
		return toText(*value);
	}

	// {1,2,3} ; {"a b","c\"d",NULL}
	template <class T>
	Text	toText(const std::vector<T> &values)
	{
		std::string out = "{";
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			if (i > 0)
				out += ',';
			const Text element = toText(values[i]);
			out += element ? quoteArrayElement(*element) : "NULL";
		}
		return out + "}";
	}

	template <std::integral T>
	bool	fromText(std::string_view text, T &out)
	{
		const char *end = text.data() + text.size();
		const auto [stop, error] = std::from_chars(text.data(), end, out);
		return error == std::errc{} && stop == end;
	}
}
```

Relis le §1.6 avec ce fichier sous les yeux :

- les **déclarations** d'abord, puis les **définitions** des templates (piège 2) ;
- `toText(const char *)` existe pour le piège 1 ;
- `template <std::integral T>` : un **concept** (C++20). Ce template n'accepte que les
  types entiers (`int`, `std::int64_t`, `short`…). Sans lui, `toText(T value)` accepterait
  **tous** les types, et volerait les appels destinés aux autres surcharges. `bool` est
  aussi un `std::integral`, mais la surcharge **non template** `toText(bool)` passe
  avant : à égalité, le C++ préfère une fonction normale à un template ;
- `toText(std::chrono::sys_time<Duration>)` : accepte un `time_point` à n'importe quelle
  précision, et le ramène à la microseconde avec `floor` ;
- les tableaux : chaque élément est mis entre guillemets, avec `\` devant `"` et `\`. Un
  élément `NULL` s'écrit `NULL` **sans** guillemets (avec, ce serait le texte « NULL »).

### 6.2 `src/Convert.cpp`

```cpp
#include <pg/Convert.hpp>
#include <charconv>
#include <cstdio>

namespace
{
	// Lit exactement `count` chiffres à partir de `pos`.
	bool	readDigits(std::string_view text, std::size_t pos, std::size_t count, int &out)
	{
		if (pos + count > text.size())
			return false;
		out = 0;
		for (std::size_t i = pos; i < pos + count; ++i)
		{
			if (text[i] < '0' || text[i] > '9')
				return false;
			out = out * 10 + (text[i] - '0');
		}
		return true;
	}

	// "2026-10-27" : seulement des années à 4 chiffres, pas d'avant J.-C.
	bool	readDate(std::string_view text, std::chrono::year_month_day &out)
	{
		int year = 0;
		int month = 0;
		int day = 0;
		if (text.size() < 10 || text[4] != '-' || text[7] != '-'
			|| !readDigits(text, 0, 4, year)
			|| !readDigits(text, 5, 2, month)
			|| !readDigits(text, 8, 2, day))
			return false;
		out = std::chrono::year{year} / std::chrono::month{static_cast<unsigned>(month)}
			/ std::chrono::day{static_cast<unsigned>(day)};
		return out.ok();
	}
}

pg::Text	pg::toText(bool value)
{
	return value ? "true" : "false";
}

pg::Text	pg::toText(double value)
{
	char buffer[64];
	const auto [end, error] = std::to_chars(buffer, buffer + sizeof buffer, value);
	return std::string(buffer, end);
}

pg::Text	pg::toText(const char *value)
{
	return std::string(value);
}

pg::Text	pg::toText(std::string_view value)
{
	return std::string(value);
}

pg::Text	pg::toText(const std::string &value)
{
	return value;
}

// ISO 8601 en UTC : 2026-10-27T08:30:00.123456Z
pg::Text	pg::toText(Timestamp value)
{
	const auto day = std::chrono::floor<std::chrono::days>(value);
	const std::chrono::year_month_day date{day};
	const std::chrono::hh_mm_ss time{value - day};
	char buffer[40];
	std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02uT%02d:%02d:%02d.%06dZ",
		static_cast<int>(date.year()),
		static_cast<unsigned>(date.month()),
		static_cast<unsigned>(date.day()),
		static_cast<int>(time.hours().count()),
		static_cast<int>(time.minutes().count()),
		static_cast<int>(time.seconds().count()),
		static_cast<int>(time.subseconds().count()));
	return std::string(buffer);
}

pg::Text	pg::toText(std::chrono::year_month_day value)
{
	char buffer[16];
	std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02u",
		static_cast<int>(value.year()),
		static_cast<unsigned>(value.month()),
		static_cast<unsigned>(value.day()));
	return std::string(buffer);
}

pg::Text	pg::toText(std::nullopt_t)
{
	return std::nullopt;
}

std::string	pg::quoteArrayElement(std::string_view value)
{
	std::string out = "\"";
	for (const char c : value)
	{
		if (c == '"' || c == '\\')
			out += '\\';
		out += c;
	}
	return out + "\"";
}

bool	pg::fromText(std::string_view text, bool &out)
{
	if (text == "t")
		out = true;
	else if (text == "f")
		out = false;
	else
		return false;
	return true;
}

bool	pg::fromText(std::string_view text, double &out)
{
	const char *end = text.data() + text.size();
	const auto [stop, error] = std::from_chars(text.data(), end, out);
	return error == std::errc{} && stop == end;
}

bool	pg::fromText(std::string_view text, std::string &out)
{
	out = std::string(text);
	return true;
}

bool	pg::fromText(std::string_view text, std::chrono::year_month_day &out)
{
	return text.size() == 10 && readDate(text, out);
}

// TIMESTAMPTZ : "2026-10-27 08:30:00.123456+00" (décalage +HH, +HH:MM ou +HH:MM:SS)
// TIMESTAMP   : "2026-10-27 08:30:00.123456"    (sans décalage : lu comme UTC)
bool	pg::fromText(std::string_view text, Timestamp &out)
{
	std::chrono::year_month_day date;
	int hour = 0;
	int minute = 0;
	int second = 0;
	if (!readDate(text, date) || text.size() < 19
		|| (text[10] != ' ' && text[10] != 'T') || text[13] != ':' || text[16] != ':'
		|| !readDigits(text, 11, 2, hour)
		|| !readDigits(text, 14, 2, minute)
		|| !readDigits(text, 17, 2, second)
		|| hour > 23 || minute > 59 || second > 59)
		return false;
	std::size_t pos = 19;

	int micros = 0;
	if (pos < text.size() && text[pos] == '.')
	{
		int digits = 0;
		for (++pos; pos < text.size() && text[pos] >= '0' && text[pos] <= '9'; ++pos, ++digits)
			if (digits < 6)
				micros = micros * 10 + (text[pos] - '0');
		if (digits == 0 || digits > 6)
			return false;
		for (; digits < 6; ++digits)
			micros *= 10;
	}

	int offset = 0;		// en secondes, à l'est de Greenwich
	if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
	{
		const int sign = text[pos] == '-' ? -1 : 1;
		int parts[3] = {0, 0, 0};
		if (!readDigits(text, pos + 1, 2, parts[0]))
			return false;
		pos += 3;
		for (int i = 1; i < 3 && pos < text.size() && text[pos] == ':'; ++i, pos += 3)
			if (!readDigits(text, pos + 1, 2, parts[i]))
				return false;
		offset = sign * (parts[0] * 3600 + parts[1] * 60 + parts[2]);
	}
	if (pos != text.size())
		return false;

	out = std::chrono::sys_days{date}
		+ std::chrono::hours{hour} + std::chrono::minutes{minute}
		+ std::chrono::seconds{second} + std::chrono::microseconds{micros}
		- std::chrono::seconds{offset};
	return true;
}
```

- `std::to_chars` pour `double` : la représentation **la plus courte** qui redonne
  exactement le même nombre (`2.5`, pas `2.500000`). `std::to_string` en donnerait 6
  décimales, et perdrait de la précision ;
- GCC 12 n'a pas encore `std::format` (arrivé avec GCC 13) : on formate les dates avec
  `snprintf`, en convertissant explicitement chaque champ (`%02u` attend un `unsigned`) ;
- `std::chrono::hh_mm_ss` découpe une durée en heures, minutes, secondes et fraction
  (`subseconds()`, ici en microsecondes) ;
- `fromText(…, bool &)` n'accepte que `t` et `f` : c'est ce que **renvoie**
  PostgreSQL. `toText(bool)` envoie `true`/`false`, que PostgreSQL **accepte** ;
- `readDigits` n'utilise pas `from_chars`, qui accepterait un signe `-` : dans une date,
  seuls des chiffres sont valides.

### 6.3 `include/pg/Params.hpp` et `src/Params.cpp`

```cpp
#pragma once
#include <pg/Convert.hpp>
#include <cstddef>
#include <vector>

namespace pg
{
	// Les valeurs de $1, $2… d'une requête, déjà converties en texte.
	class Params
	{
		public:
			template <class T>
			void	add(const T &value)
			{
				this->_values.push_back(toText(value));
			}

			std::size_t				size() const;
			const std::vector<Text>	&values() const;

		private:
			std::vector<Text>	_values;
	};

	// pg::params(42, "alice", std::optional<int>{}) : $1 = 42, $2 = alice, $3 = NULL
	template <class... Args>
	Params	params(const Args &...args)
	{
		Params result;
		(result.add(args), ...);	// fold expression : un add par argument, dans l'ordre
		return result;
	}
}
```

```cpp
#include <pg/Params.hpp>

std::size_t	pg::Params::size() const
{
	return this->_values.size();
}

const std::vector<pg::Text>	&pg::Params::values() const
{
	return this->_values;
}
```

`add` est un template défini **dans** l'en-tête : le compilateur doit voir son corps
pour l'instancier avec chaque type. C'est pour ça que tout `Convert.hpp` est inclus.

### 6.4 Les tests : `tests/ConvertTests.cpp`

```cpp
#include <pg/Convert.hpp>
#include <gtest/gtest.h>

using namespace std::chrono;

TEST(Convert, ScalarsToText)
{
	EXPECT_EQ(pg::toText(true), "true");
	EXPECT_EQ(pg::toText(42), "42");
	EXPECT_EQ(pg::toText(std::int64_t{-9000000000}), "-9000000000");
	EXPECT_EQ(pg::toText(2.5), "2.5");
}

TEST(Convert, StringLiteralStaysAString)
{
	// Sans la surcharge const char *, "alice" deviendrait… true.
	EXPECT_EQ(pg::toText("alice"), "alice");
}

TEST(Convert, EmptyOptionalIsNull)
{
	EXPECT_EQ(pg::toText(std::optional<int>{}), std::nullopt);
	EXPECT_EQ(pg::toText(std::optional<int>{7}), "7");
}

TEST(Convert, VectorToArrayLiteral)
{
	EXPECT_EQ(pg::toText(std::vector<int>{1, 2, 3}), R"({"1","2","3"})");
	const std::vector<std::optional<std::string>> tags{"a b", R"(c"d)", std::nullopt};
	EXPECT_EQ(pg::toText(tags), R"({"a b","c\"d",NULL})");
}

TEST(Convert, TimestampToIso8601)
{
	const pg::Timestamp t = sys_days{2026y / October / 27} + 8h + 30min + 123456us;
	EXPECT_EQ(pg::toText(t), "2026-10-27T08:30:00.123456Z");
}

TEST(Convert, ReadsTimestampWithOffset)
{
	pg::Timestamp t;
	ASSERT_TRUE(pg::fromText("2026-10-27 10:30:00.5+02", t));
	EXPECT_EQ(t, sys_days{2026y / October / 27} + 8h + 30min + 500ms);
	ASSERT_TRUE(pg::fromText("2026-10-27 08:30:00", t));		// TIMESTAMP, sans décalage
	EXPECT_EQ(t, sys_days{2026y / October / 27} + 8h + 30min);
	ASSERT_TRUE(pg::fromText("2026-10-27 13:00:00+04:30", t));
	EXPECT_EQ(t, sys_days{2026y / October / 27} + 8h + 30min);
}

TEST(Convert, RejectsMalformedText)
{
	pg::Timestamp t;
	EXPECT_FALSE(pg::fromText("2026-10-27", t));
	EXPECT_FALSE(pg::fromText("2026-13-01 00:00:00", t));
	EXPECT_FALSE(pg::fromText("infinity", t));
	int n = 0;
	EXPECT_FALSE(pg::fromText("12abc", n));
	bool b = false;
	EXPECT_FALSE(pg::fromText("true", b));		// PostgreSQL répond "t" ou "f"
}
```

- `2026y / October / 27`, `8h`, `30min`, `123456us` : les littéraux de `<chrono>`
  (C++20), grâce au `using namespace std::chrono;` du fichier de test. Bien plus lisible
  qu'un nombre de microsecondes depuis 1970 ;
- `R"(...)"` : une chaîne **brute** (*raw string*), où `"` et `\` n'ont pas besoin
  d'être échappés. Indispensable pour lire les tableaux PostgreSQL ;
- `"2026-10-27 13:00:00+04:30"` vaut bien 08:30 UTC : 13:00 moins 4 h 30.

### 6.5 Les déclarer et vérifier

```cmake
add_library(cpp-pg STATIC
	src/Version.cpp
	src/Settings.cpp
	src/SqlState.cpp
	src/Convert.cpp
	src/Params.cpp
)
```

Et `tests/ConvertTests.cpp` à la fin de la liste de `add_executable(cpp-pg_tests …)`.

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R '^Convert\.'
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 7`.

---

## 7. Étape F : `Rows` et `Connection`

On parle enfin à PostgreSQL.

### 7.1 `include/pg/Rows.hpp`

```cpp
#pragma once
#include <pg/Convert.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

// Le type de libpq (PGresult), déclaré sans inclure libpq-fe.h.
struct pg_result;

namespace pg
{
	// Erreur de programmation en lisant une ligne : colonne inconnue, NULL lu
	// dans un type non optionnel, texte illisible dans le type demandé.
	// C'est un bug à corriger, pas une erreur attendue : exception.
	class RowError : public std::logic_error
	{
		public:
			using std::logic_error::logic_error;
	};

	namespace detail
	{
		template <class T>
		struct IsOptional : std::false_type {};
		template <class T>
		struct IsOptional<std::optional<T>> : std::true_type {};
	}

	// Une ligne d'un résultat. Garde le résultat en vie (shared_ptr) : une Row
	// reste valide même après la destruction de ses Rows.
	class Row
	{
		public:
			// row.get<std::int64_t>("id"), row.get<std::optional<std::string>>("bio")
			template <class T>
			T	get(std::string_view column) const;

		private:
			friend class Rows;
			Row(std::shared_ptr<pg_result> result, int index);

			// nullptr si la valeur est NULL. RowError si la colonne n'existe pas.
			const char	*text(std::string_view column) const;

			std::shared_ptr<pg_result>	_result;
			int							_index;
	};

	// Le résultat d'une requête : des lignes (SELECT, RETURNING) ou rien.
	class Rows
	{
		public:
			class Iterator
			{
				public:
					Row			operator*() const;
					Iterator	&operator++();
					bool		operator==(const Iterator &other) const = default;

				private:
					friend class Rows;
					Iterator(const Rows *rows, std::size_t index);

					const Rows	*_rows;
					std::size_t	_index;
			};

			std::size_t	size() const;
			bool		empty() const;
			Row			operator[](std::size_t index) const;	// RowError si hors limites
			Iterator	begin() const;
			Iterator	end() const;

			// Lignes touchées par INSERT, UPDATE ou DELETE.
			std::int64_t		affected() const;
			// La réponse du serveur : "INSERT 0 1", "COMMIT", "ROLLBACK"…
			std::string_view	command() const;

		private:
			friend class Connection;
			explicit Rows(pg_result *result);	// en prend possession

			std::shared_ptr<pg_result>	_result;
	};

	template <class T>
	T	Row::get(std::string_view column) const
	{
		const char *value = this->text(column);
		if constexpr (detail::IsOptional<T>::value)
		{
			if (value == nullptr)
				return std::nullopt;
			typename T::value_type out{};
			if (!fromText(value, out))
				throw RowError("column '" + std::string(column) + "': cannot read '" + value + "'");
			return out;
		}
		else
		{
			if (value == nullptr)
				throw RowError("column '" + std::string(column) + "' is NULL");
			T out{};
			if (!fromText(value, out))
				throw RowError("column '" + std::string(column) + "': cannot read '" + value + "'");
			return out;
		}
	}
}
```

- `struct pg_result;` : la déclaration anticipée du §1.3 ;
- `RowError` hérite de `std::logic_error` : la famille des exceptions qui signalent une
  **erreur de programmation** (`std::out_of_range`, `std::invalid_argument`…).
  `using std::logic_error::logic_error;` **hérite** de ses constructeurs : on peut écrire
  `RowError("message")` sans rien redéfinir ;
- `Row` garde un `std::shared_ptr<pg_result>` : chaque `Row` est un **propriétaire** du
  résultat. `const pg::Row row = (*rows)[0];` reste valide même si `rows` disparaît. Le
  prix : un compteur incrémenté à chaque `Row`, négligeable devant un aller-retour
  réseau ;
- `Iterator` : juste ce qu'il faut pour `for (const pg::Row row : *rows)`. Une boucle
  *range-for* appelle `begin()`, `end()`, `*it`, `++it` et `it != end`. Depuis C++20,
  `operator==` « par défaut » (`= default`) compare tous les membres, et le compilateur
  en déduit `!=` ;
- `get<T>` est défini dans l'en-tête (template), après la classe.

### 7.2 `src/Rows.cpp`

```cpp
#include <pg/Rows.hpp>
#include <libpq-fe.h>
#include <charconv>
#include <cstring>

pg::Row::Row(std::shared_ptr<pg_result> result, int index)
	: _result(std::move(result)), _index(index)
{
}

const char	*pg::Row::text(std::string_view column) const
{
	const std::string name(column);		// PQfnumber veut une chaîne terminée par '\0'
	const int number = PQfnumber(this->_result.get(), name.c_str());
	if (number < 0)
		throw RowError("unknown column '" + name + "'");
	if (PQgetisnull(this->_result.get(), this->_index, number))
		return nullptr;
	return PQgetvalue(this->_result.get(), this->_index, number);
}

pg::Rows::Rows(pg_result *result)
	: _result(result, PQclear)
{
}

std::size_t	pg::Rows::size() const
{
	return static_cast<std::size_t>(PQntuples(this->_result.get()));
}

bool	pg::Rows::empty() const
{
	return this->size() == 0;
}

pg::Row	pg::Rows::operator[](std::size_t index) const
{
	if (index >= this->size())
		throw RowError("row " + std::to_string(index) + " out of range ("
			+ std::to_string(this->size()) + " rows)");
	return Row(this->_result, static_cast<int>(index));
}

pg::Rows::Iterator	pg::Rows::begin() const
{
	return Iterator(this, 0);
}

pg::Rows::Iterator	pg::Rows::end() const
{
	return Iterator(this, this->size());
}

std::int64_t	pg::Rows::affected() const
{
	const char *text = PQcmdTuples(this->_result.get());	// "" si sans objet
	std::int64_t count = 0;
	std::from_chars(text, text + std::strlen(text), count);
	return count;
}

std::string_view	pg::Rows::command() const
{
	return PQcmdStatus(this->_result.get());
}

pg::Rows::Iterator::Iterator(const Rows *rows, std::size_t index)
	: _rows(rows), _index(index)
{
}

pg::Row	pg::Rows::Iterator::operator*() const
{
	return (*this->_rows)[this->_index];
}

pg::Rows::Iterator	&pg::Rows::Iterator::operator++()
{
	++this->_index;
	return *this;
}
```

- `_result(result, PQclear)` : le deleter du `shared_ptr` est passé au constructeur. Pas
  besoin de structure à part comme pour `unique_ptr` : un `shared_ptr` mémorise son
  deleter à l'exécution ;
- `PQfnumber` renvoie `-1` pour une colonne inconnue ;
- `PQcmdTuples` renvoie le nombre de lignes touchées **en texte** (`"2"`), ou `""` pour
  un `SELECT` : `from_chars` laisse alors `count` à 0.

### 7.3 `include/pg/Connection.hpp`

```cpp
#pragma once
#include <core/Result.hpp>
#include <pg/Params.hpp>
#include <pg/Rows.hpp>
#include <pg/Settings.hpp>
#include <memory>
#include <string_view>

// Le type de libpq (PGconn), déclaré sans inclure libpq-fe.h.
struct pg_conn;

namespace pg
{
	// Une connexion à PostgreSQL. Elle possède son PGconn : non copiable,
	// déplaçable, fermée (PQfinish) à sa destruction.
	class Connection
	{
		public:
			static core::Result<Connection>	open(const ConnectionSettings &settings);

			// Une seule instruction SQL. Les valeurs passent en $1, $2…,
			// jamais collées dans le texte de la requête.
			core::Result<Rows>	execute(std::string_view sql, const Params &params = {});

			// connection.query("SELECT ... WHERE id = $1", id)
			template <class... Args>
			core::Result<Rows>	query(std::string_view sql, const Args &...args)
			{
				return this->execute(sql, pg::params(args...));
			}

			bool	isHealthy() const;		// la connexion au serveur est-elle ouverte ?
			bool	inTransaction() const;	// un BEGIN sans COMMIT ni ROLLBACK ?
			bool	reset();				// se reconnecter (PQreset) ; true si ça a marché

		private:
			struct Close
			{
				void	operator()(pg_conn *connection) const noexcept;
			};

			explicit Connection(pg_conn *connection);

			std::unique_ptr<pg_conn, Close>	_connection;
	};
}
```

Aucun destructeur, aucun constructeur de copie ou de déplacement : la règle du zéro
(§1.3). Le constructeur à partir d'un `pg_conn *` est **privé** : la seule façon
d'obtenir une `Connection` est `open`, qui vérifie qu'elle marche.

### 7.4 `src/Connection.cpp`

```cpp
#include <pg/Connection.hpp>
#include <pg/SqlState.hpp>
#include <libpq-fe.h>
#include <string>
#include <vector>

namespace
{
	// Les messages de libpq finissent par un retour à la ligne.
	std::string	trimmed(const char *message)
	{
		std::string text = message ? message : "";
		while (!text.empty() && (text.back() == '\n' || text.back() == ' '))
			text.pop_back();
		return text;
	}
}

void	pg::Connection::Close::operator()(pg_conn *connection) const noexcept
{
	PQfinish(connection);
}

pg::Connection::Connection(pg_conn *connection)
	: _connection(connection)
{
}

core::Result<pg::Connection>	pg::Connection::open(const ConnectionSettings &settings)
{
	const std::string timeout = std::to_string(settings.connectTimeoutSeconds);
	const char *const keys[] = {
		"host", "port", "dbname", "user", "password",
		"application_name", "connect_timeout", "client_encoding", "options",
		nullptr
	};
	const char *const values[] = {
		settings.host.c_str(), settings.port.c_str(), settings.database.c_str(),
		settings.user.c_str(), settings.password.c_str(),
		settings.applicationName.c_str(), timeout.c_str(), "UTF8",
		// Chaque session parle en UTC et en ISO : Row::get<Timestamp> sait lire la
		// réponse. Et pas de NOTICE, que libpq écrirait sur stderr.
		"-c TimeZone=UTC -c DateStyle=ISO -c client_min_messages=warning",
		nullptr
	};
	// 0 : dbname est un nom de base, jamais une chaîne de connexion à interpréter.
	Connection connection(PQconnectdbParams(keys, values, 0));
	if (connection.isHealthy())
		return connection;
	return core::fail(core::ErrorCode::Unavailable,
		"cannot connect to PostgreSQL at " + settings.host + ":" + settings.port
		+ ": " + trimmed(PQerrorMessage(connection._connection.get())));
}

core::Result<pg::Rows>	pg::Connection::execute(std::string_view sql, const Params &params)
{
	const std::string text(sql);		// libpq veut une chaîne terminée par '\0'
	std::vector<const char *> values;
	values.reserve(params.size());
	for (const Text &value : params.values())
		values.push_back(value ? value->c_str() : nullptr);		// nullptr = NULL

	pg_result *raw = PQexecParams(this->_connection.get(), text.c_str(),
		static_cast<int>(values.size()),
		nullptr,		// types des paramètres : déduits par le serveur
		values.data(),
		nullptr,		// longueurs : inutiles en format texte
		nullptr,		// formats des paramètres : tous en texte
		0);				// format du résultat : texte
	if (raw == nullptr)
		return core::fail(core::ErrorCode::Unavailable,
			trimmed(PQerrorMessage(this->_connection.get())));
	Rows rows(raw);		// possède le résultat dès maintenant, même en cas d'erreur

	const ExecStatusType status = PQresultStatus(raw);
	if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK)
		return rows;
	const char *state = PQresultErrorField(raw, PG_DIAG_SQLSTATE);
	if (state == nullptr)	// erreur produite par libpq elle-même : connexion perdue
		return core::fail(core::ErrorCode::Unavailable, trimmed(PQresultErrorMessage(raw)));
	return core::fail(errorCodeFor(state),
		trimmed(PQresultErrorField(raw, PG_DIAG_MESSAGE_PRIMARY))
		+ " (SQLSTATE " + state + ")");
}

bool	pg::Connection::isHealthy() const
{
	return PQstatus(this->_connection.get()) == CONNECTION_OK;
}

bool	pg::Connection::inTransaction() const
{
	const PGTransactionStatusType status = PQtransactionStatus(this->_connection.get());
	return status == PQTRANS_INTRANS || status == PQTRANS_INERROR;
}

bool	pg::Connection::reset()
{
	PQreset(this->_connection.get());
	return this->isHealthy();
}
```

Les points importants, de haut en bas :

- **`PQconnectdbParams`** prend deux tableaux, clés et valeurs, terminés par `nullptr`.
  L'ancien code construisait une chaîne `"host=… password=…"` : un mot de passe
  contenant une espace l'aurait cassée. Ici, chaque valeur voyage seule ;
- **le dernier argument `0`** : sinon, libpq interpréterait `dbname` comme une chaîne de
  connexion complète si elle contient un `=`. Une valeur de config ne doit jamais
  changer le **sens** des autres ;
- **`options`** envoie des réglages de session : UTC et ISO (§1.8), et
  `client_min_messages=warning`. Sans lui, un `CREATE TABLE IF NOT EXISTS` sur une
  table existante enverrait un `NOTICE`, que libpq **écrit sur stderr** : une lib ne
  doit rien écrire dans le dos de l'application ;
- **`Connection connection(PQconnectdbParams(…))`** : le résultat est confié à un
  propriétaire **immédiatement**. Même en échec, `PQconnectdbParams` alloue un objet
  qu'il faut libérer avec `PQfinish` : si on renvoie l'erreur, `connection` est détruite
  et le libère. Pas de fuite possible ;
- **`Rows rows(raw);`** : pareil pour le résultat d'une requête, **avant** de regarder
  s'il est en erreur. Les trois `return` qui suivent n'ont pas à penser à `PQclear` ;
- **`return rows;`** : la fonction renvoie un `core::Result<Rows>`, et `rows` est un
  `Rows`. Le C++ construit le `Result` en **déplaçant** `rows` (pas de copie : une
  variable locale renvoyée est traitée comme un objet qu'on peut vider) ;
- **`PQresultErrorField(raw, PG_DIAG_SQLSTATE) == nullptr`** : l'erreur vient de libpq,
  pas du serveur (§1.7).

### 7.5 La base commune des tests : `tests/support/PgTest.hpp` et `.cpp`

Crée le dossier `tests/support/`. Cette classe sera la base (*fixture*) de tous les
tests qui ont besoin de PostgreSQL :

```cpp
#pragma once
#include <pg/Connection.hpp>
#include <gtest/gtest.h>
#include <optional>

// Base des tests qui parlent à un vrai PostgreSQL (variables PG_TEST_*).
// Sans PG_TEST_HOST, ces tests sont sautés : la lib reste testable sans base.
// Chaque test commence avec une table `items` vide.
class PgTest : public testing::Test
{
	protected:
		void	SetUp() override;

		pg::ConnectionSettings			settings;
		std::optional<pg::Connection>	connection;
};
```

```cpp
#include "PgTest.hpp"
#include <core/ConfigReader.hpp>

void	PgTest::SetUp()
{
	const core::SystemEnvironment environment;
	if (!environment.get("PG_TEST_HOST"))
		GTEST_SKIP() << "PG_TEST_HOST is not set: no test database";
	core::ConfigReader config(environment);
	this->settings = pg::readSettings(config, "PG_TEST_");
	const auto checked = config.finish();
	ASSERT_TRUE(checked) << checked.error().message;

	auto opened = pg::Connection::open(this->settings);
	ASSERT_TRUE(opened) << opened.error().message;
	this->connection = std::move(*opened);
	ASSERT_TRUE(this->connection->query(
		"CREATE TABLE IF NOT EXISTS items ("
		"  id         BIGSERIAL PRIMARY KEY,"
		"  name       TEXT NOT NULL UNIQUE,"
		"  score      INTEGER CHECK (score >= 0),"
		"  created_at TIMESTAMPTZ NOT NULL DEFAULT now())"));
	ASSERT_TRUE(this->connection->query("TRUNCATE items RESTART IDENTITY"));
}
```

- **`GTEST_SKIP()`** dans `SetUp` : le test est marqué « sauté » et son corps ne
  s'exécute pas (§1.9) ;
- **`ASSERT_…` dans `SetUp`** : si la config est fausse ou la connexion impossible,
  `SetUp` s'arrête, GoogleTest marque le test en **échec** et **ne lance pas** son
  corps. Une config présente mais fausse doit se voir, contrairement à une config
  absente ;
- **`std::optional<pg::Connection>`** : `Connection` n'a pas de constructeur par défaut
  (on ne l'obtient que par `open`). L'`optional` permet de déclarer le membre vide, puis
  de le remplir dans `SetUp`.

### 7.6 Les tests : `tests/QueryTests.cpp`

```cpp
#include "support/PgTest.hpp"

using namespace std::chrono;

TEST(Connection, ReportsUnreachableServer)
{
	pg::ConnectionSettings settings;
	settings.host = "127.0.0.1";
	settings.port = "1";		// personne n'écoute ici
	const auto connection = pg::Connection::open(settings);
	ASSERT_FALSE(connection);
	EXPECT_EQ(connection.error().code, core::ErrorCode::Unavailable);
}

class Query : public PgTest {};

TEST_F(Query, ReadsTypedColumnsByName)
{
	const auto rows = this->connection->query(
		"SELECT $1::bigint AS id, $2::text AS name, $3::boolean AS active", 42, "alice", true);
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 1u);
	const pg::Row row = (*rows)[0];
	EXPECT_EQ(row.get<std::int64_t>("id"), 42);
	EXPECT_EQ(row.get<std::string>("name"), "alice");
	EXPECT_TRUE(row.get<bool>("active"));
}

TEST_F(Query, NullNeedsAnOptional)
{
	const auto rows = this->connection->query("SELECT $1::integer AS score", std::optional<int>{});
	ASSERT_TRUE(rows) << rows.error().message;
	EXPECT_EQ((*rows)[0].get<std::optional<int>>("score"), std::nullopt);
	EXPECT_THROW((*rows)[0].get<int>("score"), pg::RowError);
}

TEST_F(Query, UnknownColumnIsABug)
{
	const auto rows = this->connection->query("SELECT 1 AS id");
	ASSERT_TRUE(rows) << rows.error().message;
	EXPECT_THROW((*rows)[0].get<int>("idd"), pg::RowError);
}

TEST_F(Query, ReadsTimestamptz)
{
	const auto rows = this->connection->query(
		"SELECT TIMESTAMPTZ '2026-10-27 10:30:00.123456+02' AS at, $1::timestamptz AS echo",
		pg::Timestamp{sys_days{2026y / October / 27} + 8h});
	ASSERT_TRUE(rows) << rows.error().message;
	EXPECT_EQ((*rows)[0].get<pg::Timestamp>("at"),
		sys_days{2026y / October / 27} + 8h + 30min + 123456us);
	EXPECT_EQ((*rows)[0].get<pg::Timestamp>("echo"), sys_days{2026y / October / 27} + 8h);
}

TEST_F(Query, ParametersAreNeverSql)
{
	const std::string evil = "x'); DROP TABLE items; --";
	ASSERT_TRUE(this->connection->query("INSERT INTO items (name) VALUES ($1)", evil));
	const auto rows = this->connection->query("SELECT name FROM items");
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 1u);
	EXPECT_EQ((*rows)[0].get<std::string>("name"), evil);
}

TEST_F(Query, ArrayParameter)
{
	ASSERT_TRUE(this->connection->query(
		"INSERT INTO items (name) SELECT unnest($1::text[])",
		std::vector<std::string>{"a", "b \"quoted\"", "c"}));
	const auto rows = this->connection->query(
		"SELECT name FROM items WHERE name = ANY($1) ORDER BY id",
		std::vector<std::string>{"a", "b \"quoted\""});
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 2u);
	EXPECT_EQ((*rows)[1].get<std::string>("name"), "b \"quoted\"");
}

TEST_F(Query, MapsSqlErrors)
{
	ASSERT_TRUE(this->connection->query("INSERT INTO items (name) VALUES ($1)", "alice"));
	const auto duplicate = this->connection->query("INSERT INTO items (name) VALUES ($1)", "alice");
	ASSERT_FALSE(duplicate);
	EXPECT_EQ(duplicate.error().code, core::ErrorCode::Conflict);
	EXPECT_EQ(duplicate.error().message,
		"duplicate key value violates unique constraint \"items_name_key\" (SQLSTATE 23505)");

	const auto negative = this->connection->query(
		"INSERT INTO items (name, score) VALUES ($1, $2)", "bob", -1);
	ASSERT_FALSE(negative);
	EXPECT_EQ(negative.error().code, core::ErrorCode::Invalid);

	const auto typo = this->connection->query("SELEC 1");
	ASSERT_FALSE(typo);
	EXPECT_EQ(typo.error().code, core::ErrorCode::Internal);
}

TEST_F(Query, CountsAffectedRows)
{
	ASSERT_TRUE(this->connection->query("INSERT INTO items (name) VALUES ('a'), ('b'), ('c')"));
	const auto updated = this->connection->query("UPDATE items SET score = 1 WHERE name <> $1", "a");
	ASSERT_TRUE(updated) << updated.error().message;
	EXPECT_EQ(updated->affected(), 2);
	EXPECT_TRUE(updated->empty());
}
```

- `class Query : public PgTest {};` puis `TEST_F(Query, …)` : chaque test reçoit un
  `PgTest` neuf, avec `SetUp` appelé avant. Le nom de la classe devient le préfixe du
  test (`Query.ReadsTimestamptz`) ;
- `Connection.ReportsUnreachableServer` est un `TEST` simple, sans fixture : il n'a pas
  besoin de base (le port 1 ne répond jamais) et tourne toujours ;
- `$1::bigint`, `$2::text` : le *cast* dit à PostgreSQL le type du paramètre. Sans lui,
  dans un `SELECT $1`, il ne pourrait pas le deviner ;
- `ParametersAreNeverSql` : la valeur ressemble à une injection, et se retrouve…
  dans la colonne, telle quelle ;
- `ANY($1)` avec un `std::vector` : PostgreSQL déduit `text[]` de la comparaison avec
  `name`. Un seul paramètre pour une liste de longueur quelconque, au lieu de construire
  `IN ('a', 'b', …)` à la main.

### 7.7 Les déclarer et vérifier

```cmake
add_library(cpp-pg STATIC
	src/Version.cpp
	src/Settings.cpp
	src/SqlState.cpp
	src/Convert.cpp
	src/Params.cpp
	src/Rows.cpp
	src/Connection.cpp
)
```

```cmake
    add_executable(cpp-pg_tests
        tests/VersionTests.cpp
        tests/SqlStateTests.cpp
        tests/SettingsTests.cpp
        tests/ConvertTests.cpp
        tests/support/PgTest.cpp
        tests/QueryTests.cpp
    )
```

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R '^(Connection|Query)\.'
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 9`. La base de test répond :
l'étape B est validée pour de vrai.

**Et sans base ?** Enlève `PG_TEST_HOST` de l'environnement, pour **cette commande
seulement** (`env -u`) :

```sh
env -u PG_TEST_HOST ctest --preset debug -R '^Query\.'
```

✅ **Attendu** : les 8 tests de `Query` sont **sautés**, pas en échec.

```
100% tests passed, 0 tests failed out of 8
...
The following tests did not run:
	 <n> - Query.ReadsTypedColumnsByName (Skipped)
	 <n> - Query.NullNeedsAnOptional (Skipped)
	 ...
```

---

## 8. Étape G : `ConnectionPool` et `Transaction`

### 8.1 `include/pg/Pool.hpp`

Les trois classes sont dans le même en-tête, parce qu'elles se tiennent :
`ConnectionPool::begin` renvoie une `Transaction`, qui contient une `PooledConnection`,
qui pointe vers son `ConnectionPool`. L'ordre dans le fichier suit ce qu'il faut
**connaître en entier** : `PooledConnection` (il lui suffit de **savoir** que
`ConnectionPool` existe, puisqu'il n'en garde qu'un pointeur), puis `Transaction`
(contient une `PooledConnection` complète), puis `ConnectionPool`.

```cpp
#pragma once
#include <core/Logger.hpp>
#include <core/Result.hpp>
#include <pg/Connection.hpp>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace pg
{
	class ConnectionPool;

	// Une connexion empruntée au pool, rendue automatiquement à sa destruction.
	class PooledConnection
	{
		public:
			PooledConnection(PooledConnection &&other) noexcept;
			PooledConnection	&operator=(PooledConnection &&other) noexcept;
			PooledConnection(const PooledConnection &) = delete;
			PooledConnection	&operator=(const PooledConnection &) = delete;
			~PooledConnection();

			Connection	&operator*();
			Connection	*operator->();

		private:
			friend class ConnectionPool;
			PooledConnection(ConnectionPool &pool, Connection connection);
			void	giveBack() noexcept;

			ConnectionPool	*_pool;			// nullptr : rien à rendre (objet déplacé)
			Connection		_connection;
	};

	enum class Isolation
	{
		ReadCommitted,
		RepeatableRead,
		Serializable
	};

	// BEGIN à la création, ROLLBACK à la destruction si commit() n'a pas été
	// appelé. Toutes les requêtes passent par la même connexion, du BEGIN au
	// COMMIT : personne d'autre ne peut s'y glisser.
	class Transaction
	{
		public:
			Transaction(Transaction &&other) noexcept;
			Transaction	&operator=(Transaction &&) = delete;
			~Transaction();

			core::Result<Rows>	execute(std::string_view sql, const Params &params = {});

			template <class... Args>
			core::Result<Rows>	query(std::string_view sql, const Args &...args)
			{
				return this->execute(sql, pg::params(args...));
			}

			core::Result<void>	commit();
			core::Result<void>	rollback();

		private:
			friend class ConnectionPool;
			explicit Transaction(PooledConnection connection);
			void	requireOpen() const;

			PooledConnection	_connection;
			bool				_open;
	};

	struct PoolOptions
	{
		std::size_t					size = 4;
		std::chrono::milliseconds	acquireTimeout{2000};
		int							connectAttempts = 12;		// pour la 1re connexion
		std::chrono::milliseconds	retryDelay{5000};
	};

	// N connexions ouvertes au démarrage, prêtées une par une. Thread-safe.
	// Doit vivre plus longtemps que toutes les connexions et transactions prêtées.
	class ConnectionPool
	{
		public:
			static core::Result<std::unique_ptr<ConnectionPool>>	open(
				const ConnectionSettings &settings,
				const PoolOptions &options,
				core::ILogger &logger);

			ConnectionPool(const ConnectionPool &) = delete;
			ConnectionPool	&operator=(const ConnectionPool &) = delete;
			~ConnectionPool();

			core::Result<PooledConnection>	acquire();
			core::Result<PooledConnection>	acquire(std::chrono::milliseconds timeout);
			core::Result<Transaction>		begin(Isolation isolation = Isolation::ReadCommitted);

			// Une requête seule, hors transaction : emprunte une connexion le temps de la requête.
			template <class... Args>
			core::Result<Rows>	query(std::string_view sql, const Args &...args)
			{
				auto connection = this->acquire();
				if (!connection)
					return std::unexpected(connection.error());
				return (*connection)->query(sql, args...);
			}

			std::size_t	idleCount() const;

		private:
			friend class PooledConnection;
			ConnectionPool(const PoolOptions &options, core::ILogger &logger,
				std::vector<Connection> connections);
			void	release(Connection connection) noexcept;

			PoolOptions				_options;
			core::ILogger			&_logger;
			mutable std::mutex		_mutex;
			std::condition_variable	_available;
			std::vector<Connection>	_idle;
	};
}
```

- **`friend class ConnectionPool;`** dans `PooledConnection` et `Transaction` : seul le
  pool peut appeler leurs constructeurs privés. On ne peut pas fabriquer une
  `PooledConnection` qui ne viendrait pas d'un pool ;
- **`PooledConnection` ne se copie pas, mais se déplace** : deux copies rendraient deux
  fois la même connexion. Un objet **déplacé** a `_pool == nullptr` et ne rend rien ;
- **`operator*` et `operator->`** : `(*pooled)->query(…)` s'utilise comme une
  `Connection`, à la manière d'un pointeur intelligent ;
- **`Transaction &operator=(Transaction &&) = delete;`** : réaffecter une transaction
  ouverte (que faire de l'ancienne ?) n'a pas de sens clair. On garde seulement le
  **constructeur** de déplacement, indispensable pour la renvoyer dans un `Result` ;
- **`ConnectionPool::query`** : une requête seule, hors transaction. Elle emprunte une
  connexion, s'en sert, et la rend à la fin de la fonction (destruction de
  `connection`). C'est un template, donc dans l'en-tête ;
- **`std::unexpected(connection.error())`** : on fait **suivre** l'erreur d'un
  `Result<PooledConnection>` dans un `Result<Rows>`. Les types de valeur diffèrent, le
  type d'erreur est le même.

### 8.2 `src/Pool.cpp`

```cpp
#include <pg/Pool.hpp>
#include <cassert>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace
{
	std::string_view	beginStatement(pg::Isolation isolation)
	{
		switch (isolation)
		{
			case pg::Isolation::ReadCommitted:
				return "BEGIN ISOLATION LEVEL READ COMMITTED";
			case pg::Isolation::RepeatableRead:
				return "BEGIN ISOLATION LEVEL REPEATABLE READ";
			case pg::Isolation::Serializable:
				return "BEGIN ISOLATION LEVEL SERIALIZABLE";
		}
		return "BEGIN";
	}
}

// ---- PooledConnection ----

pg::PooledConnection::PooledConnection(ConnectionPool &pool, Connection connection)
	: _pool(&pool), _connection(std::move(connection))
{
}

pg::PooledConnection::PooledConnection(PooledConnection &&other) noexcept
	: _pool(std::exchange(other._pool, nullptr)), _connection(std::move(other._connection))
{
}

pg::PooledConnection	&pg::PooledConnection::operator=(PooledConnection &&other) noexcept
{
	if (this != &other)
	{
		this->giveBack();
		this->_pool = std::exchange(other._pool, nullptr);
		this->_connection = std::move(other._connection);
	}
	return *this;
}

pg::PooledConnection::~PooledConnection()
{
	this->giveBack();
}

void	pg::PooledConnection::giveBack() noexcept
{
	if (this->_pool != nullptr)
		std::exchange(this->_pool, nullptr)->release(std::move(this->_connection));
}

pg::Connection	&pg::PooledConnection::operator*()
{
	return this->_connection;
}

pg::Connection	*pg::PooledConnection::operator->()
{
	return &this->_connection;
}

// ---- Transaction ----

pg::Transaction::Transaction(PooledConnection connection)
	: _connection(std::move(connection)), _open(true)
{
}

pg::Transaction::Transaction(Transaction &&other) noexcept
	: _connection(std::move(other._connection)), _open(std::exchange(other._open, false))
{
}

pg::Transaction::~Transaction()
{
	if (this->_open)
		(void)this->_connection->execute("ROLLBACK");
}

void	pg::Transaction::requireOpen() const
{
	if (!this->_open)
		throw std::logic_error("pg::Transaction used after commit() or rollback()");
}

core::Result<pg::Rows>	pg::Transaction::execute(std::string_view sql, const Params &params)
{
	this->requireOpen();
	return this->_connection->execute(sql, params);
}

core::Result<void>	pg::Transaction::commit()
{
	this->requireOpen();
	this->_open = false;
	const auto result = this->_connection->execute("COMMIT");
	if (!result)
		return std::unexpected(result.error());
	// Après une erreur dans la transaction, PostgreSQL répond à COMMIT par
	// « ROLLBACK », sans erreur : rien n'a été enregistré.
	if (result->command() == "ROLLBACK")
		return core::fail(core::ErrorCode::Internal,
			"transaction rolled back: an earlier query in it failed");
	return {};
}

core::Result<void>	pg::Transaction::rollback()
{
	this->requireOpen();
	this->_open = false;
	const auto result = this->_connection->execute("ROLLBACK");
	if (!result)
		return std::unexpected(result.error());
	return {};
}

// ---- ConnectionPool ----

core::Result<std::unique_ptr<pg::ConnectionPool>>	pg::ConnectionPool::open(
	const ConnectionSettings &settings, const PoolOptions &options, core::ILogger &logger)
{
	if (options.size == 0)
		return core::fail(core::ErrorCode::Invalid, "pool size must be at least 1");
	std::vector<Connection> connections;
	connections.reserve(options.size);

	// La première connexion attend que le serveur soit prêt.
	for (int attempt = 1; ; ++attempt)
	{
		auto connection = Connection::open(settings);
		if (connection)
		{
			connections.push_back(std::move(*connection));
			break;
		}
		logger.warning("PostgreSQL connection failed", {
			{"attempt", std::to_string(attempt) + "/" + std::to_string(options.connectAttempts)},
			{"error", connection.error().message}});
		if (attempt >= options.connectAttempts)
			return std::unexpected(connection.error());
		std::this_thread::sleep_for(options.retryDelay);
	}
	// Les suivantes : le serveur répond, un seul essai chacune.
	while (connections.size() < options.size)
	{
		auto connection = Connection::open(settings);
		if (!connection)
			return std::unexpected(connection.error());
		connections.push_back(std::move(*connection));
	}
	logger.info("PostgreSQL pool ready", {
		{"host", settings.host},
		{"database", settings.database},
		{"size", std::to_string(options.size)}});
	return std::unique_ptr<ConnectionPool>(
		new ConnectionPool(options, logger, std::move(connections)));
}

pg::ConnectionPool::ConnectionPool(const PoolOptions &options, core::ILogger &logger,
	std::vector<Connection> connections)
	: _options(options), _logger(logger), _idle(std::move(connections))
{
}

pg::ConnectionPool::~ConnectionPool()
{
	// Une connexion encore prêtée garderait un pointeur vers ce pool détruit.
	assert(this->_idle.size() == this->_options.size
		&& "pg::ConnectionPool destroyed while connections are still borrowed");
}

core::Result<pg::PooledConnection>	pg::ConnectionPool::acquire()
{
	return this->acquire(this->_options.acquireTimeout);
}

core::Result<pg::PooledConnection>	pg::ConnectionPool::acquire(std::chrono::milliseconds timeout)
{
	std::unique_lock lock(this->_mutex);
	if (!this->_available.wait_for(lock, timeout, [this] { return !this->_idle.empty(); }))
		return core::fail(core::ErrorCode::Unavailable,
			"no PostgreSQL connection available after "
			+ std::to_string(timeout.count()) + " ms");
	Connection connection = std::move(this->_idle.back());
	this->_idle.pop_back();
	return PooledConnection(*this, std::move(connection));
}

core::Result<pg::Transaction>	pg::ConnectionPool::begin(Isolation isolation)
{
	auto connection = this->acquire();
	if (!connection)
		return std::unexpected(connection.error());
	const auto started = (*connection)->execute(beginStatement(isolation));
	if (!started)
		return std::unexpected(started.error());
	return Transaction(std::move(*connection));
}

std::size_t	pg::ConnectionPool::idleCount() const
{
	const std::lock_guard lock(this->_mutex);
	return this->_idle.size();
}

void	pg::ConnectionPool::release(Connection connection) noexcept
{
	// Hors du verrou : ROLLBACK et PQreset parlent au serveur, ça peut prendre du temps.
	if (connection.inTransaction())
		(void)connection.execute("ROLLBACK");
	if (!connection.isHealthy())
	{
		this->_logger.warning("PostgreSQL connection lost, reconnecting");
		if (!connection.reset())
			this->_logger.error("PostgreSQL reconnection failed");
	}
	{
		const std::lock_guard lock(this->_mutex);
		// Ne réalloue jamais : la capacité vaut déjà la taille du pool.
		this->_idle.push_back(std::move(connection));
	}
	this->_available.notify_one();
}
```

En détail :

- **`std::exchange(x, nouvelle)`** : met `nouvelle` dans `x` et renvoie l'**ancienne**
  valeur, en une seule expression. Idéal pour les déplacements : on prend le pointeur
  de l'autre et on le remet à `nullptr` d'un coup ;
- **`giveBack`** est appelé par le destructeur **et** par l'opérateur de déplacement
  (avant d'écraser la connexion qu'on tenait). Il remet `_pool` à `nullptr` : rendre deux
  fois est impossible ;
- **`commit()`** passe `_open` à `false` **avant** d'envoyer `COMMIT` : qu'il réussisse
  ou non, la transaction est **terminée** pour PostgreSQL. Puis il vérifie
  `command() == "ROLLBACK"` (piège 2 du §1.5) ;
- **`~Transaction`** : le corps du destructeur s'exécute **avant** la destruction des
  membres. Le `ROLLBACK` part donc pendant que `_connection` existe encore, puis
  `_connection` est détruite et rend la connexion au pool ;
- **`open`** : la boucle de tentatives corrige l'audit 4.5 : `attempt` va de 1 à
  `connectAttempts` **inclus**, chaque échec est loggé avec `ILogger` (fini les
  `[INFO BDD]`), et en cas d'échec final on **renvoie l'erreur** au lieu de continuer.
  Pas d'attente après la dernière tentative ;
- **`new ConnectionPool(…)`** et pas `std::make_unique` : le constructeur est privé, et
  `make_unique` l'appellerait depuis **sa** fonction, qui n'a pas le droit. `open` est un
  membre de la classe : elle a le droit ;
- **`release`** fait le travail **lent** (`ROLLBACK`, `PQreset`) **hors du verrou**, comme
  `StreamLogger` construit sa ligne hors du verrou (guide 04, Q8). Puis
  `push_back` sous le verrou, puis `notify_one` ;
- **`release` est `noexcept`** : il est appelé depuis un destructeur. Le vecteur
  `_idle` a été créé avec une capacité de `size`, et il ne contient jamais plus de
  `size` connexions : `push_back` ne réalloue jamais, donc ne lance jamais
  `std::bad_alloc` ;
- **`~ConnectionPool`** vérifie avec `assert` que toutes les connexions sont rentrées.
  Une connexion encore prêtée garderait un pointeur vers un pool détruit (§1.4).
  `assert` disparaît en `Release` (`-DNDEBUG`), mais tous nos tests tournent en `Debug`.

### 8.3 La fixture reçoit un pool

Remplace `tests/support/PgTest.hpp` par :

```cpp
#pragma once
#include <core/Logger.hpp>
#include <pg/Pool.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <sstream>

// Base des tests qui parlent à un vrai PostgreSQL (variables PG_TEST_*).
// Sans PG_TEST_HOST, ces tests sont sautés : la lib reste testable sans base.
// Chaque test commence avec une table `items` vide.
class PgTest : public testing::Test
{
	protected:
		void	SetUp() override;
		std::unique_ptr<pg::ConnectionPool>	openPool(std::size_t size);

		// Membres détruits dans l'ordre inverse : le pool avant le logger qu'il utilise.
		pg::ConnectionSettings				settings;
		std::optional<pg::Connection>		connection;
		std::ostringstream					logs;
		core::StreamLogger					logger{logs, core::LogLevel::Debug};
		std::unique_ptr<pg::ConnectionPool>	pool;
};
```

Et dans `tests/support/PgTest.cpp`, ajoute à la **fin** de `SetUp` :

```cpp
	this->pool = this->openPool(2);
	ASSERT_NE(this->pool, nullptr);
}
```

puis, après `SetUp`, la nouvelle fonction :

```cpp
std::unique_ptr<pg::ConnectionPool>	PgTest::openPool(std::size_t size)
{
	pg::PoolOptions options;
	options.size = size;
	options.acquireTimeout = std::chrono::milliseconds{200};
	options.connectAttempts = 1;
	auto pool = pg::ConnectionPool::open(this->settings, options, this->logger);
	if (!pool)
	{
		ADD_FAILURE() << pool.error().message;
		return nullptr;
	}
	return std::move(*pool);
}
```

- **L'ordre des membres** : `logger` est déclaré **avant** `pool`. Les membres sont
  détruits dans l'ordre **inverse** : le pool (qui peut encore logger en rendant une
  connexion) disparaît avant le logger. C'est la question 6 du guide 04, appliquée ;
- `logs` capture tout ce que le pool écrit : un test pourra vérifier un message ;
- **un pool de 2** : assez pour deux transactions parallèles, assez petit pour tester
  l'épuisement ;
- `ADD_FAILURE()` : un échec **non fatal** (comme `EXPECT`), utilisable dans une
  fonction qui renvoie une valeur (un `ASSERT` y est interdit : il fait `return;`).

### 8.4 Les tests : `tests/TransactionTests.cpp`

```cpp
#include "support/PgTest.hpp"
#include <latch>
#include <thread>

namespace
{
	std::int64_t	countItems(pg::ConnectionPool &pool)
	{
		const auto rows = pool.query("SELECT count(*) AS n FROM items");
		return rows ? (*rows)[0].get<std::int64_t>("n") : -1;
	}
}

class TransactionTest : public PgTest {};

TEST_F(TransactionTest, CommitKeepsChanges)
{
	auto tx = this->pool->begin();
	ASSERT_TRUE(tx) << tx.error().message;
	ASSERT_TRUE(tx->query("INSERT INTO items (name) VALUES ($1)", "kept"));
	ASSERT_TRUE(tx->commit());
	EXPECT_EQ(countItems(*this->pool), 1);
}

TEST_F(TransactionTest, DestructorRollsBack)
{
	{
		auto tx = this->pool->begin();
		ASSERT_TRUE(tx) << tx.error().message;
		ASSERT_TRUE(tx->query("INSERT INTO items (name) VALUES ($1)", "forgotten"));
	}	// pas de commit() : ROLLBACK ici
	EXPECT_EQ(countItems(*this->pool), 0);
	EXPECT_EQ(this->pool->idleCount(), 2u);
}

TEST_F(TransactionTest, FailedQueryPoisonsTheTransaction)
{
	auto tx = this->pool->begin();
	ASSERT_TRUE(tx) << tx.error().message;
	ASSERT_TRUE(tx->query("INSERT INTO items (name) VALUES ($1)", "first"));
	ASSERT_FALSE(tx->query("INSERT INTO items (name) VALUES ($1)", "first"));	// doublon
	const auto next = tx->query("SELECT 1");
	ASSERT_FALSE(next);
	EXPECT_NE(next.error().message.find("25P02"), std::string::npos);
	const auto committed = tx->commit();
	ASSERT_FALSE(committed);
	EXPECT_EQ(countItems(*this->pool), 0);
}

TEST_F(TransactionTest, UseAfterCommitIsABug)
{
	auto tx = this->pool->begin();
	ASSERT_TRUE(tx) << tx.error().message;
	ASSERT_TRUE(tx->commit());
	EXPECT_THROW((void)tx->query("SELECT 1"), std::logic_error);
}

// Deux threads, deux transactions en même temps : chacune ne voit que ses
// propres lignes tant qu'elle n'a pas validé.
TEST_F(TransactionTest, ParallelTransactionsDoNotMix)
{
	std::latch bothInserted(2);
	bool sawOther[2] = {true, true};
	auto worker = [&](int id, bool commit)
	{
		auto tx = this->pool->begin();
		ASSERT_TRUE(tx) << tx.error().message;
		ASSERT_TRUE(tx->query("INSERT INTO items (name) VALUES ($1)", "thread-" + std::to_string(id)));
		bothInserted.arrive_and_wait();		// les deux INSERT sont faits, aucun n'est validé
		const auto rows = tx->query("SELECT count(*) AS n FROM items");
		ASSERT_TRUE(rows) << rows.error().message;
		sawOther[id] = (*rows)[0].get<std::int64_t>("n") != 1;
		if (commit)
		{
			ASSERT_TRUE(tx->commit());		// accolades : ASSERT_ cache un if/else
		}
	};
	std::thread first(worker, 0, true);
	std::thread second(worker, 1, false);
	first.join();
	second.join();
	EXPECT_FALSE(sawOther[0]);
	EXPECT_FALSE(sawOther[1]);
	const auto rows = this->pool->query("SELECT name FROM items");
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 1u);
	EXPECT_EQ((*rows)[0].get<std::string>("name"), "thread-0");
}
```

- **`DestructorRollsBack`** : le bloc `{ … }` détruit `tx` sans `commit()`. Puis on vérifie
  que la table est vide **et** que les deux connexions sont rentrées au pool ;
- **`FailedQueryPoisonsTheTransaction`** : les deux pièges du §1.5 dans un seul test ;
- **`ParallelTransactionsDoNotMix`** : `std::latch` (C++20) est un compte à rebours.
  Chaque thread fait son `INSERT`, puis `arrive_and_wait()` : il décompte, et attend que
  le compteur arrive à 0. Les deux `SELECT count(*)` n'ont donc lieu que quand **les
  deux** lignes existent, **non validées**. Chacune ne voit que **la sienne** (1 ligne).
  À la fin, seule la transaction validée laisse une trace ;
- **les accolades autour de `ASSERT_TRUE(tx->commit())`** : sans elles, la compilation
  échoue avec `suggest explicit braces to avoid ambiguous 'else'
  [-Werror=dangling-else]`. Les macros `ASSERT_*` cachent un `if … else` : derrière un
  `if (commit)` sans accolades, le `else` caché devient ambigu. Je suis tombé dessus en
  écrivant ce guide ;
- `sawOther[id]` : chaque thread écrit **sa** case du tableau. Deux threads qui écrivent
  dans deux variables différentes, ce n'est pas une *data race*.

### 8.5 Les tests : `tests/PoolTests.cpp`

```cpp
#include "support/PgTest.hpp"
#include <thread>

using namespace std::chrono;

class Pool : public PgTest {};

TEST_F(Pool, ExhaustedPoolTimesOut)
{
	auto first = this->pool->acquire();
	auto second = this->pool->acquire();
	ASSERT_TRUE(first && second);
	const auto start = steady_clock::now();
	const auto third = this->pool->acquire(milliseconds{100});
	const auto waited = steady_clock::now() - start;
	ASSERT_FALSE(third);
	EXPECT_EQ(third.error().code, core::ErrorCode::Unavailable);
	EXPECT_GE(waited, milliseconds{100});
}

TEST_F(Pool, WaiterGetsReleasedConnection)
{
	std::optional<pg::PooledConnection> first(*this->pool->acquire());
	std::optional<pg::PooledConnection> second(*this->pool->acquire());
	std::thread releaser([&]
	{
		std::this_thread::sleep_for(milliseconds{50});
		first.reset();		// rend la connexion au pool
	});
	const auto third = this->pool->acquire(seconds{2});
	releaser.join();
	EXPECT_TRUE(third);
}

TEST_F(Pool, ReleaseRollsBackForgottenTransaction)
{
	{
		auto connection = this->pool->acquire();
		ASSERT_TRUE(connection);
		ASSERT_TRUE((*connection)->query("BEGIN"));
		ASSERT_TRUE((*connection)->query("INSERT INTO items (name) VALUES ('ghost')"));
	}	// rendue au pool au milieu d'une transaction
	auto first = this->pool->acquire();
	auto second = this->pool->acquire();
	ASSERT_TRUE(first && second);
	EXPECT_FALSE((*first)->inTransaction());
	EXPECT_FALSE((*second)->inTransaction());
}

TEST_F(Pool, ReconnectsBrokenConnection)
{
	{
		auto victim = this->pool->acquire();
		ASSERT_TRUE(victim);
		const auto pid = (*victim)->query("SELECT pg_backend_pid() AS pid");
		ASSERT_TRUE(pid);
		auto killer = pg::Connection::open(this->settings);
		ASSERT_TRUE(killer);
		ASSERT_TRUE(killer->query("SELECT pg_terminate_backend($1)",
			(*pid)[0].get<int>("pid")));
		const auto lost = (*victim)->query("SELECT 1");
		ASSERT_FALSE(lost);
		EXPECT_EQ(lost.error().code, core::ErrorCode::Unavailable);
	}	// rendue cassée : le pool la reconnecte
	auto first = this->pool->acquire();
	auto second = this->pool->acquire();
	ASSERT_TRUE(first && second);
	EXPECT_TRUE((*first)->query("SELECT 1"));
	EXPECT_TRUE((*second)->query("SELECT 1"));
	EXPECT_NE(this->logs.str().find("PostgreSQL connection lost"), std::string::npos);
}
```

- **`ExhaustedPoolTimesOut`** : les deux connexions sont prises, la troisième demande
  attend 100 ms puis renvoie `Unavailable`. On mesure avec `steady_clock` (une horloge qui
  ne recule jamais, contrairement à l'heure système) ;
- **`WaiterGetsReleasedConnection`** : un thread rend une connexion après 50 ms
  (`first.reset()` détruit la `PooledConnection` dans l'`optional`). Le `acquire` qui
  dormait est réveillé par `notify_one` ;
- **`ReconnectsBrokenConnection`** : une **seconde** connexion tue la première côté
  serveur (`pg_terminate_backend`, l'équivalent d'un redémarrage de la base). La requête
  suivante échoue en `Unavailable` : c'est libpq qui constate la coupure
  (`server closed the connection unexpectedly`), donc **sans** SQLSTATE, le cas du §1.7.
  La connexion est rendue cassée, et le pool la répare avec `PQreset`. Le log le
  confirme.

### 8.6 Les déclarer et vérifier

`src/Pool.cpp` à la fin de `add_library`, `tests/TransactionTests.cpp` et
`tests/PoolTests.cpp` à la fin de `add_executable`.

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R '^(TransactionTest|Pool)\.'
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 9`.

### 8.7 Casser exprès : le `COMMIT` qui ment

Un contrôle qui n'a jamais échoué ne prouve rien (guide 02, §9.3). Dans
`src/Pool.cpp`, fonction `commit()`, désactive la vérification :

```cpp
	if (false && result->command() == "ROLLBACK")
```

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R '^TransactionTest\.FailedQuery'
```

✅ **Attendu** : le test échoue.

```
/app/libs/cpp-pg/tests/TransactionTests.cpp:<ligne>: Failure
Value of: committed
  Actual: true
Expected: false
```

`commit()` a dit « réussi », alors que la table est vide. **Remets** la ligne d'origine
(`if (result->command() == "ROLLBACK")`), recompile, et vérifie que le test repasse.

---

## 9. Étape H : installation, README, CHANGELOG

### 9.1 Le package-consumer

Remplace `libs/cpp-pg/tests/package-consumer/main.cpp` :

```cpp
#include <pg/Pool.hpp>

// Compile avec les en-têtes installés, et se lie à libpq : Connection::open
// vers un port fermé doit échouer proprement.
int main()
{
	pg::ConnectionSettings settings;
	settings.host = "127.0.0.1";
	settings.port = "1";
	const auto connection = pg::Connection::open(settings);
	const bool unavailable = !connection
		&& connection.error().code == core::ErrorCode::Unavailable;
	return (unavailable && pg::params(1, "a").size() == 2) ? 0 : 1;
}
```

Il ne demande pas de base : il prouve que les en-têtes installés compilent **et** que
l'édition de liens trouve libpq (`Connection::open` l'appelle). C'est ce que garantit
`find_dependency(PostgreSQL)`.

### 9.2 La lib seule, installée

cpp-pg seule a besoin de cpp-core **installée** (§4.4). On installe donc les deux dans
le même préfixe, cpp-core d'abord :

```sh
T="-DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=$VCPKG_INSTALLED_DIR -DVCPKG_MANIFEST_INSTALL=OFF"
rm -rf /tmp/core /tmp/pg /tmp/libs-inst /tmp/pg-cons
cmake -S libs/cpp-core -B /tmp/core $T -DCPP_CORE_BUILD_TESTS=OFF
cmake --build /tmp/core -j"$(nproc)"
cmake --install /tmp/core --prefix /tmp/libs-inst
cmake -S libs/cpp-pg -B /tmp/pg $T -DCMAKE_PREFIX_PATH=/tmp/libs-inst
cmake --build /tmp/pg -j"$(nproc)"
ctest --test-dir /tmp/pg
cmake --install /tmp/pg --prefix /tmp/libs-inst
find /tmp/libs-inst -type f -path '*pg*' | sort
cmake -S libs/cpp-pg/tests/package-consumer -B /tmp/pg-cons $T -DCMAKE_PREFIX_PATH=/tmp/libs-inst
cmake --build /tmp/pg-cons
/tmp/pg-cons/package_consumer && echo OK
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 32` (les `PG_TEST_*` du
conteneur sont là : les tests à base tournent aussi), puis 13 fichiers :

```
/tmp/libs-inst/include/pg/Connection.hpp
/tmp/libs-inst/include/pg/Convert.hpp
/tmp/libs-inst/include/pg/Params.hpp
/tmp/libs-inst/include/pg/Pool.hpp
/tmp/libs-inst/include/pg/Rows.hpp
/tmp/libs-inst/include/pg/Settings.hpp
/tmp/libs-inst/include/pg/SqlState.hpp
/tmp/libs-inst/include/pg/Version.hpp
/tmp/libs-inst/lib/cmake/cpp-pg/cpp-pgConfig.cmake
/tmp/libs-inst/lib/cmake/cpp-pg/cpp-pgConfigVersion.cmake
/tmp/libs-inst/lib/cmake/cpp-pg/cpp-pgTargets-noconfig.cmake
/tmp/libs-inst/lib/cmake/cpp-pg/cpp-pgTargets.cmake
/tmp/libs-inst/lib/libcpp-pg.a
```

et enfin `OK`.

**Curiosité** : sans la ligne `find_dependency(PostgreSQL)`, le package-consumer ne se
configure même pas. Je l'ai vérifié en la retirant du fichier installé :

```
CMake Error at /tmp/libs-inst/lib/cmake/cpp-pg/cpp-pgTargets.cmake:61 (set_target_properties):
  The link interface of target "cpp-pg::cpp-pg" contains:

    PostgreSQL::PostgreSQL
```

`cpp-pgTargets.cmake` dit « pour lier cpp-pg, il faut aussi `PostgreSQL::PostgreSQL` »,
mais personne n'a créé cette cible chez le consommateur.

Fais le ménage : `rm -rf /tmp/core /tmp/pg /tmp/libs-inst /tmp/pg-cons`.

### 9.3 `README.md`

Remplace le contenu de `libs/cpp-pg/README.md` :

````markdown
# cpp-pg

> Accès à PostgreSQL au-dessus de libpq : pool de connexions, transactions RAII,
> paramètres typés et lecture des colonnes par leur nom.

## Exemple minimal

```cpp
#include <pg/Pool.hpp>
#include <iostream>

int main()
{
    core::StreamLogger logger(std::cerr, core::LogLevel::Info);
    const core::SystemEnvironment environment;
    core::ConfigReader config(environment);
    const pg::ConnectionSettings settings = pg::readSettings(config, "DB_");
    if (!config.finish())
        return 1;

    auto pool = pg::ConnectionPool::open(settings, pg::PoolOptions{}, logger);
    if (!pool)
        return 1;

    auto tx = (*pool)->begin();
    if (!tx)
        return 1;
    const auto rows = tx->query(
        "INSERT INTO accounts (email, created_at) VALUES ($1, $2) RETURNING id",
        "alice@example.com", std::chrono::system_clock::now());
    if (!rows || !tx->commit())
        return 1;                    // sans commit(), le destructeur fait ROLLBACK
    std::cout << (*rows)[0].get<std::int64_t>("id") << '\n';
}
```

## Utilisation

Dans le monorepo (`backend/libs/CMakeLists.txt`), après cpp-core :

```cmake
add_subdirectory(cpp-core)
add_subdirectory(cpp-pg)
```

Depuis un autre projet, une fois installée (avec cpp-core) :

```cmake
find_package(cpp-pg 0.1 CONFIG REQUIRED)
target_link_libraries(mon_app PRIVATE cpp-pg::cpp-pg)
```

Option : `CPP_PG_BUILD_TESTS` (ON quand la lib est le projet principal, OFF sinon).
Les tests qui ont besoin d'une base lisent `PG_TEST_HOST`, `PG_TEST_PORT`,
`PG_TEST_NAME`, `PG_TEST_USER` et `PG_TEST_PASSWORD` ; sans `PG_TEST_HOST`, ils sont
sautés.

## Garanties

- `ConnectionPool` est thread-safe. `acquire()` attend au plus `acquireTimeout`, puis
  renvoie une erreur `Unavailable`.
- Une connexion rendue au pool est remise en état : `ROLLBACK` si une transaction est
  restée ouverte, reconnexion (`PQreset`) si elle est cassée.
- Une `Transaction` garde sa connexion du `BEGIN` au `COMMIT`. Détruite sans
  `commit()`, elle fait `ROLLBACK`.
- Les valeurs passent toujours en paramètres `$n`, jamais dans le texte SQL.
- Erreurs attendues en `core::Result` (`23505` → `Conflict`, `23xxx`/`22xxx` →
  `Invalid`, `40001`/`40P01`/`08xxx` → `Unavailable`, le reste → `Internal`).
  Erreurs de programmation (colonne inconnue, `NULL` lu sans `std::optional`,
  transaction utilisée après `commit()`) en exception.
- Chaque session est en `TimeZone=UTC` et `DateStyle=ISO`.

## Limites

- Format texte uniquement (pas de format binaire, pas de `bytea`).
- Pas de requêtes préparées (`PQprepare`), ni de mode pipeline, ni de `LISTEN/NOTIFY`.
- Une `Transaction` ne doit pas être partagée entre threads.
- Le pool doit vivre plus longtemps que les connexions et transactions qu'il prête.
````

L'exemple du README a été compilé tel quel (`-Wall -Wextra -Wpedantic -Werror`).

### 9.4 `CHANGELOG.md`

````markdown
# Changelog

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions : [SemVer](https://semver.org/lang/fr/).

## [Unreleased]

## [0.1.0]

### Added
- `Connection` : connexion RAII, requêtes à paramètres typés.
- `ConnectionPool`, `PooledConnection` : pool thread-safe avec délai d'attente et
  reconnexion.
- `Transaction` : `BEGIN`/`COMMIT`, `ROLLBACK` automatique, niveaux d'isolation.
- `Rows`, `Row::get<T>("colonne")` : lecture par nom, `std::optional` pour `NULL`,
  `TIMESTAMPTZ` en `pg::Timestamp`.
- `errorCodeFor` : codes SQLSTATE vers `core::ErrorCode`.
- `readSettings` : paramètres de connexion lus avec `core::ConfigReader`.
````

---

## 10. Étape I : vérifier tout le parcours

Sur l'hôte, depuis `~/matcha` :

```sh
make test
make sanitize
```

✅ **Attendu**, pour chacun : `100% tests passed, 0 tests failed out of 225`
(193 + 32).

Sous `sanitize`, les tests de cpp-pg tournent avec ASan, UBSan et LSan : aucune fuite
de `PGresult` ni de `PGconn`, aucune *data race* signalée dans les tests à deux threads.
C'est le critère de fin du plan : « tout passe sous `sanitize` ».

---

## 11. Étape J : commiter

Depuis `~/matcha` :

```sh
git add docker-compose.dev.yml backend/libs/cpp-pg backend/libs/CMakeLists.txt \
    backend/CMakePresets.json
git status
```

✅ **Attendu** : sous `Changes to be committed`, les 32 nouveaux fichiers de
`backend/libs/cpp-pg/` et 3 fichiers modifiés, rien d'autre.

```sh
git commit -m "cpp-pg : connexion, pool, transactions, paramètres typés (audit 4.4, 4.5)"
git log --oneline -3
```

Coche les tâches de cpp-pg du mardi dans le planning. La suite (QueryBuilder,
migrations, branchement dans le backend) est le guide 06. 🎉

---

## 12. Questions

Réponds sans regarder, puis relis la section indiquée si tu bloques. Écris sous chaque
**Ta réponse**, à la place de la ligne `…`. Préviens-moi quand tu as fini.

### Q1. Le mutex de `Database` (§1.1)

`Database::execute` prend un mutex à chaque appel. Pourquoi est-ce que ça ne suffit pas
à rendre une transaction sûre ? Et pourquoi la solution n'est-elle pas « garder le
mutex du `BEGIN` au `COMMIT` » ?

**Ta réponse :**

…

### Q2. `PRIVATE`, mais `find_dependency` (§1.3, §4.4, §9.2)

libpq est liée en `PRIVATE` à cpp-pg. Pourquoi faut-il quand même
`find_dependency(PostgreSQL)` dans `cpp-pgConfig.cmake.in` ? Et qu'est-ce que
`PRIVATE` épargne, concrètement, à un projet qui utilise cpp-pg ?

**Ta réponse :**

…

### Q3. `"alice"` devient `true` (§1.6)

Explique pourquoi, avec seulement `toText(bool)` et `toText(std::string_view)`,
`toText("alice")` appelle la version `bool`. Que se passerait-il avec un
`std::string name = "alice"; toText(name);` ?

**Ta réponse :**

…

### Q4. `Row` et `shared_ptr` (§1.3, §7.1)

Si `Row` gardait un simple `pg_result *` au lieu d'un `std::shared_ptr<pg_result>`,
que se passerait-il avec ce code ? Quel outil le détecterait ?

```cpp
const pg::Row row = (*pool->query("SELECT 1 AS n"))[0];
int n = row.get<int>("n");
```

**Ta réponse :**

…

### Q5. `wait_for` et son prédicat (§1.4, §8.2)

Pourquoi `acquire` passe-t-il une lambda à `wait_for` au lieu d'écrire simplement
`_available.wait_for(lock, timeout);` puis de prendre une connexion ?

**Ta réponse :**

…

### Q6. Le `COMMIT` qui ment (§1.5, §8.7)

Une transaction fait un `INSERT` qui échoue sur un doublon, et le code (mal écrit)
ignore l'erreur et appelle `commit()`. Que répond PostgreSQL, et que renverrait un
`commit()` qui se contenterait de vérifier `PQresultStatus` ?

**Ta réponse :**

…

### Q7. Le pool qui meurt trop tôt (§1.4, §8.2)

Que se passe-t-il ici, en `debug` puis en `release` ?

```cpp
pg::PooledConnection keep = [&] {
    auto pool = pg::ConnectionPool::open(settings, options, logger);
    return std::move(*(*pool)->acquire());
}();
```

**Ta réponse :**

…

### Q8. Pas de transaction annulée dans nos tests (§1.9)

Le plan proposait d'enfermer chaque test dans une transaction annulée à la fin. Pourquoi
ce guide vide-t-il plutôt la table avec `TRUNCATE` au début de chaque test ? Cite deux
tests qui ne pourraient pas fonctionner autrement.

**Ta réponse :**

…

---

## 13. Correction

> Remplie après tes réponses.
>
> **Légende** : 🟢 juste et complet · 🟡 juste mais incomplet ou imprécis ·
> 🔴 faux ou à côté · ⏳ pas encore corrigé

| Question | Évaluation |
|---|---|
| Q1. Le mutex de `Database` | ⏳ |
| Q2. `PRIVATE`, mais `find_dependency` | ⏳ |
| Q3. `"alice"` devient `true` | ⏳ |
| Q4. `Row` et `shared_ptr` | ⏳ |
| Q5. `wait_for` et son prédicat | ⏳ |
| Q6. Le `COMMIT` qui ment | ⏳ |
| Q7. Le pool qui meurt trop tôt | ⏳ |
| Q8. Pas de transaction annulée | ⏳ |
