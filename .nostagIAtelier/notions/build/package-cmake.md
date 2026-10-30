# Rendre une lib installable
> Un package CMake permet à un autre projet d'écrire find_package et d'obtenir une cible prête à l'emploi.

## Mise en situation
`cpp-pg` fonctionne dans le monorepo, avec `add_subdirectory`. Mais une lib présentée comme réutilisable doit pouvoir s'installer, et servir depuis un autre dépôt, sans ses sources.

## La théorie
### Deux façons de consommer
- **Sous-projet** : `add_subdirectory(cpp-pg)`, avec les sources.
- **Package** : `find_package(cpp-pg REQUIRED)`, depuis une version installée.

Dans les deux cas, on veut écrire `cpp-pg::cpp-pg`. D'où l'alias :

```cmake
add_library(cpp-pg::cpp-pg ALIAS cpp-pg)
```

### Installer
```cmake
install(TARGETS cpp-pg EXPORT cpp-pgTargets ...)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT cpp-pgTargets NAMESPACE cpp-pg:: DESTINATION lib/cmake/cpp-pg)
```

`EXPORT` génère un fichier décrivant la cible : emplacement, chemins, dépendances.

### Le fichier de configuration
`find_package` cherche `cpp-pgConfig.cmake`. Le gabarit `.in` est :

```cmake
@PACKAGE_INIT@
include(CMakeFindDependencyMacro)
find_dependency(cpp-core)
find_dependency(PostgreSQL)
include("${CMAKE_CURRENT_LIST_DIR}/cpp-pgTargets.cmake")
check_required_components(cpp-pg)
```

Chaque dépendance doit y figurer, même PRIVATE, car une lib statique ne contient pas ses dépendances.

### BUILD_INTERFACE
```cmake
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
$<INSTALL_INTERFACE:include>
```

Le chemin diffère selon qu'on compile depuis les sources ou qu'on utilise la version installée.

## Exemple déroulé : le package-consumer
`tests/package-consumer` est un petit projet indépendant :

```cmake
find_package(cpp-pg 0.2 REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE cpp-pg::cpp-pg)
```

Il installe la lib dans un dossier [temporaire](../cpp/duree-de-vie-objets.md), puis compile ce programme contre elle. Il attrape ce que les tests normaux ne voient pas :

- un en-tête public non installé ;
- un `find_dependency` manquant ;
- un chemin absolu vers tes sources.

## Le gabarit
`libs/new-lib.sh` génère tout cela depuis `libs/_template`.

## Erreurs fréquentes
- Oublier un `find_dependency`.
- Un en-tête public qui inclut un en-tête privé.
- Oublier d'augmenter la version.

## À retenir
Une lib est installable quand un projet extérieur la consomme sans connaître ses sources. Le package-consumer le prouve.

## Exercice
Pourquoi les tests normaux ne détectent-ils pas un `find_dependency` oublié ?

## Corrigé
En sous-projet, les cibles existent déjà dans le même build. Le fichier `Config.cmake` n'est jamais lu. Seul un consommateur externe passe par `find_package`.

## Voir aussi

- [CMake : cibles et visibilité](cmake-cibles.md)
- [Semver et CHANGELOG](semver.md)
- [Une bibliothèque : cœur et adaptateur](../archi/lib-coeur-adaptateur.md)
