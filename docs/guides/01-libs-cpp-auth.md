# Guide 01 : passer `cpp-auth` dans `backend/libs/`

> Planning : lundi, « Monorepo › Créer backend/libs/ et y déplacer cpp-auth »
> (corrige l'audit 6.1 #14). Durée : environ 1 h 30, lecture comprise.
>
> Ce guide te dit **exactement** quoi faire, et surtout **pourquoi**. C'est toi qui tapes
> les commandes et écris le code. Toutes les commandes et tous les résultats attendus ont
> été vérifiés sur une copie du projet.

## Sommaire

0. [Ce qu'on fait, et le résultat attendu](#0-ce-quon-fait-et-le-résultat-attendu)
1. [Les notions CMake à connaître avant de commencer](#1-les-notions-cmake-à-connaître-avant-de-commencer)
2. [Étape A : partir d'un dépôt propre](#2-étape-a--partir-dun-dépôt-propre)
3. [Étape B : mesurer l'état actuel](#3-étape-b--mesurer-létat-actuel)
4. [Étape C : déplacer le dossier](#4-étape-c--déplacer-le-dossier)
5. [Étape D : trouver ce qui pointe vers l'ancien chemin](#5-étape-d--trouver-ce-qui-pointe-vers-lancien-chemin)
6. [Étape E : brancher `libs/` dans CMake, puis 1er commit](#6-étape-e--brancher-libs-dans-cmake-puis-1er-commit)
7. [Étape F : corriger l'option des tests](#7-étape-f--corriger-loption-des-tests)
8. [Étape G : le piège du cache CMake](#8-étape-g--le-piège-du-cache-cmake)
9. [Étape H : réactiver les tests de la lib en dev](#9-étape-h--réactiver-les-tests-de-la-lib-en-dev)
10. [Étape I : rendre la dépendance OpenSSL explicite](#10-étape-i--rendre-la-dépendance-openssl-explicite)
11. [Étape J : prouver que la lib est indépendante, puis 2e commit](#11-étape-j--prouver-que-la-lib-est-indépendante-puis-2e-commit)
12. [Vérifie que tu as compris](#12-vérifie-que-tu-as-compris)

---

## 0. Ce qu'on fait, et le résultat attendu

On transforme `backend/` en **monorepo** : un seul dépôt qui contient l'application
(`src/`) **et** des bibliothèques réutilisables (`libs/`).

```
AVANT                              APRÈS
backend/                           backend/
├── CMakeLists.txt                 ├── CMakeLists.txt        ← add_subdirectory(libs)
├── cpp-auth/                      ├── libs/
│   ├── CMakeLists.txt             │   ├── CMakeLists.txt    ← nouveau : liste les libs
│   ├── include/ src/ tests/       │   └── cpp-auth/         ← déplacé, presque inchangé
│   └── vcpkg.json                 │       └── CMakeLists.txt  ← option des tests corrigée
├── src/                           ├── src/
└── vcpkg.json                     └── vcpkg.json            ← + openssl
```

**La règle d'or du monorepo** : `src/` peut utiliser `libs/`, **jamais l'inverse**. Une
lib ne doit rien savoir de Matcha. Le dossier `libs/` rend cette frontière visible : si un
jour tu écris `#include "repositories/..."` dans une lib, tu la casses.

**Pourquoi c'est une étape à part entière**, alors que ça ressemble à un simple
déplacement : elle t'oblige à comprendre les mécanismes CMake que tu réutiliseras pour
chaque lib de la semaine (cpp-core, cpp-pg, etc.).

---

## 1. Les notions CMake à connaître avant de commencer

Lis cette partie une fois en entier. Tu y reviendras pendant les étapes.

### 1.1 CMake ne compile pas, il génère

CMake est un **générateur** : il lit tes `CMakeLists.txt` et produit des `Makefile`, que
`make` utilise ensuite pour appeler le compilateur. Il y a donc deux phases :

| Phase | Commande | Ce qui se passe | Quand la relancer |
|---|---|---|---|
| **Configuration** | `cmake -S . -B build` | Lit les `CMakeLists.txt`, cherche les dépendances, écrit `build/CMakeCache.txt` et les Makefiles | Automatique quand un `CMakeLists.txt` change |
| **Compilation** | `cmake --build build` | Compile les `.cpp` modifiés et fait l'édition de liens | À chaque modification de code |

`-S` est le dossier des **sources** (là où se trouve le `CMakeLists.txt`), `-B` le dossier
de **build** (là où vont les fichiers générés). Chez toi, `matcha-build` (appelé par
`make rebuild`) fait les deux à la suite.

### 1.2 La cible (*target*)

Une **cible** est une chose que CMake sait construire : un exécutable (`add_executable`)
ou une bibliothèque (`add_library`). Chaque cible porte ses propres réglages : ses
fichiers sources, ses dossiers d'includes, ses dépendances.

```cmake
add_library(cpp-auth STATIC ...)                    # la cible s'appelle "cpp-auth"
target_include_directories(cpp-auth PUBLIC include) # ses includes
target_link_libraries(matcha_backend PRIVATE cpp-auth::cpp-auth)  # le backend l'utilise
```

`PUBLIC` et `PRIVATE` disent si un réglage **se propage** à ceux qui utilisent la cible.
Les includes de cpp-auth sont `PUBLIC` : quand `matcha_backend` se lie à cpp-auth, il
reçoit automatiquement le dossier `include/` de cpp-auth. C'est pour ça que
`#include <auth/token/...>` fonctionne dans `src/main.cpp` sans que le backend écrive le
chemin nulle part.

### 1.3 Deux façons d'utiliser une bibliothèque

| | `add_subdirectory(chemin)` | `find_package(Nom CONFIG)` |
|---|---|---|
| Ce que ça fait | Inclut le `CMakeLists.txt` d'un sous-dossier **dans le même build** | Cherche une lib **déjà compilée et installée** ailleurs, grâce à son fichier `NomConfig.cmake` |
| La lib est compilée… | en même temps que ton projet | avant, séparément |
| Utilisé chez toi par | le backend pour cpp-auth (et bientôt toutes tes libs) | le backend pour Crow, GTest… (installés par vcpkg) et `cpp-auth/tests/package-consumer` |

Une lib propre doit **supporter les deux**. C'est ce que tu vérifieras à l'étape J.

### 1.4 L'alias `cpp-auth::cpp-auth`

La cible s'appelle `cpp-auth`, mais le backend se lie à `cpp-auth::cpp-auth`. Pourquoi ?

- Avec `find_package`, les cibles importées portent le préfixe défini dans
  `install(EXPORT ... NAMESPACE cpp-auth::)`. On obtient donc `cpp-auth::cpp-auth`.
- Avec `add_subdirectory`, la ligne `add_library(cpp-auth::cpp-auth ALIAS cpp-auth)` crée
  le **même nom**.

Résultat : le code qui utilise la lib s'écrit pareil dans les deux cas. Autre avantage :
un nom contenant `::` est forcément une cible. Si tu fais une faute de frappe, CMake
s'arrête avec une erreur, au lieu de chercher en silence un fichier `libcpp-auht.a`.

### 1.5 `project()` et `PROJECT_IS_TOP_LEVEL`

`project(nom VERSION x.y.z LANGUAGES CXX)` déclare un projet. On peut l'appeler dans un
sous-dossier, ce qui y déclare un sous-projet.

Depuis CMake 3.21, chaque appel à `project()` définit la variable `PROJECT_IS_TOP_LEVEL` :

- `ON` si ce projet est **le premier** à avoir été lu, c'est-à-dire si on a lancé
  `cmake -S libs/cpp-auth` directement ;
- `OFF` s'il a été inclus par un autre, avec `add_subdirectory(...)` depuis le backend.

C'est exactement ce qu'il faut pour qu'une lib sache si elle est **développée** (on veut
ses tests) ou **utilisée** (on ne les veut pas).

### 1.6 `option()` et le cache, la notion la plus piégeuse

```cmake
option(CPP_AUTH_BUILD_TESTS "Build cpp-auth tests" OFF)
```

déclare un interrupteur ON/OFF, que l'utilisateur peut forcer avec
`cmake -DCPP_AUTH_BUILD_TESTS=ON`. **Mais** la valeur est enregistrée dans
`build/CMakeCache.txt`, et `option()` **ne remplace jamais une valeur déjà présente dans
le cache**. Changer la valeur par défaut dans le `CMakeLists.txt` n'a donc **aucun
effet** sur un dossier `build/` existant. Tu vas le constater à l'étape G.

Pourquoi ce comportement ? Pour que tes choix (`-D...`) survivent aux reconfigurations
automatiques. Le revers : il faut parfois repartir d'un cache vide.

### 1.7 vcpkg : manifeste et *toolchain*

- `vcpkg.json` (le **manifeste**) liste les dépendances. vcpkg les télécharge et les
  compile dans `/opt/vcpkg_installed` (dans l'étape `deps` du Dockerfile).
- `-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake` branche vcpkg dans CMake : c'est ce qui permet
  à `find_package(Crow ...)` de trouver ce que vcpkg a installé.
- **Un seul manifeste est lu : celui à la racine du projet compilé.** Dans le monorepo,
  c'est `backend/vcpkg.json`. Celui de `cpp-auth` ne sert que si on compile la lib
  toute seule.

---

## 2. Étape A : partir d'un dépôt propre

**Pourquoi** : tu vas faire deux commits qui doivent ne contenir **que** cette étape. Or
ton dépôt contient encore le travail des sessions précédentes (secrets, Docker
multi-étapes, docs), pas encore commité, et une partie est **déjà indexée** (la
suppression de `docker-entrypoint.sh` et des fichiers de build). Si tu ne le commites pas
avant, il partira dans ton commit de déplacement.

```sh
cd ~/matcha
git branch --show-current   # tu dois être sur libs/move-cpp-auth
git status                  # relis la liste
git diff --stat             # taille des changements, fichier par fichier
git add -A
git commit -m "Secrets, build multi-étapes et mode dev"
git status                  # doit afficher : nothing to commit, working tree clean
```

> `sortie.txt` partira dans ce commit avec `git add -A`. Supprime-le avant (`rm sortie.txt`)
> si tu ne veux pas le versionner.

---

## 3. Étape B : mesurer l'état actuel

**Pourquoi** : pour prouver à la fin que tu n'as rien cassé, il te faut un chiffre de
départ.

```sh
make up          # terminal 1 : laisse-le tourner
make test        # terminal 2
```

✅ **Résultat attendu** : à la fin, `100% tests passed, 0 tests failed out of 174`.

Note ce chiffre : **174**. Dedans, **151** viennent de cpp-auth et **23** du backend. Tu
verras ce découpage réapparaître à l'étape G.

---

## 4. Étape C : déplacer le dossier

**Où** : dans `backend/`. La destination est `backend/libs/cpp-auth`, à côté de
`backend/src/`.

**Comment** : `git mv <source> <destination>`. On déplace le dossier `cpp-auth` existant ;
`libs/` n'est que l'endroit où on le pose.

```sh
cd ~/matcha/backend
mkdir libs
git mv cpp-auth libs/cpp-auth
git status
```

**Pourquoi `mkdir` d'abord ?** Pour que la destination existe. Git ne suit pas les
dossiers vides : `libs/` n'existera pour lui qu'une fois qu'il contiendra quelque chose.

**Pourquoi `git mv` plutôt que `mv` ?** Git n'enregistre pas les renommages : à chaque
commit, il **devine** qu'un fichier supprimé et un fichier ajouté au contenu identique
forment un renommage. `git mv` déplace les fichiers et met directement le changement dans
l'index, sans en oublier un. Ce qui compte vraiment, c'est que le commit soit un
**déplacement pur**, sans aucune modification de contenu. Git voit alors des renommages à
100 %, et `git log --follow libs/cpp-auth/src/auth/Authenticator.cpp` retrouve tout
l'historique d'avant le déplacement.

✅ **Résultat attendu**, dans « Changes to be committed » :

```
renamed:    cpp-auth/CMakeLists.txt -> libs/cpp-auth/CMakeLists.txt
renamed:    cpp-auth/README.md -> libs/cpp-auth/README.md
...
```

❌ Si tu vois des `deleted:` d'un côté et des `new file:` de l'autre, le déplacement n'a
pas été fait par git. Annule avec `git reset --hard` (tu n'as rien d'autre en cours grâce
à l'étape A), puis recommence.

---

## 5. Étape D : trouver ce qui pointe vers l'ancien chemin

**Pourquoi** : un déplacement casse tout ce qui contient le chemin **en dur**. Avant de
corriger au hasard, on fait l'inventaire.

```sh
cd ~/matcha
grep -rn "cpp-auth" backend docker-compose*.yml Makefile \
  --exclude-dir=libs --exclude-dir=build --exclude-dir=vcpkg_installed
```

Pour chaque ligne trouvée, demande-toi : **chemin** ou **nom de cible** ?

| Ligne trouvée | Type | À faire |
|---|---|---|
| `backend/CMakeLists.txt:23: add_subdirectory(cpp-auth)` | chemin | à corriger, à l'étape E |
| `backend/CMakeLists.txt:58: PRIVATE cpp-auth::cpp-auth` (et lignes 94, 112, 155, 178) | nom de cible (§1.4) | rien |

**Une seule ligne à corriger.** Voici pourquoi le reste ne casse pas :

- **Les `#include <auth/...>`** : le dossier d'includes est déclaré *dans* la lib, avec
  `${CMAKE_CURRENT_SOURCE_DIR}/include`. Cette variable vaut toujours « le dossier du
  `CMakeLists.txt` en cours de lecture », où qu'il soit. Le chemin suit donc la lib.
- **Les certificats de test** (`tests/certs/test-key.pem`) : les tests reçoivent
  `CPP_AUTH_SOURCE_DIR`, construit lui aussi à partir de `CMAKE_CURRENT_SOURCE_DIR`.
- **Le Dockerfile** : l'étape `deps` ne copie que `vcpkg.json`, et l'étape `build` copie
  tout (`COPY . .`). Aucun des deux ne nomme cpp-auth.

👉 **La leçon** : une lib bien écrite ne connaît que **ses propres chemins relatifs**.
C'est ce qui la rend déplaçable. Garde ce critère en tête pour toutes tes futures libs.

---

## 6. Étape E : brancher `libs/` dans CMake, puis 1er commit

### 6.1 Créer `backend/libs/CMakeLists.txt`

**Pourquoi un fichier à part**, plutôt que `add_subdirectory(libs/cpp-auth)` directement
dans le CMakeLists racine ? Cette semaine, tu vas ajouter cpp-core, cpp-pg,
cpp-validation, cpp-geo, cpp-realtime et cpp-notify. Avec ce fichier, la liste des libs
et **leur ordre** vivent au même endroit, à côté des libs, et non au milieu de la config
de l'application.

**Pourquoi l'ordre compte** : CMake lit les `add_subdirectory` de haut en bas. Si cpp-pg
appelle `target_link_libraries(cpp-pg PUBLIC cpp-core::cpp-core)` et que cpp-core n'a pas
encore été lu, la cible `cpp-core::cpp-core` n'existe pas : CMake s'arrête avec une erreur
du type « target not found ». On déclare donc les libs **des couches basses vers les
couches hautes**, comme dans le schéma du planning.

Contenu à écrire dans `backend/libs/CMakeLists.txt` :

```cmake
# Bibliothèques internes du backend, indépendantes de Matcha.
# Ordre important : une lib doit être déclarée AVANT celles qui en dépendent
# (couche 0 en premier, voir le planning « Semaine des bibliothèques »).

add_subdirectory(cpp-auth)
```

Ici, `cpp-auth` est relatif à `libs/`. C'est le dossier où se trouve ce fichier.

### 6.2 Modifier `backend/CMakeLists.txt`

Ligne 23, remplace :

```cmake
add_subdirectory(cpp-auth)
```

par :

```cmake
add_subdirectory(libs)
```

### 6.3 Vérifier

```sh
make rebuild
```

✅ **Résultat attendu** : `[100%] Built target matcha_backend`, puis
`[dev] Serveur arrete, matcha-dev-server le relance...`. Dans le terminal de `make up`, le
serveur redémarre (`Crow/master server is running`).

Pas besoin de relancer cmake à la main : CMake voit que `CMakeLists.txt` a changé et
reconfigure tout seul.

### 6.4 Premier commit

```sh
cd ~/matcha
git add backend/CMakeLists.txt backend/libs/CMakeLists.txt
git commit -m "Déplace cpp-auth dans backend/libs/"
git show --stat HEAD | tail -5
```

✅ Tu dois voir surtout des `=>` (des renommages) et seulement deux fichiers modifiés : le
CMakeLists racine (1 ligne) et le nouveau `libs/CMakeLists.txt`.

**Pourquoi s'arrêter ici pour commiter ?** Ce commit est un **déplacement sans
changement de comportement**. Le suivant **change** un comportement (quels tests sont
compilés). Séparés, chacun se relit en 10 secondes. Et si le deuxième pose problème, tu
peux l'annuler (`git revert`) sans défaire le déplacement.

---

## 7. Étape F : corriger l'option des tests

### 7.1 Comprendre le bug (audit 6.1 #14)

Ouvre `backend/libs/cpp-auth/CMakeLists.txt`, lignes 1 à 10 :

```cmake
cmake_minimum_required(VERSION 3.20)

set(CPP_AUTH_VERSION 0.1.0)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)        # suis-je le projet principal ?
    project(cpp-auth VERSION ${CPP_AUTH_VERSION} LANGUAGES CXX)
    option(CPP_AUTH_BUILD_TESTS "Build cpp-auth tests" OFF)    # oui → tests OFF
else()
    option(CPP_AUTH_BUILD_TESTS "Build cpp-auth tests" ON)     # non → tests ON
endif()
```

C'est **à l'envers** :

- quelqu'un qui **utilise** cpp-auth (le backend) doit compiler 151 tests qui ne le
  concernent pas, et avoir GTest installé ;
- quelqu'un qui **développe** cpp-auth seule n'a pas ses tests par défaut.

Ce qu'on veut : **tests ON quand je développe la lib, OFF quand je l'utilise.** C'est la
définition même de `PROJECT_IS_TOP_LEVEL` (§1.5).

### 7.2 Le code à écrire

Remplace les lignes 1 à 10 par :

```cmake
cmake_minimum_required(VERSION 3.21)

set(CPP_AUTH_VERSION 0.1.0)
project(cpp-auth VERSION ${CPP_AUTH_VERSION} LANGUAGES CXX)

option(CPP_AUTH_BUILD_TESTS "Build cpp-auth tests" ${PROJECT_IS_TOP_LEVEL})
```

Ligne par ligne :

| Ligne | Pourquoi |
|---|---|
| `VERSION 3.21` | `PROJECT_IS_TOP_LEVEL` n'existe qu'à partir de CMake 3.21. Déclarer ce minimum, c'est garantir une erreur claire sur un CMake trop ancien plutôt qu'un comportement faux. Le conteneur a CMake 3.25. |
| `set(CPP_AUTH_VERSION 0.1.0)` | Inchangé. Toujours utilisé plus bas par `write_basic_package_version_file` (le fichier qui permet `find_package(cpp-auth 0.1 CONFIG)`). |
| `project(...)` **sans `if`** | On l'appelle dans tous les cas, car c'est cet appel qui calcule `PROJECT_IS_TOP_LEVEL`. Appeler `project()` dans un sous-dossier est autorisé et courant. |
| `option(... ${PROJECT_IS_TOP_LEVEL})` | La valeur par défaut **est** la réponse à « suis-je le projet principal ? ». Plus besoin de `if/else`. |

Le `if/else` disparaît entièrement : 5 lignes au lieu de 10, et plus de logique à lire.

Ne touche pas au reste du fichier : le bloc `if(CPP_AUTH_BUILD_TESTS) ... endif()` en bas
reste tel quel.

### 7.3 Lancer les tests

```sh
make test
```

Lis le total… et passe à l'étape G.

---

## 8. Étape G : le piège du cache CMake

✅ **Résultat attendu à l'étape F : toujours 174 tests.** Ta modification n'est pas
fausse : tu viens de rencontrer le comportement décrit au §1.6.

**Constate-le toi-même** :

```sh
make shell                                   # ouvre un terminal dans le conteneur backend
grep CPP_AUTH_BUILD_TESTS build/CMakeCache.txt
```

✅ Tu dois lire `CPP_AUTH_BUILD_TESTS:BOOL=ON`. C'est la valeur calculée par l'**ancien**
code (sous-projet → ON), gardée dans le cache du volume `backend_build`. Ta nouvelle valeur
par défaut ne s'applique qu'à un cache où la variable n'existe pas encore.

**Repartir d'un cache vide** (toujours dans le conteneur) :

```sh
rm build/CMakeCache.txt
exit
make test
```

**Pourquoi supprimer seulement `CMakeCache.txt` et pas tout `build/` ?** Le cache contient
les **réglages**, le reste contient les **fichiers compilés** (`.o`). En ne supprimant que
le cache, on garde les `.o` et la recompilation reste rapide. Il existe aussi
`cmake --fresh`, qui fait la même chose, à connaître.

✅ **Résultat attendu** : `100% tests passed, 0 tests failed out of 23`.

Ce sont les 23 tests du backend, sans les 151 de cpp-auth. Ta correction fonctionne :
utilisée comme sous-projet, la lib ne compile plus ses tests.

> 💡 Retiens ce réflexe pour toute la semaine : **« j'ai changé une `option()` ou un
> `set(... CACHE ...)` et rien ne change » → c'est le cache.**

---

## 9. Étape H : réactiver les tests de la lib en dev

**Pourquoi** : pour une dépendance externe, ne pas compiler ses tests est le bon choix.
Mais cpp-auth est **ta** lib, développée dans ce dépôt : `make test` doit la vérifier.
Il faut donc dire explicitement au build de dev : « je veux aussi les tests de cpp-auth ».

**Où le mettre ?**

| Endroit | Verdict |
|---|---|
| `backend/dev/matcha-build` (le script de compilation en dev) | ✅ **maintenant** : c'est lui qui configure CMake en dev |
| un `CMakePresets.json` | ✅ **plus tard** : tâche « Drapeaux et presets communs » du planning, où tous les `-D` déménageront |
| le CMakeLists racine (`set(CPP_AUTH_BUILD_TESTS ON)`) | ❌ l'application imposerait un choix interne à la lib. Exactement le couplage qu'on veut éviter. |

**Le code** : dans `backend/dev/matcha-build`, dans la commande `cmake -S . -B build`,
ajoute une ligne `-DCPP_AUTH_BUILD_TESTS=ON \` juste après la ligne
`-DVCPKG_MANIFEST_INSTALL=OFF \`, avec un commentaire au-dessus de la commande :

```sh
# Tests des libs internes actives en dev (a deplacer dans CMakePresets.json).
cmake -S . -B build \
	...
	-DVCPKG_MANIFEST_INSTALL=OFF \
	-DCPP_AUTH_BUILD_TESTS=ON \
	> /dev/null
```

**Pourquoi c'est sans risque pour `make rebuild`** : `make rebuild` ne compile que la
cible `matcha_backend` (`--target matcha_backend`). Les tests sont *déclarés* mais pas
*compilés*, donc ça ne ralentit rien. Seul `make test` (`matcha-build all`) les compile.

**Pourquoi la prod n'est pas concernée** : l'étape `build` du Dockerfile ne passe pas
cette option. cpp-auth y est donc en `OFF`, et la prod ne compile ni ne configure ses
tests.

**Appliquer** : ce script est **copié dans l'image** `dev` (`COPY dev/ /usr/local/bin/`
dans le Dockerfile). Le modifier sur ton disque ne change donc rien au conteneur qui
tourne. Il faut reconstruire l'image :

```sh
# Ctrl+C dans le terminal de make up, puis :
make build
# dans le 2e terminal, une fois le serveur lancé :
make test
```

✅ **Résultat attendu** : `100% tests passed, 0 tests failed out of 174`. Les 151 tests
de cpp-auth sont revenus, mais cette fois **parce que tu l'as demandé**, pas par défaut.

---

## 10. Étape I : rendre la dépendance OpenSSL explicite

**Le constat** : compare `backend/libs/cpp-auth/vcpkg.json` et `backend/vcpkg.json`.
cpp-auth déclare `openssl`, la racine non. Pourtant, ça compile. Pourquoi ?

<details>
<summary>Réponse</summary>

La racine déclare `curl` avec la *feature* `openssl` : vcpkg installe donc OpenSSL **pour
curl**, et cpp-auth en profite. C'est une **dépendance transitive** (on l'obtient « par
ricochet »). Ça marche, mais c'est fragile : le jour où tu retires curl, cpp-auth casse
sans raison apparente.

</details>

**La règle** (§1.7) : seul le manifeste racine est lu. Il doit donc contenir **l'union des
dépendances directes** de l'application et de toutes les libs.

**Le code** : dans `backend/vcpkg.json`, ajoute `"openssl",` dans la liste
`dependencies`, par exemple juste après `"libsodium",`.

**Appliquer** : `vcpkg.json` est copié par l'étape `deps` du Dockerfile, il faut donc
reconstruire l'image (`make build`). vcpkg relance son installation, mais ses paquets sont
en cache sur goinfre : c'est rapide. Rien ne change dans le binaire (OpenSSL était déjà
là) : tu as rendu une dépendance **honnête**, pas ajouté une fonctionnalité.

---

## 11. Étape J : prouver que la lib est indépendante, puis 2e commit

**Pourquoi** : jusqu'ici, tu as testé cpp-auth *à travers* le backend. Une vraie lib doit
aussi fonctionner **seule**, et **installée** (via `find_package`, §1.3). Ces deux
vérifications font partie de la « définition de terminé » du planning.

Tout se passe dans le conteneur, dans `/tmp`, pour ne rien laisser dans le projet :

```sh
make shell
T="-DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=$VCPKG_INSTALLED_DIR -DVCPKG_MANIFEST_INSTALL=OFF"
```

`T` évite de retaper les trois options vcpkg à chaque commande.

### 11.1 La lib seule

```sh
cmake -S libs/cpp-auth -B /tmp/alone $T
grep CPP_AUTH_BUILD_TESTS /tmp/alone/CMakeCache.txt
cmake --build /tmp/alone -j"$(nproc)"
ctest --test-dir /tmp/alone
```

✅ **Résultats attendus** :

- `CPP_AUTH_BUILD_TESTS:BOOL=ON`, sans que tu l'aies demandé : ici cpp-auth est le
  projet principal, donc `PROJECT_IS_TOP_LEVEL` vaut ON ;
- `100% tests passed, 0 tests failed out of 151`.

### 11.2 La lib installée, utilisée par un autre projet

```sh
cmake --install /tmp/alone --prefix /tmp/inst
ls /tmp/inst/lib/cmake/cpp-auth
cmake -S libs/cpp-auth/tests/package-consumer -B /tmp/cons $T -DCMAKE_PREFIX_PATH=/tmp/inst
cmake --build /tmp/cons
/tmp/cons/cpp_auth_package_consumer && echo OK
exit
```

Ce qui se passe :

1. `cmake --install` copie le `.a`, les en-têtes et les fichiers `cpp-auth*.cmake` dans
   `/tmp/inst`, comme le ferait un vrai « make install ».
2. `package-consumer` est un mini-projet qui fait `find_package(cpp-auth CONFIG
   REQUIRED)`. `-DCMAKE_PREFIX_PATH=/tmp/inst` lui dit où chercher.
3. S'il compile et s'exécute, ton `cpp-authConfig.cmake` est correct : il retrouve bien
   les dépendances de cpp-auth (OpenSSL, jwt-cpp…, via `find_dependency`).

✅ **Résultats attendus** : `ls` affiche `cpp-authConfig.cmake`,
`cpp-authConfigVersion.cmake`, `cpp-authTargets.cmake` et
`cpp-authTargets-noconfig.cmake`, et la dernière commande affiche `OK`.

C'est **cette mécanique** (install + Config.cmake + package-consumer) que tu recopieras
dans `libs/_template` à la tâche suivante du planning. Prends le temps d'ouvrir
`libs/cpp-auth/cmake/cpp-authConfig.cmake.in` et `tests/package-consumer/` pour voir à
quoi ils ressemblent.

### 11.3 La prod démarre toujours

```sh
make down
make prod        # Ctrl+C quand "Crow/master server is running" s'affiche
make prod-down
```

### 11.4 Deuxième commit

```sh
cd ~/matcha
git status       # 3 fichiers : libs/cpp-auth/CMakeLists.txt, dev/matcha-build, vcpkg.json
git add -A
git commit -m "cpp-auth : tests désactivés par défaut en sous-projet (audit 6.1 #14)"
```

Coche la tâche dans le planning. 🎉

---

## 12. Vérifie que tu as compris

Réponds sans regarder. Si tu bloques, relis la section indiquée.

1. Quelle différence entre `add_subdirectory()` et `find_package()` ? Lequel utilise le
   backend pour cpp-auth, et lequel utilise `package-consumer` ? (§1.3, étape J)
2. Pourquoi l'alias `cpp-auth::cpp-auth` existe-t-il ? (§1.4)
3. Pourquoi les `#include <auth/...>` du backend n'ont-ils pas cassé après le
   déplacement ? (étape D)
4. Pourquoi changer la valeur par défaut d'une `option()` n'a-t-il eu aucun effet sur le
   build existant ? Comment l'as-tu contourné ? (§1.6, étape G)
5. Que vaut `PROJECT_IS_TOP_LEVEL` pour cpp-auth quand tu lances `make test` ? Et à
   l'étape J ? (§1.5)
6. Pourquoi modifier `dev/matcha-build` demande-t-il un `make build`, alors qu'un
   `.cpp` ne demande qu'un `make rebuild` ? (étape H)
7. Demain, cpp-pg dépendra de cpp-core. Que se passe-t-il si tu écris
   `add_subdirectory(cpp-pg)` avant `add_subdirectory(cpp-core)` ? (étape E)

Quand tu as fini : montre `git log --oneline -3` et tes réponses pour une revue.
