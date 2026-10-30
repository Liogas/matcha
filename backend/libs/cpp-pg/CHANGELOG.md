# Changelog

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions : [SemVer](https://semver.org/lang/fr/).

## [Unreleased]

## [0.2.0]

### Added
- `Executor` : interface commune à `Connection`, `Transaction` et `ConnectionPool`.
- `Connection::executeScript`, `Transaction::executeScript` : plusieurs instructions
  sans paramètres (fichiers de migration).
- `QueryBuilder`, `SortWhitelist`, `Query` : conditions optionnelles numérotées en
  `$n`, tri par liste blanche.
- `migrate`, `listMigrations` : migrations ordonnées, transactionnelles, verrouillées,
  avec somme de contrôle et mode `dryRun`.

### Changed
- `query(...)` vient désormais d'`Executor` ; `ConnectionPool::execute` emprunte une
  connexion le temps d'une requête.

### Fixed
- Tests : `RESOURCE_LOCK` était mal orthographié, les tests à base pouvaient tourner
  en parallèle.

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