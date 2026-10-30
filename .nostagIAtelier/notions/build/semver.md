# Semver et CHANGELOG
> Le numéro de version annonce la nature des changements.

## Mise en situation
Un projet utilise `cpp-pg 0.2.0`. Une nouvelle version sort. Peut-il la prendre sans risque ? Le numéro doit répondre à cette question, sans lire le code.

## La théorie
### MAJEUR.MINEUR.CORRECTIF
- **MAJEUR** : cassure de compatibilité.
- **MINEUR** : ajout compatible.
- **CORRECTIF** : correction compatible.

Avant 1.0, tout peut encore changer : une version mineure peut casser l'API. C'est le cas de `cpp-pg`.

### Qu'est-ce qu'une cassure
Tout ce qui oblige un utilisateur à modifier son code : renommer une fonction, changer un paramètre, retirer un en-tête, modifier le comportement documenté.

### Le CHANGELOG
Un fichier pour les humains, du plus récent au plus ancien :

```markdown
## [0.2.0]
### Added
- QueryBuilder
- MigrationRunner
### Changed
- Executor : interface commune
### Fixed
- ...
```

### Dans CMake
```cmake
project(cpp-pg VERSION 0.2.0 LANGUAGES CXX)
```

`find_package(cpp-pg 0.2 REQUIRED)` vérifie la compatibilité. Le fichier de version généré applique une politique, comme `SameMajorVersion`.

## Exemple déroulé
```text
0.1.0 : connexion, pool, transactions
0.2.0 : QueryBuilder, migrations, Executor
0.2.1 : correction de la numérotation des $n
1.0.0 : API stable annoncée
2.0.0 : Rows change de nom
```

## Erreurs fréquentes
- Publier une cassure en correctif.
- Oublier le CHANGELOG.
- Un numéro différent dans [CMake](cmake-cibles.md) et le CHANGELOG.

## À retenir
La version est une promesse faite à celui qui utilise ta lib.

## Exercice
Ajouter [`QueryBuilder`](../sql/query-builder.md) à une lib en 1.2.3 donne quelle version ? Et renommer `execute` ?

## Corrigé
Un ajout compatible donne 1.3.0. Un renommage casse les appelants, donc 2.0.0.

## Voir aussi

- [Rendre une lib installable](package-cmake.md)
