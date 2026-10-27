# Guide 03 : warnings en erreur, presets CMake et sanitizers

> Planning : lundi, « Monorepo › Drapeaux et presets communs » (corrige l'audit 4.19).
> Durée : environ 1 h 30, lecture comprise.
>
> Même principe que les guides 01 et 02 : je te dis **exactement** quoi faire et
> **pourquoi**, c'est toi qui tapes. Tout a été vérifié sur une copie du projet, dans
> l'image `matcha_backend:dev` (GCC 12.2, CMake 3.25), y compris le build de prod.
>
> Questions de fin au §10, avec une zone de réponse. Préviens-moi quand tu as fini et je
> remplis la correction (§11).

## Sommaire

0. [Ce qu'on fait, et le résultat attendu](#0-ce-quon-fait-et-le-résultat-attendu)
1. [Les notions à connaître avant de commencer](#1-les-notions-à-connaître-avant-de-commencer)
2. [Étape A : partir d'un dépôt propre](#2-étape-a--partir-dun-dépôt-propre)
3. [Étape B : `CMakePresets.json`](#3-étape-b--cmakepresetsjson)
4. [Étape C : `cmake/Warnings.cmake`, et le premier warning](#4-étape-c--cmakewarningscmake-et-le-premier-warning)
5. [Étape D : le preset `release`, et un faux positif](#5-étape-d--le-preset-release-et-un-faux-positif)
6. [Étape E : le preset `sanitize`, et trois bugs exprès](#6-étape-e--le-preset-sanitize-et-trois-bugs-exprès)
7. [Étape F : brancher les presets dans Docker et le Makefile](#7-étape-f--brancher-les-presets-dans-docker-et-le-makefile)
8. [Étape G : vérifier tout le parcours](#8-étape-g--vérifier-tout-le-parcours)
9. [Étape H : commiter](#9-étape-h--commiter)
10. [Questions](#10-questions)
11. [Correction](#11-correction)

---

## 0. Ce qu'on fait, et le résultat attendu

La définition de terminé du planning exige, pour chaque bibliothèque :

> Tests GoogleTest verts sous le preset `sanitize` (ASan + UBSan), zéro warning en `-Werror`.

Aujourd'hui, rien de tout ça n'existe (audit 4.19) : le compilateur n'affiche presque aucun
warning parce qu'on ne lui en demande pas, et il n'y a aucune configuration avec
*sanitizers*. Le sujet de Matcha exige pourtant « aucune erreur ni warning » : sans
`-Wall -Wextra`, on ne peut pas le prouver.

Autre problème : les options de CMake sont recopiées à trois endroits (`matcha-build`, le
`Dockerfile`, et ta variable `$T` des guides 01 et 02). Les **presets** les rassemblent
dans un seul fichier.

```
backend/
├── CMakePresets.json        ← nouveau : debug, release, sanitize
├── CMakeLists.txt           ← +1 ligne : include(cmake/Warnings.cmake)
├── cmake/
│   └── Warnings.cmake       ← nouveau : -Wall -Wextra -Wpedantic
├── Dockerfile               ← le build de prod utilise le preset release
├── dev/
│   ├── matcha-build         ← utilise le preset debug
│   └── matcha-dev-server    ← le binaire est dans build/debug/
└── libs/cpp-auth/           ← 1 vrai warning corrigé (JsonJwksParser)
Makefile                     ← `make test` via le preset, nouveau `make sanitize`
```

À la fin :

| Vérification | Résultat attendu |
|---|---|
| `cmake --build --preset debug` | 0 warning, 174 tests passent |
| `cmake --build --preset release` | 0 warning |
| 3 bugs volontaires | passent en `debug`, **échouent** en `sanitize` |
| `make sanitize` | 174 tests passent sous ASan + UBSan |
| build de l'image de prod | compile avec le preset `release` |

---

## 1. Les notions à connaître avant de commencer

### 1.1 Les warnings : `-Wall`, `-Wextra`, `-Wpedantic`

Un **warning** est un message du compilateur qui dit : « c'est du C++ valide, mais c'est
probablement une erreur ». Par défaut, GCC n'en affiche que très peu. On les active par
familles :

| Option | Ce qu'elle active | Exemples |
|---|---|---|
| `-Wall` | les warnings presque toujours justes. **Pas « tous »**, malgré le nom | variable inutilisée, fonction déclarée mais jamais utilisée, `if (x = 0)` |
| `-Wextra` | une deuxième série, un peu plus bavarde | paramètre inutilisé, comparaison `signed`/`unsigned` |
| `-Wpedantic` | tout ce qui sort du **standard** C++ (extensions de GCC) | tableau de taille variable `int t[n];`, `;` en trop après une fonction |

Ce sont les trois du planning. Il en existe d'autres (`-Wshadow`, `-Wconversion`…), plus
bruyants : on pourra les ajouter un par un plus tard, ils iront dans le même fichier.

### 1.2 `-Werror` et `CMAKE_COMPILE_WARNING_AS_ERROR`

`-Werror` transforme chaque warning en **erreur** : le fichier ne compile pas. Sans lui, un
warning défile au milieu de 100 lignes de compilation et personne ne le lit. Avec lui, la
règle « zéro warning » est **vérifiée par la machine**.

On ne l'écrit pas à la main. Depuis CMake 3.24, la variable
`CMAKE_COMPILE_WARNING_AS_ERROR=ON` ajoute `-Werror` (ou l'équivalent du compilateur) à
toutes les cibles. Deux avantages :

- on la met dans les **presets** (§1.7) : c'est un choix de **notre** environnement de
  build, pas une propriété des libs ;
- en cas d'urgence, on la coupe sans toucher aux fichiers :
  `cmake --build --preset debug --compile-no-warning-as-error`.

**Pourquoi ne jamais mettre `-Werror` dans le `CMakeLists.txt` d'une lib** : la lib
s'installe chez d'autres (guide 02). Le jour où leur compilateur, plus récent, invente un
nouveau warning, leur build casserait à cause de **ton** choix.

### 1.3 `add_compile_options` : une option de **dossier**

```cmake
add_compile_options(-Wall -Wextra -Wpedantic)
```

Cette commande ajoute ces options à toutes les cibles **créées après elle**, dans ce
dossier **et ses sous-dossiers** (`add_subdirectory`). Deux conséquences :

- **l'ordre compte** : il faut l'appeler **avant** `add_subdirectory(libs)` et avant les
  `add_executable`. Une cible déjà créée ne la reçoit pas ;
- cpp-auth et les futures libs sont vérifiées dans le monorepo **sans rien changer** dans
  leur `CMakeLists.txt`. Et ces options ne partent **pas** dans le paquet installé : ce ne
  sont pas des propriétés `INTERFACE` (guide 02, §1.3). Une lib reste indépendante.

L'autre manière serait une fonction `activer_warnings(cible)` appelée sur chaque cible.
Mais les libs devraient alors connaître une fonction du backend, ce qui casserait leur
indépendance.

### 1.4 Les en-têtes « système » : pourquoi Crow ne déclenche rien

Le backend inclut Crow, jwt-cpp, nlohmann_json… Leur code n'est pas à nous, et on ne veut
pas de leurs warnings.

Les cibles **importées** par `find_package` (comme `Crow::Crow`) ajoutent leurs dossiers
d'en-têtes avec `-isystem` au lieu de `-I`. Tu peux le voir dans
`build/debug/libs/cpp-auth/CMakeFiles/cpp-auth.dir/flags.make` :

```
CXX_INCLUDES = -I/app/libs/cpp-auth/include -isystem /opt/vcpkg_installed/x64-linux/include
```

GCC n'affiche **aucun** warning pour un fichier trouvé via `-isystem`. Seul notre code est
vérifié. C'est pour ça que tout le backend ne donne qu'**un** warning (§4). Tu verras au §5
la seule exception.

### 1.5 Les types de build : `Debug` et `Release`

`CMAKE_BUILD_TYPE` choisit des options d'optimisation :

| Type | Options ajoutées | Pour |
|---|---|---|
| `Debug` | `-g` (infos de débogage, pas d'optimisation) | développer, déboguer avec `gdb` |
| `Release` | `-O3 -DNDEBUG` (optimisation maximale, `assert` désactivés) | la prod |

Retiens une conséquence pour le §5 : **certains warnings n'existent qu'en Release**. Ils
viennent de l'analyse que fait l'optimiseur (par exemple « cette variable peut être lue
avant d'être initialisée »), et cette analyse ne tourne pas en `-O0`.

### 1.6 Les *sanitizers* : ASan, UBSan, LSan

Un *sanitizer* est un détecteur de bugs **ajouté au programme par le compilateur**. Le
programme vérifie lui-même, pendant qu'il tourne, ce qu'il fait de sa mémoire.

| Sanitizer | Option | Détecte |
|---|---|---|
| **ASan** (*AddressSanitizer*) | `-fsanitize=address` | débordement de tableau, utilisation après `delete`, double `delete` |
| **LSan** (*LeakSanitizer*) | inclus dans ASan | fuites mémoire (`new` sans `delete`), rapportées à la fin du programme |
| **UBSan** (*UndefinedBehaviorSanitizer*) | `-fsanitize=undefined` | comportements indéfinis : dépassement d'un `int`, décalage de bits trop grand, pointeur nul déréférencé… |

**Pourquoi c'est indispensable en C++** : écrire une case après la fin d'un tableau ne
plante **pas** forcément. Le programme continue, avec une mémoire corrompue, et le test
passe. Tu vas le constater au §6 : trois bugs qui passent en `debug`, attrapés en
`sanitize`.

Ce que ça coûte et les règles à connaître :

- le programme est environ **2 fois plus lent** et consomme plus de mémoire : on ne s'en
  sert que pour les tests, jamais en prod ;
- l'option se passe à la **compilation** (pour instrumenter le code) **et** à l'**édition
  de liens** (pour ajouter la bibliothèque d'exécution d'ASan) ;
- `-fno-sanitize-recover=all` : par défaut, UBSan **affiche** l'erreur puis **continue**,
  et le test passe quand même. Avec cette option, il arrête le programme : le test échoue ;
- `-fno-omit-frame-pointer` : garde des informations qui rendent les piles d'appels
  d'ASan lisibles ;
- ASan n'est compatible ni avec **valgrind** (installé dans l'image), ni avec **TSan**
  (le sanitizer des *data races*). On ne les combine jamais.

Les libs de vcpkg ne sont pas instrumentées (elles sont déjà compilées). Un bug **à
l'intérieur** de Crow ne serait pas vu, mais tout notre code, lui, est vérifié.

### 1.7 `CMakePresets.json`

Jusqu'ici, tu tapais :

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_INSTALLED_DIR=$VCPKG_INSTALLED_DIR -DVCPKG_MANIFEST_INSTALL=OFF \
    -DCPP_AUTH_BUILD_TESTS=ON
```

Un **preset** donne un nom à ce paquet d'options, dans un fichier JSON à côté du
`CMakeLists.txt`. La commande devient `cmake --preset debug`. Il y a trois sortes de
presets, une par commande :

| Sorte | Commande | Remplace |
|---|---|---|
| `configurePresets` | `cmake --preset debug` | `cmake -S . -B build -D...` |
| `buildPresets` | `cmake --build --preset debug` | `cmake --build build` |
| `testPresets` | `ctest --preset debug` | `ctest --test-dir build --output-on-failure` |

Ce qu'il faut connaître dans le fichier :

- `"hidden": true` : un preset qu'on ne peut pas lancer, seulement **hériter** (une base
  commune) ;
- `"inherits": "base"` : reprend tout ce que définit `base`, puis ajoute ou remplace ;
- `${sourceDir}` : le dossier du `CMakePresets.json` ; `${presetName}` : le nom du preset ;
- `$env{VAR}` : une variable d'environnement. L'image définit `VCPKG_ROOT=/opt/vcpkg` et
  `VCPKG_INSTALLED_DIR=/opt/vcpkg_installed` (voir le `Dockerfile`) ;
- `binaryDir` : le dossier de build. On en prend **un par preset** (`build/debug`,
  `build/release`, `build/sanitize`), pour qu'ils ne s'écrasent pas : le cache d'un
  dossier de build garde ses options (guide 01, étape G).

`CMakePresets.json` est **commité** : il décrit le projet. Si un jour tu veux un réglage
personnel, il existe aussi `CMakeUserPresets.json`, prévu pour **ne pas** être commité.

> 💡 Les presets remplacent `$T` pour le **backend**. Pour compiler une lib **seule**
> (étape H du guide 02), `$T` reste utile : chaque lib n'a pas ses propres presets.

---

## 2. Étape A : partir d'un dépôt propre

Depuis `~/matcha` :

```sh
git status
```

✅ **Attendu** : seulement les fichiers de documentation que j'ai ajoutés ou modifiés.

```
modified:   docs/README.md
modified:   docs/guides/02-libs-template.md
Untracked files:
        docs/guides/00-plan.md
        docs/guides/03-presets-warnings.md
        docs/guides/04-cpp-core.md
```

(Dans le guide 02, j'ai seulement corrigé une phrase : la lib suivante n'est pas cpp-core
mais ce guide-ci. Le guide 04 et le plan des guides suivants sont déjà prêts.)

Commite-les à part :

```sh
git add docs/README.md docs/guides/
git status                 # vérifie : seulement ces 5 fichiers
git commit -m "docs : guides 03 et 04, plan des guides suivants"
```

Puis une branche, à partir de `libs/template` où tu es :

```sh
git switch -c build/presets
```

Garde deux terminaux, comme d'habitude :

```sh
make up          # terminal 1, si ce n'est pas déjà lancé
make shell       # terminal 2 : tu arrives dans /app, c'est-à-dire backend/
```

Sauf mention contraire, les commandes `cmake` et `ctest` de ce guide se tapent **dans le
conteneur**, dans `/app`.

---

## 3. Étape B : `CMakePresets.json`

Crée `backend/CMakePresets.json` (à côté du `CMakeLists.txt` racine) :

```json
{
    "version": 6,
    "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
    "configurePresets": [
        {
            "name": "base",
            "hidden": true,
            "binaryDir": "${sourceDir}/build/${presetName}",
            "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
            "cacheVariables": {
                "VCPKG_INSTALLED_DIR": "$env{VCPKG_INSTALLED_DIR}",
                "VCPKG_MANIFEST_INSTALL": "OFF",
                "CMAKE_COMPILE_WARNING_AS_ERROR": "ON"
            }
        },
        {
            "name": "debug",
            "inherits": "base",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CPP_AUTH_BUILD_TESTS": "ON"
            }
        },
        {
            "name": "release",
            "inherits": "base",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release"
            }
        },
        {
            "name": "sanitize",
            "inherits": "debug",
            "cacheVariables": {
                "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer",
                "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined"
            }
        }
    ],
    "buildPresets": [
        { "name": "debug", "configurePreset": "debug" },
        { "name": "release", "configurePreset": "release" },
        { "name": "sanitize", "configurePreset": "sanitize" }
    ],
    "testPresets": [
        {
            "name": "debug",
            "configurePreset": "debug",
            "output": { "outputOnFailure": true }
        },
        {
            "name": "sanitize",
            "configurePreset": "sanitize",
            "output": { "outputOnFailure": true },
            "environment": { "UBSAN_OPTIONS": "print_stacktrace=1" }
        }
    ]
}
```

Bloc par bloc :

- `"version": 6` : le format du fichier. La version 6 est la plus récente que comprend
  CMake 3.25 (celui de l'image). `cmakeMinimumRequired` donne un message clair à qui
  aurait un CMake plus ancien.
- **`base`** (caché) : ce qui est commun à tous. C'est exactement ta variable `$T`
  (`toolchainFile` est la forme preset de `-DCMAKE_TOOLCHAIN_FILE=`), plus le `-Werror`
  (§1.2).
- **`debug`** : ce qu'utilisera `make test`. `CPP_AUTH_BUILD_TESTS=ON` vient de
  `matcha-build` : c'est le commentaire « à déplacer dans CMakePresets.json » que tu avais
  lu au guide 01, étape H.
- **`release`** : ce qu'utilisera l'image de prod. Pas de tests de cpp-auth.
- **`sanitize`** hérite de **`debug`**, pas de `base` : on garde `Debug` (les numéros de
  ligne des rapports d'ASan sont justes, rien n'est optimisé) et les tests de cpp-auth,
  et on ajoute les sanitizers. Les options sont dans `CMAKE_CXX_FLAGS` (compilation) **et**
  `CMAKE_EXE_LINKER_FLAGS` (édition de liens, §1.6).
- **`buildPresets`** et **`testPresets`** : chacun pointe vers son `configurePreset`, et
  retrouve donc tout seul le bon `binaryDir`.
- `outputOnFailure` : comme `--output-on-failure`, affiche la sortie d'un test qui échoue.
- `UBSAN_OPTIONS=print_stacktrace=1` : variable d'environnement posée pendant les tests.
  UBSan affiche alors la pile d'appels, pas seulement la ligne fautive.

Il n'y a pas de `testPresets` `release` : on teste en `debug` et en `sanitize`, et l'image
de prod ne contient pas les tests.

**Essaie**, dans le conteneur :

```sh
cmake --preset debug
```

✅ **Attendu** (début) :

```
Preset CMake variables:

  CMAKE_BUILD_TYPE="Debug"
  CMAKE_COMPILE_WARNING_AS_ERROR="ON"
  CMAKE_TOOLCHAIN_FILE:FILEPATH="/opt/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CPP_AUTH_BUILD_TESTS="ON"
  VCPKG_INSTALLED_DIR="/opt/vcpkg_installed"
  VCPKG_MANIFEST_INSTALL="OFF"
...
-- Build files have been written to: /app/build/debug
```

CMake affiche d'abord les variables qu'il a lues dans le preset : c'est le moyen de
vérifier qu'il a compris ton fichier. Puis :

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug
```

✅ **Attendu** : la compilation passe, puis `100% tests passed, 0 tests failed out of 174`.

`-j"$(nproc)"` : compile en parallèle sur tous les cœurs (`nproc` donne leur nombre).
Sans lui, `make` compile un fichier à la fois.

**Pourquoi tout passe malgré le `-Werror`** : aucun warning n'est encore activé (§1.1).
C'est l'étape suivante.

> 💡 `build/debug` est dans `/app/build`, donc dans le volume Docker `backend_build` :
> comme avant, les `.o` sont gardés et rien n'arrive dans ton dossier sur l'hôte.

---

## 4. Étape C : `cmake/Warnings.cmake`, et le premier warning

### 4.1 Le fichier

Crée `backend/cmake/Warnings.cmake` :

```cmake
# Avertissements du compilateur pour tout le code compilé dans ce dossier et
# ses sous-dossiers (src/, tests/, libs/). -Werror n'est pas ici : ce sont les
# presets qui l'activent, via CMAKE_COMPILE_WARNING_AS_ERROR.
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)
```

**Pourquoi un fichier à part pour trois lignes** : la liste des warnings est une décision
qui va évoluer (§1.1, et dès le §5). Elle a sa place à elle, avec ses commentaires, plutôt
qu'au milieu de la liste des sources du backend.

### 4.2 L'inclure, au bon endroit

Dans `backend/CMakeLists.txt`, ajoute **une ligne** juste après
`set(CMAKE_CXX_STANDARD_REQUIRED ON)` :

```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/Warnings.cmake)
```

- `include(...)` lit le fichier et exécute son contenu **à cet endroit**, comme si tu
  l'avais copié ici. Le chemin est relatif au dossier du `CMakeLists.txt`.
- **Pourquoi ici** : avant `add_subdirectory(libs)` et avant tous les `add_executable`
  (§1.3). Plus bas, une partie des cibles ne recevrait pas les options.

### 4.3 Compiler : la première erreur

```sh
cmake --build --preset debug -j"$(nproc)"
```

Pas besoin de relancer `cmake --preset debug` : le `CMakeLists.txt` a changé, et
`cmake --build` le détecte et reconfigure tout seul.

✅ **Attendu** : une **erreur**, plusieurs fois (une par fichier concerné) :

```
In file included from /app/libs/cpp-auth/tests/jwks/JwksParserTests.cpp:4:
/app/libs/cpp-auth/include/auth/jwks/JsonJwksParser.hpp:7:17: error: 'bool {anonymous}::isValidBase64Url(const std::string&)' defined but not used [-Werror=unused-function]
    7 |         bool    isValidBase64Url(const std::string &value)
      |                 ^~~~~~~~~~~~~~~~
cc1plus: all warnings being treated as errors
```

Lis le message de gauche à droite :

- `-Werror=unused-function` : c'est un warning (`-Wunused-function`, activé par `-Wall`)
  transformé en erreur par `-Werror` ;
- `defined but not used` : une fonction **définie** mais jamais appelée…
- `In file included from .../JwksParserTests.cpp` : … dans le fichier de test qui
  **inclut** cet en-tête.

C'est le **seul** warning de tout le backend et de cpp-auth : tout le reste du code passe
déjà `-Wall -Wextra -Wpedantic`.

### 4.4 Comprendre, puis corriger

Ouvre `backend/libs/cpp-auth/include/auth/jwks/JsonJwksParser.hpp` :

```cpp
namespace
{
	bool	isValidBase64Url(const std::string &value)
	{
		...
	}
}
```

Une fonction dans un **namespace anonyme** (`namespace { }`) n'est visible que dans le
fichier `.cpp` qui la contient. Mais ici, elle est dans un **en-tête**. Chaque `.cpp` qui
inclut `JsonJwksParser.hpp` reçoit donc **sa propre copie** de la fonction :

| Fichier qui inclut l'en-tête | Appelle `isValidBase64Url` ? | Résultat |
|---|---|---|
| `src/jwks/JsonJwksParser.cpp` | oui | OK |
| `tests/jwks/JwksParserTests.cpp` | non | copie inutile → warning |
| `tests/jwks/HttpJwksProviderTests.cpp` | non | copie inutile → warning |
| `backend/src/main.cpp` | non | copie inutile → warning |

Le warning signale donc un vrai défaut : un **détail d'implémentation** du parseur
traîne dans son **en-tête public**, et tout utilisateur de la lib le reçoit.

**La correction** : déplacer la fonction là où elle est utilisée.

1. Dans `JsonJwksParser.hpp`, **supprime** tout le bloc `namespace { ... }` (de la ligne
   `namespace` jusqu'à son `}` fermant). Il reste le `#pragma once`, l'`#include` et le
   `namespace auth { class JsonJwksParser ... }`.
2. Dans `backend/libs/cpp-auth/src/jwks/JsonJwksParser.cpp`, colle ce même bloc juste
   après la ligne `using json = nlohmann::json;` :

```cpp
using json = nlohmann::json;

namespace
{
	bool	isValidBase64Url(const std::string &value)
	{
		...      // le corps, inchangé
	}
}

auth::JwksParseResult	auth::JsonJwksParser::parse(
```

Dans un `.cpp`, le namespace anonyme a enfin le sens voulu : « fonction privée à ce
fichier ».

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug
```

✅ **Attendu** : la compilation passe, sans aucune ligne `warning`, et
`100% tests passed, 0 tests failed out of 174`.

**Curiosité** : vérifie que les options sont bien passées au compilateur.

```sh
grep -c Wpedantic build/debug/CMakeFiles/matcha_backend.dir/flags.make
grep -c Werror build/debug/CMakeFiles/matcha_backend.dir/flags.make
```

✅ `1` et `1`. `flags.make` contient les options exactes de compilation d'une cible.

---

## 5. Étape D : le preset `release`, et un faux positif

```sh
cmake --preset release
cmake --build --preset release --target matcha_backend -j"$(nproc)"
```

`--target matcha_backend` : en release on ne compile que l'exécutable, comme le fera
l'image de prod.

✅ **Attendu** : une **erreur**, que tu n'avais pas en debug :

```
/opt/vcpkg_installed/x64-linux/include/picojson/picojson.h:249:10: error: '<unnamed>.picojson::value::u_' may be used uninitialized [-Werror=maybe-uninitialized]
cc1plus: all warnings being treated as errors
gmake[3]: *** [libs/cpp-auth/CMakeFiles/cpp-auth.dir/build.make:216: libs/cpp-auth/CMakeFiles/cpp-auth.dir/src/token/TokenValidator.cpp.o] Error 1
```

Mène l'enquête, avec les notions du §1 :

1. **Pourquoi seulement en Release ?** `-Wmaybe-uninitialized` vient de l'analyse de
   l'optimiseur (§1.5). En `Debug`, elle ne tourne pas.
2. **Pourquoi un warning dans `/opt/vcpkg_installed`, malgré `-isystem` (§1.4) ?**
   picojson est inclus par jwt-cpp, lui-même inclus par `TokenValidator.cpp`. En `-O3`,
   GCC **recopie** (*inline*) le code de picojson à l'intérieur de **notre** fonction,
   puis l'analyse. À ce stade, il ne sait plus que ce code venait d'un en-tête système.
3. **Est-ce un vrai bug ?** Non. C'est un **faux positif** connu de GCC 12 avec ce
   warning précis : picojson initialise bien `u_`, mais l'analyse ne suit pas tous les
   chemins. On ne peut de toute façon pas modifier picojson, qui appartient à vcpkg.

**La décision** : couper `-Wmaybe-uninitialized`, pour GCC seulement. Modifie
`backend/cmake/Warnings.cmake` :

```cmake
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    # GCC 12 en Release : faux positif dans picojson (inclus par jwt-cpp),
    # signalé malgré -isystem car le code est inliné dans le nôtre.
    $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
)
```

- `-Wno-<nom>` : désactive un warning précis. Tous les warnings fonctionnent ainsi :
  `-W<nom>` active, `-Wno-<nom>` désactive.
- `$<$<CXX_COMPILER_ID:GNU>:...>` : une **expression génératrice** (guide 02, §1.4).
  « Si le compilateur est GCC, mets cette option, sinon rien. » Clang n'a pas ce faux
  positif, et on ne le prive pas du warning pour rien.
- **Le commentaire est obligatoire** : dans six mois, une ligne `-Wno-...` sans
  explication ressemblerait à de la triche pour cacher un problème.

**Ce qu'on perd, honnêtement** : les vrais « peut-être non initialisé » de notre code ne
seront plus signalés par GCC. Ce n'est pas grave ici, pour trois raisons :
`-Wuninitialized` (« **sûrement** non initialisé ») reste actif, les tests sous
sanitizers (étape suivante) et valgrind attrapent les vraies lectures de mémoire non
initialisée, et cette option est l'une des plus connues pour ses faux positifs. Beaucoup
de gros projets la coupent aussi.

```sh
cmake --build --preset release --target matcha_backend -j"$(nproc)"
```

✅ **Attendu** : la compilation passe, sans warning.

---

## 6. Étape E : le preset `sanitize`, et trois bugs exprès

### 6.1 Compiler et tester

```sh
cmake --preset sanitize
cmake --build --preset sanitize -j"$(nproc)"
ctest --preset sanitize
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 174`.

C'est une **bonne nouvelle** : ASan et UBSan n'ont trouvé aucun bug dans le code
exercé par les tests. Mais comme au guide 02 (§9.3) : **un contrôle qui n'a jamais
échoué ne prouve rien.** On le casse exprès.

### 6.2 Trois bugs volontaires

Ajoute ces trois tests **à la fin** de `backend/tests/images/LocalImageStorageTests.cpp`.
On choisit ce fichier parce que ses tests n'ont besoin ni de la base ni de Redis :

```cpp
TEST(SanitizerDemo, HeapOverflow)
{
	int *values = new int[2];
	values[2] = 42;                 // case 2 d'un tableau de 2 cases : hors limites
	delete[] values;
}

TEST(SanitizerDemo, SignedOverflow)
{
	volatile int big = 2147483647;  // le plus grand int
	const int result = big + 1;     // dépassement : comportement indéfini
	EXPECT_NE(result, 0);
}

TEST(SanitizerDemo, Leak)
{
	int *value = new int(42);       // jamais libéré
	EXPECT_EQ(*value, 42);
}
```

`volatile` empêche le compilateur de calculer `big + 1` à la compilation : le calcul a
lieu pendant l'exécution, là où UBSan le surveille.

D'abord en **debug** :

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R SanitizerDemo
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 3`.

Les trois tests **passent** alors qu'ils contiennent trois bugs. C'est exactement le
danger décrit au §1.6 : un programme C++ faux peut sembler marcher.

Puis en **sanitize** :

```sh
cmake --build --preset sanitize -j"$(nproc)"
ctest --preset sanitize -R SanitizerDemo
```

✅ **Attendu** : les trois échouent, chacun avec son rapport (extraits) :

```
1/3 Test #24: SanitizerDemo.HeapOverflow .......***Failed
==70==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000358 ...
SUMMARY: AddressSanitizer: heap-buffer-overflow /app/tests/images/LocalImageStorageTests.cpp:<ligne> in SanitizerDemo_HeapOverflow_Test::TestBody()

2/3 Test #25: SanitizerDemo.SignedOverflow .....***Failed
/app/tests/images/LocalImageStorageTests.cpp:<ligne>:25: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'

3/3 Test #26: SanitizerDemo.Leak ...............***Failed
==72==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 4 byte(s) in 1 object(s) allocated from:

0% tests passed, 3 tests failed out of 3
```

(Les numéros de test, de processus `==70==` et les adresses changent chez toi.)

Chaque rapport donne **le type de bug, le fichier et la ligne**. Lis la pile d'appels
complète d'ASan au-dessus du `SUMMARY` : elle montre aussi **où** le tableau a été alloué.

**Remets le fichier comme avant** : supprime les trois tests, puis vérifie avec `git` (sur
l'hôte, depuis `~/matcha`) qu'il ne reste aucune trace :

```sh
git diff --stat backend/tests/
```

✅ **Attendu** : aucune ligne. `git diff` compare au dernier commit : rien affiché, le
fichier est revenu à l'identique.

---

## 7. Étape F : brancher les presets dans Docker et le Makefile

Les presets marchent. Il reste à les faire utiliser par les outils, pour ne plus avoir les
options CMake recopiées à trois endroits.

### 7.1 `backend/dev/matcha-build`

Remplace tout le contenu par :

```sh
#!/bin/sh
# Compile le backend avec le preset debug (CMakePresets.json), dans /app/build/debug
# (volume Docker, garde les .o entre deux lancements).
# Usage : matcha-build            -> uniquement l'executable
#         matcha-build all        -> executable + tests
set -eu
cd /app

cmake --preset debug > /dev/null

if [ "${1:-}" = "all" ]; then
	cmake --build --preset debug --parallel "$(nproc)"
else
	cmake --build --preset debug --target matcha_backend --parallel "$(nproc)"
fi
```

- Les 5 options `-D...` ont disparu : elles sont dans le preset `debug`.
- L'ancien script acceptait `CMAKE_BUILD_TYPE` en variable d'environnement. Ce n'est plus
  le cas : en dev, c'est toujours `debug`. Pour un autre type, on lance directement
  `cmake --preset release`.
- `--parallel` est la forme longue de `-j`.

### 7.2 `backend/dev/matcha-dev-server`

Le binaire n'est plus dans `build/` mais dans `build/debug/`. Change la ligne :

```sh
binary=/app/build/matcha_backend
```

en :

```sh
binary=/app/build/debug/matcha_backend
```

### 7.3 `Makefile` (à la racine du dépôt)

Remplace la cible `test` :

```make
# Compile executable + tests puis lance ctest.
test:
	$(COMPOSE_DEV) exec backend sh -c 'matcha-build all && ctest --preset debug'

# Compile et lance tous les tests sous ASan + UBSan (dossier build/sanitize).
sanitize:
	$(COMPOSE_DEV) exec backend sh -c 'cmake --preset sanitize > /dev/null && cmake --build --preset sanitize --parallel "$$(nproc)" && ctest --preset sanitize'
```

Ajoute `sanitize` à la ligne d'aide :

```make
	@echo "Dev        : build / up / down / start / stop / rebuild / test / sanitize / logs / shell"
```

et à la liste `.PHONY` de la dernière ligne :

```make
.PHONY: all secrets build up down start stop rebuild test sanitize logs shell prod prod-down
```

- ⚠️ Les lignes de commande d'un Makefile commencent par une **tabulation**, pas des
  espaces. Sinon : `missing separator`.
- `$$(nproc)` : dans un Makefile, `$` est spécial (`$(COMPOSE_DEV)`). `$$` donne un `$`
  littéral, que `sh` lira ensuite comme `$(nproc)`.
- `ctest --preset` fonctionne parce que `docker compose exec` démarre dans `/app` (le
  `WORKDIR` du Dockerfile), là où se trouve `CMakePresets.json`.
- `.PHONY` dit à `make` que `sanitize` est le nom d'une commande, pas d'un fichier à
  produire.

### 7.4 `backend/Dockerfile`, l'étape `build`

Remplace le `RUN cmake -S . -B build ...` (tout le bloc, jusqu'à
`--parallel "$(nproc)"`) par :

```dockerfile
RUN cmake --preset release \
	&& cmake --build --preset release --target matcha_backend --parallel "$(nproc)"
```

Et dans l'étape `runtime`, change le chemin du binaire :

```dockerfile
COPY --from=build /app/build/release/matcha_backend ./matcha_backend
```

- `VCPKG_ROOT` et `VCPKG_INSTALLED_DIR` sont définis par `ENV` dans l'étape `deps`, dont
  hérite `build` : `$env{...}` les trouve.
- La prod compile maintenant **avec `-Werror`** : une image de prod ne peut plus être
  construite si le code a un warning. C'est voulu : c'est elle qui sera évaluée.

### 7.5 Petits nettoyages

- `.gitignore` (racine) : supprime la ligne `/backend/build-sanitize/`. Le dossier de build
  sanitize est maintenant `backend/build/sanitize/`, déjà couvert par `/backend/build/`.
- `backend/.dockerignore` : supprime la ligne `build-sanitize/`, pour la même raison.
- `docs/docker-backend.md`, dans le schéma du `matcha-dev-server` : remplace
  `./build/matcha_backend` par `./build/debug/matcha_backend`.

---

## 8. Étape G : vérifier tout le parcours

### 8.1 Reconstruire l'image de dev

Les scripts de `backend/dev/` sont **copiés dans l'image** (`COPY dev/ /usr/local/bin/`
dans le `Dockerfile`). Ta modification n'est donc pas vue par le conteneur qui tourne : il
faut reconstruire l'image.

Avant, vide l'ancien dossier de build (le build « à plat » de `matcha-build`, qui ne
servira plus). Dans le conteneur :

```sh
rm -rf build/*
exit
```

`build/` est le point de montage du volume : on vide son **contenu** (`build/*`), pas le
dossier lui-même. Ça efface aussi `build/debug`, `build/release` et `build/sanitize` des
étapes précédentes : tout repart propre, la recompilation prend moins d'une minute.

Puis, dans le terminal 1, arrête `make up` (Ctrl+C), et :

```sh
make build
```

✅ **Attendu** : l'image est reconstruite (les dépendances vcpkg restent en cache, seule
la petite couche `dev` est refaite), puis `[dev] Premiere compilation...` et
`[dev] Lancement du serveur`. Le serveur tourne avec le binaire de `build/debug/`.

### 8.2 Les deux suites de tests

Dans un autre terminal, depuis `~/matcha` :

```sh
make test
make sanitize
```

✅ **Attendu** : les deux se terminent par `100% tests passed, 0 tests failed out of 174`.

`make sanitize` est un peu plus lent : il compile tout une seconde fois, avec les
sanitizers, dans `build/sanitize`.

### 8.3 L'image de prod

On ne lance pas `make prod` (il arrêterait l'environnement de dev). On **construit**
seulement l'image :

```sh
docker compose build backend
```

`docker compose` **sans** `-f docker-compose.dev.yml` utilise la configuration de prod
(`target: runtime`).

✅ **Attendu** : `cmake --preset release` puis la compilation passent sans warning, et la
construction se termine sans erreur.

---

## 9. Étape H : commiter

Deux commits : la correction de cpp-auth est indépendante des presets. Séparée, elle se
relit et s'annule toute seule.

```sh
git add backend/libs/cpp-auth/include/auth/jwks/JsonJwksParser.hpp backend/libs/cpp-auth/src/jwks/JsonJwksParser.cpp
git commit -m "cpp-auth : isValidBase64Url sort de l'en-tête public"
```

```sh
git add backend/CMakePresets.json backend/cmake/Warnings.cmake backend/CMakeLists.txt \
    backend/dev/matcha-build backend/dev/matcha-dev-server backend/Dockerfile \
    backend/.dockerignore Makefile .gitignore docs/docker-backend.md
git status
```

✅ **Attendu** : sous `Changes to be committed`, 2 nouveaux fichiers et 8 modifiés, et
rien dans `Changes not staged` ni `Untracked files`.

```sh
git commit -m "build : presets CMake, warnings en erreur et preset sanitize (audit 4.19)"
git log --oneline -3
```

Coche « Drapeaux et presets communs » dans le planning. 🎉

---

## 10. Questions

Réponds sans regarder, puis relis la section indiquée si tu bloques. Écris sous chaque
**Ta réponse**, à la place de la ligne `…`. Quand tu as fini, préviens-moi : je remplis la
§11.

### Q1. `-Werror` dans le preset (§1.2)

Pourquoi `-Werror` est-il activé dans `CMakePresets.json` et pas dans
`cmake/Warnings.cmake`, ni dans le `CMakeLists.txt` de cpp-auth ?

**Ta réponse :**

…

### Q2. L'ordre de `include(cmake/Warnings.cmake)` (§1.3, §4.2)

Que se passerait-il si tu déplaçais la ligne `include(cmake/Warnings.cmake)` tout en bas
de `backend/CMakeLists.txt` ? Quelles cibles auraient encore les warnings ?

**Ta réponse :**

…

### Q3. Le warning de `JsonJwksParser.hpp` (§4.4)

`isValidBase64Url` est appelée dans `JsonJwksParser.cpp`. Pourquoi GCC dit-il quand même
qu'elle est « defined but not used » ? Et pourquoi la déplacer dans le `.cpp` est-il
mieux qu'ajouter `-Wno-unused-function` ?

**Ta réponse :**

…

### Q4. Debug et Release (§1.5, §5)

Pourquoi le warning de picojson n'apparaît-il qu'avec le preset `release`, alors que c'est
le même code et les mêmes options de warnings ?

**Ta réponse :**

…

### Q5. Les trois bugs (§1.6, §6.2)

Les trois tests `SanitizerDemo` passent en `debug`. Explique pourquoi un test peut
« passer » avec un débordement de tableau. Et que se passerait-il pour `SignedOverflow`
sans `-fno-sanitize-recover=all` ?

**Ta réponse :**

…

### Q6. `sanitize` hérite de `debug` (§3)

Pourquoi le preset `sanitize` hérite-t-il de `debug` plutôt que de `base` ou de
`release` ? Donne deux raisons.

**Ta réponse :**

…

### Q7. Un dossier de build par preset (§1.7, guide 01 étape G)

Que se passerait-il si les trois presets utilisaient le même `binaryDir` (`build/`), et
que tu lançais `cmake --preset sanitize` puis `cmake --preset debug` ?

**Ta réponse :**

…

### Q8. `make build` (§8.1)

Tu as modifié `backend/dev/matcha-build`, qui est pourtant monté dans le conteneur via
`./backend:/app`. Pourquoi faut-il quand même reconstruire l'image pour que `make test`
utilise la nouvelle version ?

**Ta réponse :**

…

---

## 11. Correction

> Remplie après tes réponses.
>
> **Légende** : 🟢 juste et complet · 🟡 juste mais incomplet ou imprécis ·
> 🔴 faux ou à côté · ⏳ pas encore corrigé

| Question | Évaluation |
|---|---|
| Q1. `-Werror` dans le preset | ⏳ |
| Q2. L'ordre de `include` | ⏳ |
| Q3. Le warning de `JsonJwksParser.hpp` | ⏳ |
| Q4. Debug et Release | ⏳ |
| Q5. Les trois bugs | ⏳ |
| Q6. `sanitize` hérite de `debug` | ⏳ |
| Q7. Un dossier de build par preset | ⏳ |
| Q8. `make build` | ⏳ |
