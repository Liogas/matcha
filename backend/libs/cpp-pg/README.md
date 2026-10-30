# cpp-pg

> Accès à PostgreSQL au-dessus de libpq : pool de connexions, transactions RAII,
> paramètres typés, lecture des colonnes par leur nom, requêtes à filtres
> optionnels et migrations.

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

## Requêtes à filtres optionnels

```cpp
const pg::SortWhitelist sorts{{"newest", "p.id DESC"}, {"age", "p.birth DESC, p.id"}};
pg::QueryBuilder builder("SELECT p.id FROM profiles p");
builder.where("p.id <> ?", viewerId)
    .whereIf(city, "p.city = ?")          // std::optional : ajouté seulement s'il a une valeur
    .orderBy(sortFromUrl, sorts)          // clé inconnue : build() renvoie Invalid
    .limit(20);
const auto query = builder.build();        // core::Result<pg::Query>
if (query)
    rows = pool->execute(query->sql, query->params);
```

## Migrations

```cpp
pg::MigrationOptions options;            // dossier "migrations", table "schema_migrations"
const auto report = pg::migrate(*pool, options, logger);
```

Fichiers `NNN_nom.sql`, appliqués par numéro, tous dans une seule transaction, sous
un verrou consultatif. Le SHA-256 de chaque fichier appliqué est enregistré : un
fichier modifié après coup est refusé. `options.dryRun` applique puis annule.


## Utilisation

Dans le monorepo (`backend/libs/CMakeLists.txt`), après cpp-core :

```cmake
add_subdirectory(cpp-core)
add_subdirectory(cpp-pg)
```

Depuis un autre projet, une fois installée (avec cpp-core) :

```cmake
find_package(cpp-pg 0.2 CONFIG REQUIRED)
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
- `Connection`, `Transaction` et `ConnectionPool` sont des `pg::Executor` : un code
  qui reçoit un `Executor &` tourne aussi bien seul que dans une transaction.
- `QueryBuilder` ne met jamais une valeur dans le texte SQL ; un tri ne peut venir que
  d'une `SortWhitelist`.

## Limites

- Format texte uniquement (pas de format binaire, pas de `bytea`).
- Pas de requêtes préparées (`PQprepare`), ni de mode pipeline, ni de `LISTEN/NOTIFY`.
- Une `Transaction` ne doit pas être partagée entre threads.
- Le pool doit vivre plus longtemps que les connexions et transactions qu'il prête.
- `QueryBuilder` ne sait faire que des `SELECT` ; un « ? » littéral (dans une chaîne
  SQL, ou l'opérateur `jsonb ?`) ne peut pas apparaître dans une condition.
- Les migrations tournent dans une transaction : pas de `CREATE INDEX CONCURRENTLY`.