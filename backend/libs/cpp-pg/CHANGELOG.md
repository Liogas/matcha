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