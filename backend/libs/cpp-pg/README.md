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