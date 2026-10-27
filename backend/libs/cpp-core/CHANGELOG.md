# Changelog

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions : [SemVer](https://semver.org/lang/fr/).

## [Unreleased]

## [0.1.0]

### Added
- `ErrorCode`, `Error` et `Result<T>` (sur `std::expected`), avec `fail()`.
- `IEnvironment`, `SystemEnvironment`, `MapEnvironment`.
- `ConfigReader` : variables requises et optionnelles, entiers, booléens stricts,
  secrets via `NAME` ou `NAME_FILE`, erreurs rapportées ensemble.
- `ILogger` et `StreamLogger` : niveaux, format logfmt, masquage des champs sensibles,
  échappement des retours à la ligne.