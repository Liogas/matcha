# CMake : cibles et visibilité
> Une cible décrit ce qu'on construit, et PUBLIC ou PRIVATE décide de ce que ses utilisateurs héritent.

## Mise en situation
`cpp-pg` utilise [libpq](../sql/libpq.md) dans ses `.cpp`, et expose [`core::Result`](../cpp/result-expected.md) dans ses en-têtes. Le backend doit voir cpp-core, mais il n'a pas besoin de libpq dans ses chemins d'inclusion. CMake doit exprimer cette différence.

## La théorie
### Les étapes de la compilation
```text
.cpp -> compilation -> .o -> édition de liens -> programme
```

- Le compilateur a besoin des **chemins d'inclusion**.
- L'éditeur de liens a besoin des **bibliothèques**.

Une lib statique `.a` est une archive de `.o` : elle ne contient pas ses dépendances, et le programme final doit toutes les lier.

### Le CMake moderne
Tout tourne autour des cibles et de leurs propriétés :

```cmake
add_library(cpp-pg STATIC src/Pool.cpp src/Connection.cpp)
add_library(cpp-pg::cpp-pg ALIAS cpp-pg)

target_include_directories(cpp-pg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(cpp-pg
    PUBLIC cpp-core::cpp-core
    PRIVATE PostgreSQL::PostgreSQL)
```

### PUBLIC, PRIVATE, INTERFACE
- **PRIVATE** : nécessaire pour me compiler.
- **[INTERFACE](../cpp/interfaces-injection.md)** : nécessaire à ceux qui m'utilisent.
- **PUBLIC** : les deux.

La règle : si un type apparaît dans un en-tête public, la dépendance est PUBLIC. [`core::Result`](../cpp/result-expected.md) est dans `Pool.hpp`, donc PUBLIC. [`PGconn`](../sql/libpq.md) n'apparaît que dans les `.cpp`, ou caché, donc PRIVATE.

### La propagation
Si le backend lie `cpp-pg`, il reçoit automatiquement les propriétés PUBLIC, y compris le chemin de `cpp-core`. On déclare une fois, et CMake propage.

### Configurer et construire
```sh
cmake --preset debug          # configure
cmake --build --preset debug  # compile
```

Le fichier `CMakeLists.txt` n'est pas exécuté à la compilation : il génère les fichiers de build.

## Exemple déroulé
Le backend appelle `pg::ConnectionPool::open`, qui renvoie un [`core::Result`](../cpp/result-expected.md).

1. Le backend inclut `<pg/Pool.hpp>` : chemin hérité de `cpp-pg`.
2. Cet en-tête inclut `<core/Result.hpp>` : chemin hérité par la chaîne PUBLIC.
3. À l'édition de liens, CMake ajoute cpp-pg, cpp-core et [libpq](../sql/libpq.md), même PRIVATE, car une lib statique en a besoin.

Si `cpp-core` était PRIVATE, l'étape 2 échouerait avec `core/Result.hpp: No such file or directory`.

## L'ordre dans libs/CMakeLists.txt
Les libs basses d'abord : une lib qui teste `if(TARGET cpp-core::cpp-core)` doit la voir déjà définie.

## Erreurs fréquentes
- `include_directories` global : tout le monde voit tout.
- Une dépendance PRIVATE exposée dans un en-tête.
- Modifier `CMakeCache.txt` à la main.

## À retenir
On raisonne en cibles et en propriétés héritées, plus en options de compilation.

## Exercice
Pourquoi une dépendance PRIVATE d'une lib statique doit-elle quand même apparaître dans `Config.cmake` ?

## Corrigé
Le `.a` ne contient pas le code de ses dépendances. Le programme final doit les lier, donc `find_dependency` doit les retrouver chez le consommateur.

## Voir aussi

- [Rendre une lib installable](package-cmake.md)
- [Les presets CMake](presets.md)
- [vcpkg : gérer les dépendances](vcpkg.md)
