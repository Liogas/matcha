# Guide 02 : le gabarit `libs/_template`

> Planning : lundi, « Monorepo › Gabarit libs/_template ». Durée : environ 1 h 30,
> lecture comprise.
>
> Comme le guide 01 : je te dis **exactement** quoi faire et **pourquoi**, c'est toi qui
> tapes. Tout a été vérifié sur une copie du projet, dans l'image `matcha_backend:dev`
> (GCC 12.2, CMake 3.25).
>
> Nouveauté : les questions de fin (§13) ont une zone de réponse. Quand tu as fini,
> préviens-moi et je remplis la **correction** (§14).

## Sommaire

0. [Ce qu'on fait, et le résultat attendu](#0-ce-quon-fait-et-le-résultat-attendu)
1. [Les notions à connaître avant de commencer](#1-les-notions-à-connaître-avant-de-commencer)
2. [Étape A : partir d'un dépôt propre](#2-étape-a--partir-dun-dépôt-propre)
3. [Étape B : créer l'arborescence](#3-étape-b--créer-larborescence)
4. [Étape C : le code d'exemple](#4-étape-c--le-code-dexemple)
5. [Étape D : le `CMakeLists.txt`](#5-étape-d--le-cmakeliststxt)
6. [Étape E : `cmake/cpp-templateConfig.cmake.in`](#6-étape-e--cmakecpp-templateconfigcmakein)
7. [Étape F : le package-consumer](#7-étape-f--le-package-consumer)
8. [Étape G : `vcpkg.json`, `README.md`, `CHANGELOG.md`](#8-étape-g--vcpkgjson-readmemd-changelogmd)
9. [Étape H : vérifier le gabarit](#9-étape-h--vérifier-le-gabarit)
10. [Étape I : le script `new-lib.sh`](#10-étape-i--le-script-new-libsh)
11. [Étape J : essayer le script, puis jeter le résultat](#11-étape-j--essayer-le-script-puis-jeter-le-résultat)
12. [Étape K : commiter](#12-étape-k--commiter)
13. [Questions](#13-questions)
14. [Correction](#14-correction)

---

## 0. Ce qu'on fait, et le résultat attendu

Cette semaine, tu vas créer six bibliothèques (cpp-core, cpp-pg, cpp-validation,
cpp-geo, cpp-realtime, cpp-notify), plus des adaptateurs `-crow`. Chacune doit respecter
la **définition de terminé** du planning :

- en-têtes publics dans `include/<nom>/`, code privé dans `src/` ;
- `CMakeLists.txt` **installable**, `<nom>Config.cmake`, `tests/package-consumer` qui passe ;
- `README`, `CHANGELOG`, version `0.1.0` ;
- `vcpkg.json` propre, tests désactivés par défaut en sous-projet.

Refaire tout ça six fois à la main, c'est six occasions d'oublier une ligne. On écrit donc
**une fois** une bibliothèque minuscule mais complète, qui coche toutes les cases : le
**gabarit**. Pour chaque nouvelle lib, un script le copie et le renomme.

```
backend/libs/
├── CMakeLists.txt                     (inchangé : le gabarit n'y est PAS déclaré)
├── new-lib.sh                         ← nouveau : crée une lib depuis le gabarit
├── cpp-auth/
└── _template/                         ← nouveau
    ├── CMakeLists.txt                 cible, installation, tests
    ├── vcpkg.json
    ├── README.md
    ├── CHANGELOG.md
    ├── cmake/
    │   └── cpp-templateConfig.cmake.in    modèle du fichier lu par find_package
    ├── include/
    │   └── tmpl/
    │       └── Version.hpp            en-tête public
    ├── src/
    │   └── Version.cpp                code privé
    └── tests/
        ├── VersionTests.cpp           test GoogleTest
        └── package-consumer/          projet externe qui utilise la lib installée
            ├── CMakeLists.txt
            └── main.cpp
```

À la fin :

| Vérification | Résultat attendu |
|---|---|
| Gabarit compilé seul | `CPP_TEMPLATE_BUILD_TESTS:BOOL=ON`, 1 test sur 1 passe |
| Gabarit installé | 6 fichiers dans le préfixe d'installation |
| package-consumer | `OK` |
| `new-lib.sh cpp-demo demo` | 10 fichiers, aucune trace de `tmpl`/`template` |
| `cpp-demo` dans le monorepo | `CPP_DEMO_BUILD_TESTS` suit l'option, 1 test passe |

---

## 1. Les notions à connaître avant de commencer

Le guide 01 t'a fait **utiliser** la mécanique d'installation de cpp-auth (étape J). Ici,
tu vas l'**écrire**. Ces notions sont nouvelles : lis-les avant de toucher au code.

### 1.1 « Installer » une bibliothèque, ça veut dire quoi ?

Compiler produit `libcpp-template.a` quelque part dans `build/`. Ce fichier seul ne suffit
pas à un autre projet : il lui faut aussi les **en-têtes** (pour `#include`) et un moyen
de dire à CMake **où** sont ces fichiers et **comment** lier la lib.

**Installer**, c'est copier ces trois choses dans un dossier appelé **préfixe** (par
défaut `/usr/local`, chez nous `/tmp/inst` pour les tests) :

```
/tmp/inst/
├── include/tmpl/Version.hpp                     ← les en-têtes publics
└── lib/
    ├── libcpp-template.a                        ← la lib compilée
    └── cmake/cpp-template/                      ← le « mode d'emploi » pour CMake
        ├── cpp-templateConfig.cmake
        ├── cpp-templateConfigVersion.cmake
        ├── cpp-templateTargets.cmake
        └── cpp-templateTargets-noconfig.cmake
```

Dans le `CMakeLists.txt`, chaque commande `install(...)` ajoute des fichiers à cette liste.
Rien n'est copié pendant `cmake --build` : la copie a lieu seulement avec
`cmake --install build --prefix <dossier>`.

### 1.2 Les quatre fichiers de `lib/cmake/cpp-template/`

Quand un projet écrit `find_package(cpp-template CONFIG REQUIRED)`, CMake cherche un
fichier `cpp-templateConfig.cmake` (le mode `CONFIG`) dans les dossiers listés par
`CMAKE_PREFIX_PATH`. Ce fichier et ses voisins jouent chacun un rôle :

| Fichier                              | Rôle                                                                                                                                           | Qui l'écrit                                  |
| ------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------- |
| `cpp-templateConfig.cmake`           | Point d'entrée lu par `find_package`. Retrouve les dépendances, puis charge `Targets.cmake`                                                    | **toi**, via le modèle `.cmake.in` (étape E) |
| `cpp-templateConfigVersion.cmake`    | Répond à « la version installée convient-elle à celle demandée ? »                                                                             | CMake (`write_basic_package_version_file`)   |
| `cpp-templateTargets.cmake`          | Crée la cible **importée** `cpp-template::cpp-template`, avec ses dossiers d'include et ses dépendances                                        | CMake (`install(EXPORT)`)                    |
| `cpp-templateTargets-noconfig.cmake` | Donne le chemin du `.a` pour un type de build. `noconfig` car on n'a pas passé `CMAKE_BUILD_TYPE` ; avec `Release`, ce serait `-release.cmake` | CMake                                        |

Une **cible importée** est une cible qui représente quelque chose de déjà compilé : CMake
ne la compile pas, il sait juste où la trouver. C'est ce que reçoit le package-consumer.

### 1.3 L'*export set* et le `NAMESPACE`

Le chemin d'une cible vers `Targets.cmake` passe par un nom intermédiaire :

```cmake
install(TARGETS cpp-template EXPORT cpp-templateTargets ...)    # 1. range la cible
                                                                #    dans l'export set
install(EXPORT cpp-templateTargets                              # 2. écrit l'export set
    FILE cpp-templateTargets.cmake                              #    dans ce fichier
    NAMESPACE cpp-template::                                    # 3. préfixe les noms
    ...)
```

- `EXPORT cpp-templateTargets` : un **export set**, une liste de cibles à exporter
  ensemble. Une lib pourrait en exporter plusieurs (par exemple un cœur et un adaptateur).
- `NAMESPACE cpp-template::` : la cible `cpp-template` devient
  `cpp-template::cpp-template` dans le projet qui l'installe.

C'est la suite du §1.4 du guide 01 : dans le monorepo, l'**alias**
`cpp-template::cpp-template` donne exactement ce même nom. Le code qui utilise la lib
s'écrit donc pareil, qu'elle vienne de `add_subdirectory` ou de `find_package`.

### 1.4 `$<BUILD_INTERFACE:...>` et `$<INSTALL_INTERFACE:...>`

Le dossier des en-têtes n'est pas au même endroit selon le contexte :

- **pendant le build** (monorepo, ou lib seule) : `backend/libs/cpp-template/include` ;
- **une fois installée** : `<préfixe>/include`.

On écrit donc les deux, entourés d'**expressions génératrices** (*generator expressions*),
c'est-à-dire des valeurs que CMake choisit à la génération, selon le contexte :

```cmake
target_include_directories(cpp-template
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>   # utilisé pendant le build
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>         # écrit dans Targets.cmake
)
```

Sans elles, `Targets.cmake` contiendrait le chemin absolu de **ta** machine
(`/app/libs/...`), qui n'existe pas chez celui qui installe la lib. CMake le sait et refuse
d'ailleurs d'exporter un chemin source absolu : `install(EXPORT)` échoue.

### 1.5 `Config.cmake.in`, `@PACKAGE_INIT@` et `find_dependency`

`cpp-templateConfig.cmake` est **généré** à partir d'un modèle, `cmake/cpp-templateConfig.cmake.in`,
par `configure_package_config_file`. Le `.in` veut dire « fichier à compléter » : CMake y
remplace les `@...@`.

- `@PACKAGE_INIT@` : remplacé par quelques lignes qui calculent le préfixe d'installation
  **depuis l'emplacement du fichier**. Le paquet reste donc valide si on déplace le dossier
  d'installation. Elles définissent aussi `check_required_components`.
- `find_dependency(X)` : comme `find_package(X REQUIRED)`, mais si `X` manque, l'erreur
  dit « cpp-template a besoin de X », au lieu d'une erreur obscure plus loin.

**Quelles dépendances lister ?** Toutes celles passées à `target_link_libraries`,
**`PUBLIC` comme `PRIVATE`**. Ça surprend, mais c'est parce que nos libs sont **statiques** :

- un `.a` n'est qu'une archive de `.o`, il ne contient pas ses dépendances ;
- c'est l'**exécutable final** qui doit lier la lib *et* tout ce qu'elle utilise ;
- CMake exporte donc une dépendance `PRIVATE` sous la forme `$<LINK_ONLY:Threads::Threads>` :
  « pas besoin de ses en-têtes, mais il faudra la lier ».

Vérifié : avec `target_link_libraries(cpp-demo PRIVATE Threads::Threads)` et sans
`find_dependency(Threads)`, le package-consumer échoue avec :

```
The link interface of target "cpp-demo::cpp-demo" contains:
    Threads::Threads
```

C'est pour ça que le `Config.cmake.in` de cpp-auth cherche `nlohmann_json` et `httplib`,
alors qu'ils sont en `PRIVATE`.

Le gabarit n'a **aucune** dépendance (hors tests) : son `Config.cmake.in` ne contient qu'un
commentaire à la place des `find_dependency`. cpp-core en aura zéro aussi ; cpp-pg y
mettra `cpp-core` et `PostgreSQL`.

### 1.6 La version : SemVer et `SameMajorVersion`

Versions au format `MAJEUR.MINEUR.CORRECTIF` (*Semantic Versioning*) :

| On change… | Quand | Exemple |
|---|---|---|
| CORRECTIF | correction de bug, rien ne casse | `0.1.0` → `0.1.1` |
| MINEUR | ajout compatible (nouvelle fonction) | `0.1.1` → `0.2.0` |
| MAJEUR | changement qui casse les utilisateurs | `0.2.0` → `1.0.0` |

`write_basic_package_version_file(... COMPATIBILITY SameMajorVersion)` génère un
`ConfigVersion.cmake` qui accepte une version installée si elle a **le même majeur** et est
**au moins** aussi récente que celle demandée. Avec la lib en `0.1.0` :

| Le projet demande | Résultat |
|---|---|
| `find_package(cpp-template CONFIG REQUIRED)` (sans version) | accepté |
| `find_package(cpp-template 0.1 CONFIG REQUIRED)` | accepté |
| `find_package(cpp-template 1.0 CONFIG REQUIRED)` | **refusé** : `Could not find a configuration file for package "cpp-template" that is compatible with requested version "1.0"` |

### 1.7 `GNUInstallDirs`

`include(GNUInstallDirs)` définit des variables standard pour les sous-dossiers
d'installation : `CMAKE_INSTALL_LIBDIR` (`lib`, ou `lib64` sur certaines distributions),
`CMAKE_INSTALL_INCLUDEDIR` (`include`)… On les utilise au lieu d'écrire `lib` en dur, pour
que la lib s'installe là où chaque système l'attend.

### 1.8 Le nom de la lib s'écrit de trois façons

Dans le gabarit, le nom « template » apparaît sous trois formes, parce que chaque endroit a
ses règles :

| Forme | Où | Règle |
|---|---|---|
| `cpp-template` | nom de projet, cible, fichiers CMake, `vcpkg.json` | tirets autorisés |
| `CPP_TEMPLATE` | options et macros (`CPP_TEMPLATE_BUILD_TESTS`, `CPP_TEMPLATE_VERSION`) | identifiant C/CMake : pas de tiret, majuscules par convention |
| `tmpl` | namespace C++ et dossier `include/tmpl/` | identifiant C++ court ; pas `template`, qui est un **mot-clé** C++ (`namespace template` ne compile pas) |

Le script de l'étape I remplace ces trois formes. Choisir des valeurs **distinctes et
rares** (`tmpl` n'apparaît nulle part ailleurs) garantit qu'il ne remplace rien d'autre.

### 1.9 Pourquoi `_template` n'est pas dans `libs/CMakeLists.txt`

Le `_` au début du nom signale « ceci n'est pas une vraie lib ». On ne l'ajoute pas avec
`add_subdirectory` : il n'a rien à faire dans le binaire du backend, et ses tests
n'apportent rien au backend.

Conséquence à garder en tête : `make test` **ne compile pas** le gabarit. Si tu le modifies,
c'est à toi de le revérifier avec la procédure de l'étape H.

---

## 2. Étape A : partir d'un dépôt propre

Depuis `~/matcha` :

```sh
git status
```

✅ **Attendu** : seulement les deux fichiers de documentation que j'ai ajoutés.

```
modified:   docs/README.md
Untracked files:
        docs/guides/02-libs-template.md
```

Commite-les à part, avant de commencer, pour que ton commit du gabarit ne contienne que
ton travail :

```sh
git add docs/README.md docs/guides/02-libs-template.md
git status                 # vérifie la liste « Changes to be committed »
git commit -m "docs : guide 02 (gabarit de bibliothèque)"
```

Tu feras un autre commit de ce fichier plus tard, quand tu auras écrit tes réponses au §13.

Puis une branche pour ce travail :

```sh
git switch -c libs/template
```

**Pourquoi une branche** : si le gabarit part dans une mauvaise direction, tu reviens à la
branche précédente sans rien défaire à la main.

---

## 3. Étape B : créer l'arborescence

```sh
cd ~/matcha/backend/libs
mkdir -p _template/{cmake,include/tmpl,src,tests/package-consumer}
```

`{a,b,c}` est une **expansion d'accolades** de bash : la commande devient
`mkdir -p _template/cmake _template/include/tmpl ...`. `-p` crée les dossiers parents
manquants (`_template/`, `_template/tests/`) et n'échoue pas s'ils existent déjà.

```sh
find _template -type d
```

✅ **Attendu** (l'ordre peut varier) :

```
_template
_template/cmake
_template/include
_template/include/tmpl
_template/src
_template/tests
_template/tests/package-consumer
```

**Pourquoi `include/tmpl/` et pas directement `include/`** : le code qui utilise la lib
écrit `#include <tmpl/Version.hpp>`. Le préfixe `tmpl/` évite les collisions : cpp-core et
cpp-pg pourront avoir chacun un `Error.hpp`, l'un dans `core/`, l'autre dans `pg/`. Une fois
installés, tous les en-têtes atterrissent dans le même `<préfixe>/include/` : sans
sous-dossier, ils s'écraseraient.

---

## 4. Étape C : le code d'exemple

Le gabarit a besoin d'**un peu** de vrai code, sinon impossible de vérifier que la lib
compile, s'installe et s'utilise. On prend la plus petite chose utile dans toutes les libs :
une fonction `version()`. Elle fait passer la chaîne complète : en-tête public, code privé,
réglage CMake, test, package-consumer.

### 4.1 `_template/include/tmpl/Version.hpp`

```cpp
#pragma once

#include <string_view>

namespace tmpl
{

// Version de la bibliothèque, au format MAJEUR.MINEUR.CORRECTIF.
std::string_view version() noexcept;

}
```

- `#pragma once` : empêche une double inclusion, comme dans le reste du backend.
- `std::string_view` : une vue en lecture seule sur une chaîne, sans copie. La chaîne de
  version est un littéral, qui existe pendant tout le programme : une vue suffit.
- `noexcept` : renvoyer un littéral ne peut pas échouer, on le promet au compilateur.
- **Seulement la déclaration** : l'en-tête est public, il est installé et lu par les
  autres projets. On y met le *quoi*, pas le *comment*.

### 4.2 `_template/src/Version.cpp`

```cpp
#include <tmpl/Version.hpp>

namespace tmpl
{

std::string_view version() noexcept
{
    return CPP_TEMPLATE_VERSION;
}

}
```

- `#include <tmpl/Version.hpp>` avec des chevrons, **comme un utilisateur** de la lib.
  Si le chemin d'include est mal réglé dans CMake, ça casse ici tout de suite, pas chez
  le premier utilisateur.
- `CPP_TEMPLATE_VERSION` n'est défini nulle part dans le code : c'est une **macro passée
  par CMake** (étape D, `target_compile_definitions`). La version n'est écrite qu'à un
  endroit, `project(... VERSION 0.1.0)`, et le code la reçoit de là.

### 4.3 `_template/tests/VersionTests.cpp`

```cpp
#include <tmpl/Version.hpp>

#include <gtest/gtest.h>

#include <regex>
#include <string>

TEST(Version, FollowsSemanticVersioning)
{
    const std::string v{tmpl::version()};

    EXPECT_TRUE(std::regex_match(v, std::regex{R"(\d+\.\d+\.\d+)"})) << v;
}
```

- `TEST(Version, FollowsSemanticVersioning)` : un test GoogleTest. Le premier nom est le
  groupe, le second le cas. CTest l'affichera `Version.FollowsSemanticVersioning`.
- On ne teste pas `== "0.1.0"` : le test casserait à chaque nouvelle version. On teste la
  **forme** (trois nombres séparés par des points), qui ne doit jamais changer.
- `R"(...)"` : une **chaîne brute**, où `\d` n'a pas besoin d'être écrit `\\d`.
- `<< v` : si le test échoue, GoogleTest affiche la valeur reçue. Sans ça, tu saurais
  seulement que « c'est faux ».

---

## 5. Étape D : le `CMakeLists.txt`

Crée `_template/CMakeLists.txt`. On l'écrit en quatre blocs. Il reprend la mécanique de
cpp-auth, avec quelques améliorations signalées par **(≠ cpp-auth)**.

### 5.1 L'en-tête

```cmake
cmake_minimum_required(VERSION 3.21)

project(cpp-template VERSION 0.1.0 LANGUAGES CXX)

option(CPP_TEMPLATE_BUILD_TESTS "Build cpp-template tests" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
```

- `3.21` : nécessaire pour `PROJECT_IS_TOP_LEVEL` (guide 01, §1.5).
- **(≠ cpp-auth)** La version est donnée **directement** à `project()`. CMake la range dans
  `PROJECT_VERSION`, qu'on réutilise partout. cpp-auth passe par une variable
  intermédiaire `CPP_AUTH_VERSION` : ça marche, mais c'est une étape de plus.
- `option(...)` : exactement la correction de l'étape F du guide 01. Tests ON quand la lib
  est le projet principal, OFF quand elle est un sous-projet.
- `CMakePackageConfigHelpers` : fournit `configure_package_config_file` et
  `write_basic_package_version_file` (§1.5, §1.6).

### 5.2 La bibliothèque

```cmake
# --- La bibliothèque ---------------------------------------------------------

add_library(cpp-template STATIC
    src/Version.cpp
)
add_library(cpp-template::cpp-template ALIAS cpp-template)

target_compile_features(cpp-template PUBLIC cxx_std_23)

target_include_directories(cpp-template
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(cpp-template
    PRIVATE CPP_TEMPLATE_VERSION="${PROJECT_VERSION}"
)
```

- `STATIC` : une archive `.a`, liée dans le binaire. C'est ce que fait déjà vcpkg pour
  toutes les dépendances (fiche Docker, §2) : l'image de prod n'a aucune `.so` à fournir.
- L'alias : guide 01, §1.4.
- **(≠ cpp-auth)** `cxx_std_23` au lieu de `cxx_std_17`. cpp-core exposera
  `Result<T> = std::expected<T, Error>` dans ses en-têtes publics, et `std::expected`
  n'existe qu'à partir de C++23 (vérifié : GCC 12 de l'image le fournit). Comme toutes les
  libs dépendront de cpp-core, autant que le gabarit parte en 23. `PUBLIC` car les
  utilisateurs de la lib liront ses en-têtes et doivent compiler en C++23 eux aussi.
  Le backend est déjà en C++23 (`CMAKE_CXX_STANDARD 23`).
- Les include : §1.4.
- `target_compile_definitions(... PRIVATE ...)` : équivaut à passer
  `-DCPP_TEMPLATE_VERSION="0.1.0"` au compilateur. **`PRIVATE`** : seul `Version.cpp` en a
  besoin. En `PUBLIC`, tout projet utilisant la lib recevrait cette macro, ce qui n'a aucun
  sens pour lui.

### 5.3 L'installation

```cmake
# --- Installation ------------------------------------------------------------

install(TARGETS cpp-template
    EXPORT cpp-templateTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT cpp-templateTargets
    FILE cpp-templateTargets.cmake
    NAMESPACE cpp-template::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-template
)

configure_package_config_file(
    cmake/cpp-templateConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-templateConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-template
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-templateConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-templateConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-templateConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-template
)
```

Dans l'ordre :

1. **`install(TARGETS)`** : copie le `.a` dans `lib/` et range la cible dans l'export set
   (§1.3). `ARCHIVE` désigne les libs statiques.
   **(≠ cpp-auth)** cpp-auth ajoute `INCLUDES DESTINATION ...` : c'est redondant avec le
   `$<INSTALL_INTERFACE:...>` du §5.2, on l'enlève.
2. **`install(DIRECTORY include/ ...)`** : copie les en-têtes. Le `/` final de `include/`
   compte : il copie le **contenu** du dossier (`tmpl/Version.hpp` arrive dans
   `<préfixe>/include/tmpl/`). Sans `/`, on obtiendrait `<préfixe>/include/include/tmpl/`.
3. **`install(EXPORT)`** : génère et installe `Targets.cmake` (§1.3).
4. **`configure_package_config_file`** : génère `Config.cmake` depuis le modèle (§1.5).
   Le résultat va dans `CMAKE_CURRENT_BINARY_DIR` (le dossier de build de la lib) :
   **un fichier généré ne s'écrit jamais dans les sources**. `INSTALL_DESTINATION` lui dit
   où il sera installé, pour que `@PACKAGE_INIT@` calcule les bons chemins relatifs.
5. **`write_basic_package_version_file`** : génère `ConfigVersion.cmake` (§1.6).
   **(≠ cpp-auth)** Sans `VERSION` : il prend `PROJECT_VERSION` par défaut.
6. **`install(FILES)`** : les deux fichiers générés ne sont pas des cibles, il faut les
   installer explicitement, dans le même dossier que `Targets.cmake`.

### 5.4 Les tests

```cmake
# --- Tests -------------------------------------------------------------------

if(CPP_TEMPLATE_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(cpp-template_tests
        tests/VersionTests.cpp
    )
    target_link_libraries(cpp-template_tests PRIVATE
        cpp-template::cpp-template
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(cpp-template_tests)
endif()
```

- `find_package(GTest ...)` **dans** le `if` : quand les tests sont désactivés, la lib n'a
  même pas besoin de GoogleTest. C'est aussi pourquoi `GTest` n'apparaît **pas** dans le
  `Config.cmake.in` : ce n'est pas une dépendance de la lib, seulement de ses tests.
- **(≠ cpp-auth)** Les tests lient `cpp-template::cpp-template` (l'alias) plutôt que
  `cpp-template`. Une faute de frappe dans l'alias se voit donc dès les tests.
- `GTest::gtest_main` : fournit un `main()` qui lance tous les `TEST`.
- `gtest_discover_tests` : lit la liste des tests dans l'exécutable et déclare chacun
  auprès de CTest. C'est grâce à ça que `ctest` compte des tests individuels et pas un
  seul gros exécutable.

---

## 6. Étape E : `cmake/cpp-templateConfig.cmake.in`

Crée `_template/cmake/cpp-templateConfig.cmake.in` :

```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
# Une ligne find_dependency(...) par dépendance de la lib, PUBLIC comme PRIVATE :
# la lib est statique, le projet qui l'utilise devra aussi lier ses dépendances.

include("${CMAKE_CURRENT_LIST_DIR}/cpp-templateTargets.cmake")
check_required_components(cpp-template)
```

Ligne par ligne :

- `@PACKAGE_INIT@` : §1.5. Doit rester **la première ligne**.
- `include(CMakeFindDependencyMacro)` : rend `find_dependency` disponible. Inutile tant
  qu'il n'y a pas de dépendance, mais on le garde : chaque lib réelle en aura besoin, et le
  commentaire juste dessous dit quoi ajouter.
- **L'ordre compte** : les dépendances d'abord, `Targets.cmake` ensuite. `Targets.cmake`
  fait référence à des cibles comme `cpp-core::cpp-core` : elles doivent déjà exister.
- `CMAKE_CURRENT_LIST_DIR` : le dossier du fichier en cours de lecture, donc
  `<préfixe>/lib/cmake/cpp-template/`. `Targets.cmake` est juste à côté.
- `check_required_components` : vérifie les *composants* demandés
  (`find_package(X COMPONENTS a b)`). On n'en a pas, mais c'est la fin standard d'un
  `Config.cmake`. Sans cette ligne, une demande de composant inexistant passerait en
  silence.

**Pourquoi pas de `REQUIRED` dans les `find_dependency`** (cpp-auth en met) : il est déjà
implicite. `find_dependency` transmet le `REQUIRED` (ou non) du `find_package` de
l'utilisateur.

---

## 7. Étape F : le package-consumer

C'est le test de la case « installable » : un **projet séparé**, qui ne connaît pas le
monorepo, utilise la lib installée exactement comme le ferait quelqu'un d'autre.

`_template/tests/package-consumer/CMakeLists.txt` :

```cmake
cmake_minimum_required(VERSION 3.21)
project(cpp-template_package_consumer LANGUAGES CXX)

find_package(cpp-template 0.1 CONFIG REQUIRED)

add_executable(package_consumer main.cpp)
target_link_libraries(package_consumer PRIVATE cpp-template::cpp-template)
```

- Un `project()` à lui : c'est un projet principal indépendant, pas un sous-dossier de la
  lib. Il n'est **jamais** ajouté avec `add_subdirectory`.
- `find_package(cpp-template 0.1 ...)` : **(≠ cpp-auth)** on demande une version. Ça teste
  en même temps que `ConfigVersion.cmake` est installé et correct (§1.6).
- Aucun chemin vers les sources de la lib : tout vient du préfixe donné par
  `CMAKE_PREFIX_PATH`.

`_template/tests/package-consumer/main.cpp` :

```cpp
#include <tmpl/Version.hpp>

int main()
{
    return tmpl::version().empty() ? 1 : 0;
}
```

Un programme qui **utilise vraiment** la lib : il inclut un en-tête installé et appelle une
fonction du `.a`. Si l'un des deux manque, ça casse à la compilation ou à l'édition des
liens. Le code de retour 0 signifie que tout va bien.

---

## 8. Étape G : `vcpkg.json`, `README.md`, `CHANGELOG.md`

### 8.1 `_template/vcpkg.json`

```json
{
    "name": "cpp-template",
    "version-string": "0.1.0",
    "dependencies": [
        "gtest"
    ]
}
```

Rappel du guide 01 (§1.7) : dans le monorepo, vcpkg ne lit **que** `backend/vcpkg.json`.
Ce fichier sert quand la lib est utilisée **hors** du monorepo : il dit ce qu'il faut
installer pour la compiler seule. Quand une lib aura une dépendance (cpp-pg : `libpq`), tu
l'ajouteras **ici et** dans le manifeste racine (étape I du guide 01).

### 8.2 `_template/README.md`

C'est un **squelette à remplir** pour chaque lib : les sections exigées par la définition
de terminé (but, exemple minimal, garanties, limites), avec des consignes à la place du
contenu.

````markdown
# cpp-template

> Une phrase : à quoi sert cette bibliothèque.

## Exemple minimal

```cpp
#include <tmpl/Version.hpp>

int main()
{
    return tmpl::version().empty() ? 1 : 0;
}
```

## Utilisation

Dans le monorepo (`backend/libs/CMakeLists.txt`) :

```cmake
add_subdirectory(cpp-template)
```

Depuis un autre projet, une fois installée :

```cmake
find_package(cpp-template 0.1 CONFIG REQUIRED)
target_link_libraries(mon_app PRIVATE cpp-template::cpp-template)
```

Option : `CPP_TEMPLATE_BUILD_TESTS` (ON quand la lib est le projet principal, OFF sinon).

## Garanties

- Ce que la bibliothèque promet (thread-safety, exceptions, complexité…).

## Limites

- Ce qu'elle ne fait pas, volontairement.
````

### 8.3 `_template/CHANGELOG.md`

```markdown
# Changelog

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions : [SemVer](https://semver.org/lang/fr/).

## [Unreleased]

## [0.1.0]

### Added
- Première version.
```

**Comment s'en servir** : chaque changement visible va sous `[Unreleased]`, dans une
rubrique `Added`, `Changed`, `Fixed` ou `Removed`. Au moment de sortir une version, tu
renommes `[Unreleased]` en `[0.2.0]`, tu changes la version dans `project()` et
`vcpkg.json`, et tu recrées un `[Unreleased]` vide.

---

## 9. Étape H : vérifier le gabarit

Le gabarit n'est pas compilé par `make test` (§1.9). On le vérifie donc comme à l'étape J
du guide 01, seul puis installé. Tout se passe **dans le conteneur** (c'est lui qui a
cmake, le compilateur et les paquets vcpkg).

```sh
make up          # terminal 1, si ce n'est pas déjà lancé
make shell       # terminal 2 : tu arrives dans /app, c'est-à-dire backend/
```

Dans le conteneur, prépare les options vcpkg dans une variable, pour ne pas les retaper :

```sh
T="-DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=$VCPKG_INSTALLED_DIR -DVCPKG_MANIFEST_INSTALL=OFF"
```

On construit dans `/tmp/...` : c'est jetable, et ça ne touche ni `build/` (le cache du
backend) ni les sources.

### 9.1 Le gabarit seul

```sh
cmake -S libs/_template -B /tmp/tpl $T
grep CPP_TEMPLATE_BUILD_TESTS /tmp/tpl/CMakeCache.txt
cmake --build /tmp/tpl -j"$(nproc)"
ctest --test-dir /tmp/tpl
```

✅ **Attendu** :

```
CPP_TEMPLATE_BUILD_TESTS:BOOL=ON
...
100% tests passed, 0 tests failed out of 1
```

`ON`, car ici le gabarit est le projet principal (`PROJECT_IS_TOP_LEVEL`).

### 9.2 Installé, puis utilisé par le package-consumer

```sh
cmake --install /tmp/tpl --prefix /tmp/inst
find /tmp/inst -type f | sort
```

✅ **Attendu** : exactement ces 6 fichiers (§1.1, §1.2).

```
/tmp/inst/include/tmpl/Version.hpp
/tmp/inst/lib/cmake/cpp-template/cpp-templateConfig.cmake
/tmp/inst/lib/cmake/cpp-template/cpp-templateConfigVersion.cmake
/tmp/inst/lib/cmake/cpp-template/cpp-templateTargets-noconfig.cmake
/tmp/inst/lib/cmake/cpp-template/cpp-templateTargets.cmake
/tmp/inst/lib/libcpp-template.a
```

Puis le consommateur :

```sh
cmake -S libs/_template/tests/package-consumer -B /tmp/cons $T -DCMAKE_PREFIX_PATH=/tmp/inst
cmake --build /tmp/cons
/tmp/cons/package_consumer && echo OK
```

✅ **Attendu** : `OK`.

`-DCMAKE_PREFIX_PATH=/tmp/inst` dit à `find_package` : « cherche aussi dans ce préfixe ».
C'est le seul lien entre le consommateur et la lib.

**Curiosité, pour bien comprendre** : ouvre `/tmp/inst/lib/cmake/cpp-template/cpp-templateTargets.cmake`
et cherche `INTERFACE_INCLUDE_DIRECTORIES`. Tu y verras `${_IMPORT_PREFIX}/include` :
c'est ton `$<INSTALL_INTERFACE:include>`, devenu un chemin relatif au préfixe.

### 9.3 Casser exprès : la version

Dans `libs/_template/tests/package-consumer/CMakeLists.txt`, remplace temporairement
`0.1` par `1.0`, puis :

```sh
rm -rf /tmp/cons
cmake -S libs/_template/tests/package-consumer -B /tmp/cons $T -DCMAKE_PREFIX_PATH=/tmp/inst
```

✅ **Attendu** : une erreur.

```
Could not find a configuration file for package "cpp-template" that is
compatible with requested version "1.0".
```

C'est `ConfigVersion.cmake` qui a refusé (§1.6). **Remets `0.1`** avant de continuer.

**Pourquoi casser exprès** : un test qui n'a jamais échoué ne prouve rien. Là, tu as vu que
la vérification de version fonctionne vraiment.

Si une étape ne donne pas le résultat attendu, ne passe pas à la suite. Relis le bloc
concerné de l'étape D ou E : l'erreur de CMake indique le fichier et la ligne.

---

## 10. Étape I : le script `new-lib.sh`

Le gabarit marche. Pour créer cpp-core, il faudrait maintenant :

1. copier `_template/` en `cpp-core/` ;
2. renommer `cmake/cpp-templateConfig.cmake.in` et `include/tmpl/` ;
3. remplacer les trois formes du nom (§1.8) dans **tous** les fichiers.

C'est mécanique et facile à rater (un `tmpl` oublié compile… avec le mauvais namespace).
On l'écrit une fois dans un script. Crée `backend/libs/new-lib.sh` :

```sh
#!/bin/sh
# Crée une bibliothèque à partir de libs/_template.
# Usage : libs/new-lib.sh <nom> <namespace>     ex. : libs/new-lib.sh cpp-core core
set -eu

name=${1:?usage: $0 <nom> <namespace>}
ns=${2:?usage: $0 <nom> <namespace>}

case $name in cpp-*) ;; *) echo "le nom doit commencer par cpp-" >&2; exit 1 ;; esac
macro=$(printf '%s' "$name" | tr 'a-z-' 'A-Z_')        # cpp-core -> CPP_CORE

libs=$(dirname "$0")
dest=$libs/$name
[ ! -e "$dest" ] || { echo "$dest existe déjà" >&2; exit 1; }

cp -r "$libs/_template" "$dest"
mv "$dest/cmake/cpp-templateConfig.cmake.in" "$dest/cmake/${name}Config.cmake.in"
mv "$dest/include/tmpl" "$dest/include/$ns"

find "$dest" -type f -exec sed -i \
    -e "s/cpp-template/$name/g" \
    -e "s/CPP_TEMPLATE/$macro/g" \
    -e "s/tmpl/$ns/g" \
    {} +

echo "Créé : $dest"
echo "Reste à faire : ajouter add_subdirectory($name) dans $libs/CMakeLists.txt"
```

Ligne par ligne :

- `#!/bin/sh` : `sh` et pas `bash`, comme les scripts de `backend/dev/` : il marche aussi
  dans le conteneur et n'utilise rien de spécifique à bash.
- `set -eu` : `-e` arrête le script à la première commande qui échoue (sinon, un `cp` raté
  serait suivi de `sed` sur des fichiers inexistants) ; `-u` traite une variable non
  définie comme une erreur, au lieu de la remplacer par du vide.
- `${1:?message}` : le premier argument, ou arrêt avec ce message s'il manque.
- `case $name in cpp-*) ;; *) ... esac` : le `case` de shell. Si le nom commence par
  `cpp-`, on ne fait rien (`;;`), sinon on sort en erreur. Convention de nommage du
  planning, imposée par la machine plutôt que par la mémoire.
- `tr 'a-z-' 'A-Z_'` : remplace chaque caractère de la 1re liste par celui à la même place
  dans la 2e : minuscules → majuscules, `-` → `_`. `cpp-core` devient `CPP_CORE` (§1.8).
- `dirname "$0"` : `$0` est le chemin du script tel que tu l'as lancé
  (`libs/new-lib.sh`), `dirname` en garde le dossier (`libs`). Le script retrouve ainsi
  `_template` à côté de lui, d'où que tu le lances.
- `[ ! -e "$dest" ] || { ...; exit 1; }` : « la destination n'existe pas, **sinon**
  erreur ». Protège une lib existante : sans ça, `cp -r` écrirait **dedans** et `sed`
  réécrirait ses fichiers.
- `mv` × 2 : les deux seuls **noms de fichiers ou dossiers** qui contiennent le nom.
- `find "$dest" -type f -exec sed -i ... {} +` : pour chaque fichier (`-type f`) de la
  nouvelle lib, lance `sed -i` (modifie le fichier sur place) avec trois remplacements.
  `s/a/b/g` remplace toutes les occurrences (`g`) de `a` par `b` sur chaque ligne.
  `{} +` passe tous les fichiers à un seul appel de `sed`.
- Les guillemets **doubles** autour des `sed` : le shell remplace `$name`, `$macro`, `$ns`
  par leur valeur avant de lancer `sed`.
- La dernière ligne te **rappelle** d'ajouter la lib dans `libs/CMakeLists.txt`. Le script
  ne le fait pas lui-même, volontairement (question 8, §13).

Rends-le exécutable :

```sh
chmod +x ~/matcha/backend/libs/new-lib.sh
```

Git enregistre ce bit « exécutable » : il sera conservé au commit.

---

## 11. Étape J : essayer le script, puis jeter le résultat

On crée une lib de démonstration, on vérifie qu'elle marche **comme une vraie**, puis on
la supprime. La vraie première lib (cpp-core) viendra avec le guide suivant.

### 11.1 Générer `cpp-demo`

Toujours dans le conteneur (`make shell`, dossier `/app`) :

```sh
sh libs/new-lib.sh cpp-demo demo
find libs/cpp-demo -type f | sort
grep -rn -e tmpl -e template -e TEMPLATE libs/cpp-demo || echo "aucun reste du gabarit"
```

✅ **Attendu** :

```
Créé : libs/cpp-demo
Reste à faire : ajouter add_subdirectory(cpp-demo) dans libs/CMakeLists.txt
libs/cpp-demo/CHANGELOG.md
libs/cpp-demo/CMakeLists.txt
libs/cpp-demo/README.md
libs/cpp-demo/cmake/cpp-demoConfig.cmake.in
libs/cpp-demo/include/demo/Version.hpp
libs/cpp-demo/src/Version.cpp
libs/cpp-demo/tests/VersionTests.cpp
libs/cpp-demo/tests/package-consumer/CMakeLists.txt
libs/cpp-demo/tests/package-consumer/main.cpp
libs/cpp-demo/vcpkg.json
aucun reste du gabarit
```

`grep -rn` cherche récursivement (`-r`) en affichant les numéros de ligne (`-n`). Il ne doit
**rien** trouver. `|| echo` ne s'exécute que si `grep` n'a rien trouvé.

Ouvre aussi `libs/cpp-demo/CMakeLists.txt` dans VS Code : tu dois y lire `cpp-demo`,
`CPP_DEMO_BUILD_TESTS`, `CPP_DEMO_VERSION`.

### 11.2 `cpp-demo` dans le monorepo

Ajoute temporairement dans `backend/libs/CMakeLists.txt`, **sur sa propre ligne** après
`add_subdirectory(cpp-auth)` :

```cmake
add_subdirectory(cpp-demo)
```

⚠️ Vérifie que `add_subdirectory(cpp-auth)` est bien suivi d'un retour à la ligne. Ton
fichier actuel n'en a pas à la fin : si tu ajoutes la ligne avec `echo ... >>`, tu obtiens
`add_subdirectory(cpp-auth)add_subdirectory(cpp-demo)` et CMake répond
`Parse error. Expected a newline`. Dans VS Code, il suffit d'appuyer sur Entrée.

Puis, dans le conteneur, dans un dossier de build **à part** (pour ne pas polluer le
cache de `build/`, guide 01 étape G) :

```sh
cmake -S . -B /tmp/mono $T -DCPP_DEMO_BUILD_TESTS=ON
grep -E 'CPP_(DEMO|AUTH)_BUILD_TESTS' /tmp/mono/CMakeCache.txt
cmake --build /tmp/mono --target cpp-demo_tests -j"$(nproc)"
ctest --test-dir /tmp/mono -R Version
```

✅ **Attendu** :

```
CPP_AUTH_BUILD_TESTS:BOOL=OFF
CPP_DEMO_BUILD_TESTS:BOOL=ON
...
100% tests passed, 0 tests failed out of 1
```

- `CPP_AUTH_BUILD_TESTS=OFF` : cpp-auth est un sous-projet et on n'a rien demandé.
- `CPP_DEMO_BUILD_TESTS=ON` : on l'a demandé avec `-D`. La lib générée réagit à **sa
  propre** option : le renommage `CPP_TEMPLATE` → `CPP_DEMO` a fonctionné.
- `--target cpp-demo_tests` : compile seulement l'exécutable de test de cpp-demo, pas tout
  le backend (plus rapide).
- `-R Version` : ne lance que les tests dont le nom contient `Version`.

### 11.3 Tout jeter

`cpp-demo` n'a jamais été commité, et la ligne ajoutée doit disparaître :

```sh
exit                                         # sortir du conteneur
cd ~/matcha
git restore backend/libs/CMakeLists.txt     # annule la ligne add_subdirectory(cpp-demo)
rm -rf backend/libs/cpp-demo
git status
```

✅ **Attendu** : seuls `backend/libs/_template/` et `backend/libs/new-lib.sh` apparaissent
en `Untracked files`.

`git restore <fichier>` remet le fichier dans l'état du dernier commit. Ici c'est ce qu'on
veut, mais attention : **ça efface tes modifications non commitées de ce fichier**, sans
corbeille.

---

## 12. Étape K : commiter

```sh
git add backend/libs/_template backend/libs/new-lib.sh
git status
```

✅ **Attendu** : sous `Changes to be committed`, 11 nouveaux fichiers (10 dans
`_template/`, plus `new-lib.sh`), et rien d'autre.

```sh
git commit -m "libs : gabarit _template et script new-lib.sh"
git log --oneline -3
```

---

## 13. Questions

Réponds sans regarder, puis relis la section indiquée si tu bloques. Écris sous chaque
**Ta réponse**, à la place de la ligne `…`. Quand tu as fini, préviens-moi : je remplis la
§14.

### Q1. Le préfixe d'installation (§1.1, §9.2)

Après `cmake --install`, `/tmp/inst` contient 6 fichiers. Classe-les en trois groupes et
dis à quoi sert chaque groupe.

**Ta réponse :**

…

### Q2. Les expressions génératrices (§1.4)

Pourquoi le chemin d'include est-il écrit deux fois, avec `$<BUILD_INTERFACE:...>` et
`$<INSTALL_INTERFACE:...>` ? Que se passerait-il si on écrivait seulement
`${CMAKE_CURRENT_SOURCE_DIR}/include` ?

**Ta réponse :**

…

### Q3. `find_dependency` (§1.5)

Demain, cpp-pg aura `target_link_libraries(cpp-pg PUBLIC cpp-core::cpp-core PRIVATE PostgreSQL::PostgreSQL)`.
Quelles lignes `find_dependency` faut-il dans `cmake/cpp-pgConfig.cmake.in`, et pourquoi
la réponse surprend-elle pour `PostgreSQL` ?

**Ta réponse :**

…

### Q4. La version (§1.6, §9.3)

La lib installée est en `0.1.0`. Pour chacune de ces demandes, accepté ou refusé ?
`find_package(cpp-template 0.1 ...)`, `find_package(cpp-template 0.2 ...)`,
`find_package(cpp-template 1.0 ...)`. Et quel fichier prend la décision ?

**Ta réponse :**

…

### Q5. `PRIVATE` pour la macro de version (§5.2)

Pourquoi `CPP_TEMPLATE_VERSION` est-il passé par `target_compile_definitions` plutôt
qu'écrit en dur dans `Version.cpp` ? Et pourquoi `PRIVATE` et pas `PUBLIC` ?

**Ta réponse :**

…

### Q6. `GTest` absent de `Config.cmake.in` (§5.4, §6)

La lib utilise GoogleTest pour ses tests. Pourquoi ne faut-il **pas** de
`find_dependency(GTest)` dans `Config.cmake.in` ?

**Ta réponse :**

…

### Q7. Le gabarit hors du monorepo (§1.8, §1.9)

Pourquoi le namespace s'appelle-t-il `tmpl` et pas `template` ? Et puisque `_template`
n'est pas dans `libs/CMakeLists.txt`, qu'est-ce que ça implique quand tu modifies le
gabarit ?

**Ta réponse :**

…

### Q8. Ce que le script ne fait pas (§10, et §6.1 du guide 01)

`new-lib.sh` pourrait ajouter lui-même `add_subdirectory(<nom>)` à la fin de
`libs/CMakeLists.txt`. Pourquoi est-ce une mauvaise idée ?

**Ta réponse :**

…

---

## 14. Correction

> Remplie après tes réponses.
>
> **Légende** : 🟢 juste et complet · 🟡 juste mais incomplet ou imprécis ·
> 🔴 faux ou à côté · ⏳ pas encore corrigé

| Question | Évaluation |
|---|---|
| Q1. Le préfixe d'installation | ⏳ |
| Q2. Les expressions génératrices | ⏳ |
| Q3. `find_dependency` | ⏳ |
| Q4. La version | ⏳ |
| Q5. `PRIVATE` pour la macro de version | ⏳ |
| Q6. `GTest` absent de `Config.cmake.in` | ⏳ |
| Q7. Le gabarit hors du monorepo | ⏳ |
| Q8. Ce que le script ne fait pas | ⏳ |

### Q1 — ⏳

*Réponse attendue et commentaire : à venir.*

### Q2 — ⏳

*Réponse attendue et commentaire : à venir.*

### Q3 — ⏳

*Réponse attendue et commentaire : à venir.*

### Q4 — ⏳

*Réponse attendue et commentaire : à venir.*

### Q5 — ⏳

*Réponse attendue et commentaire : à venir.*

### Q6 — ⏳

*Réponse attendue et commentaire : à venir.*

### Q7 — ⏳

*Réponse attendue et commentaire : à venir.*

### Q8 — ⏳

*Réponse attendue et commentaire : à venir.*
