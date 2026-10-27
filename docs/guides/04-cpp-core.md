# Guide 04 : cpp-core (Error, Result, Config, Logger)

> Planning : lundi, « cpp-core » (Error et Result, Config, Logger). Durée : environ 3 h,
> lecture comprise.
>
> Prérequis : le guide 03 est terminé (presets `debug`/`release`/`sanitize`, `-Werror`).
>
> Même principe : je te dis **exactement** quoi faire et **pourquoi**, c'est toi qui
> tapes. Tout a été vérifié sur une copie du projet dans l'image `matcha_backend:dev`
> (GCC 12.2, CMake 3.25) : 193 tests en `debug` et en `sanitize`, la lib installée seule
> avec son package-consumer, et le vrai démarrage du serveur.
>
> Questions de fin au §12, correction au §13.

## Sommaire

0. [Ce qu'on fait, et le résultat attendu](#0-ce-quon-fait-et-le-résultat-attendu)
1. [Les notions à connaître avant de commencer](#1-les-notions-à-connaître-avant-de-commencer)
2. [Étape A : réparer le gabarit, puis partir propre](#2-étape-a--réparer-le-gabarit-puis-partir-propre)
3. [Étape B : générer cpp-core et la brancher](#3-étape-b--générer-cpp-core-et-la-brancher)
4. [Étape C : `Error` et `Result`](#4-étape-c--error-et-result)
5. [Étape D : `Environment`](#5-étape-d--environment)
6. [Étape E : `ConfigReader`](#6-étape-e--configreader)
7. [Étape F : `Logger`](#7-étape-f--logger)
8. [Étape G : installation, README, CHANGELOG](#8-étape-g--installation-readme-changelog)
9. [Étape H : cpp-core remplace du vrai code dans `main.cpp`](#9-étape-h--cpp-core-remplace-du-vrai-code-dans-maincpp)
10. [Étape I : vérifier tout le parcours](#10-étape-i--vérifier-tout-le-parcours)
11. [Étape J : commiter](#11-étape-j--commiter)
12. [Questions](#12-questions)
13. [Correction](#13-correction)

---

## 0. Ce qu'on fait, et le résultat attendu

cpp-core est la **couche 0** du planning : toutes les autres libs en dépendront. Elle
fixe trois conventions **avant** qu'on écrive cpp-pg, cpp-validation, etc. :

| Brique | Rôle | Remplace |
|---|---|---|
| `Error`, `Result<T>` | une manière unique de dire « ça a échoué, et pourquoi » | exceptions, `bool`, `std::optional` mélangés selon les fichiers |
| `ConfigReader` | lire les variables d'environnement et les secrets Docker, et rapporter **toutes** les erreurs d'un coup | les 4 fonctions `required...EnvironmentVariable` de `main.cpp` |
| `ILogger`, `StreamLogger` | des logs à niveaux, sur une ligne, lisibles par une machine, sans mot de passe | les `std::cerr << "[ERROR BDD] ..."` |

**Pourquoi lundi** : si cpp-pg renvoie des exceptions et cpp-validation des `bool`, il
faudra réécrire leurs signatures vendredi. On décide la forme des erreurs une fois, ici.

```
backend/libs/
├── CMakeLists.txt              ← add_subdirectory(cpp-core) AVANT cpp-auth
└── cpp-core/                   ← généré par new-lib.sh, puis complété
    ├── include/core/
    │   ├── Version.hpp         (du gabarit)
    │   ├── Error.hpp           ErrorCode, Error
    │   ├── Result.hpp          Result<T> = std::expected<T, Error>, fail()
    │   ├── Environment.hpp     IEnvironment, SystemEnvironment, MapEnvironment
    │   ├── ConfigReader.hpp    lecture de la config, erreurs groupées
    │   └── Logger.hpp          ILogger, StreamLogger
    ├── src/                    un .cpp par en-tête (sauf Result, tout en en-tête)
    └── tests/                  VersionTests + 3 nouveaux fichiers de tests
```

À la fin :

| Vérification | Résultat attendu |
|---|---|
| `ctest --preset debug` / `make test` | 193 tests (174 + 19 de cpp-core) |
| `make sanitize` | 193 tests |
| cpp-core seule, installée | 11 fichiers, package-consumer `OK` |
| serveur lancé sans config | **une** ligne d'erreur qui liste tout ce qui manque |
| logs du serveur | `level=INFO msg="Server starting" port=18080` |

---

## 1. Les notions à connaître avant de commencer

### 1.1 Trois façons de signaler une erreur en C++

```cpp
Account find(int id);                    // 1. lève une exception si absent
std::optional<Account> find(int id);     // 2. vide si absent… mais pourquoi ?
bool find(int id, Account &out);         // 3. false si absent, résultat en paramètre
```

| Façon | Problème |
|---|---|
| Exception | **invisible** dans la signature : rien n'oblige l'appelant à la gérer. Une exception oubliée remonte jusqu'à `main` et arrête le serveur. Et elle coûte cher quand elle est levée |
| `std::optional` | dit « rien », mais pas **pourquoi** : introuvable ? base indisponible ? |
| `bool` + paramètre de sortie | facile à ignorer, illisible, l'objet de sortie doit exister avant |

On garde les exceptions pour ce qui est **vraiment exceptionnel** (mémoire épuisée, bug)
et on renvoie les erreurs **attendues** (compte introuvable, email déjà pris, base
injoignable) dans la valeur de retour.

### 1.2 `std::expected<T, E>` (C++23)

`std::expected<T, E>` contient **soit** une valeur `T`, **soit** une erreur `E`. C'est le
`Result` de Rust, entré dans le standard en C++23 (GCC 12 l'a, avec `-std=c++23`).

```cpp
std::expected<int, std::string> r = 42;                       // contient une valeur
std::expected<int, std::string> e = std::unexpected("boom");  // contient une erreur

if (r)                 // true si valeur  (aussi : r.has_value())
    use(*r);           // la valeur       (aussi : r.value(), r->membre)
else
    log(r.error());    // l'erreur
int n = e.value_or(0); // la valeur, ou 0 si erreur
```

- `std::unexpected(x)` : la seule façon de construire une erreur. Sans elle, `return x;`
  serait ambigu quand `T` et `E` se ressemblent.
- `std::expected<void, E>` existe : « ça a marché, rien à renvoyer », ou une erreur.
- ⚠️ `*r` sur un `expected` qui contient une erreur est un **comportement indéfini**. On
  teste toujours `if (r)` avant (ou on utilise `r.value()`, qui lève une exception).

cpp-core fixe `E` une fois pour toutes : `Result<T>` = `std::expected<T, core::Error>`.

### 1.3 La configuration par variables d'environnement

Le backend lit sa configuration dans l'**environnement** du processus (`DB_HOST`,
`JWT_ISSUER`…), que `docker-compose.yml` remplit. C'est la règle des applications
« *twelve-factor* » : le même binaire tourne en dev et en prod, seule la configuration
change.

Les **secrets** (mots de passe) suivent une convention de Docker : au lieu de
`DB_PASSWORD=...` (visible avec `docker inspect`), on passe `DB_PASSWORD_FILE=/run/secrets/db_password`,
et le programme lit le fichier. L'image officielle de PostgreSQL fait pareil. Notre
`ConfigReader` accepte les deux formes, et refuse si les deux sont présentes.

**Deux règles qu'on va coder** :

1. **Échouer au démarrage** (*fail fast*) : une config fausse doit arrêter le serveur
   tout de suite, pas au premier email envoyé à 3 h du matin.
2. **Tout rapporter d'un coup** : aujourd'hui, `main.cpp` lève une exception à la
   **première** variable manquante. Tu corriges, relances, et découvres la suivante.
   `ConfigReader` **note** chaque erreur, renvoie une valeur neutre (`""`, `0`) pour que
   la lecture continue, et `finish()` les rapporte toutes.

### 1.4 Une interface pour pouvoir tester : `IEnvironment`

`ConfigReader` ne lit pas l'environnement avec `std::getenv` directement : il le demande à
un `IEnvironment`. En prod, on lui donne un `SystemEnvironment` (qui appelle `getenv`) ;
dans les tests, un `MapEnvironment` (une simple table).

C'est le même principe que `IClock` dans cpp-auth : on **injecte** la dépendance au
monde extérieur. Les tests n'ont pas besoin de modifier les variables du processus, ce qui
serait partagé entre tous les tests et source d'interférences.

### 1.5 Des logs structurés : *logfmt*

Un log utile se lit par un humain **et** se filtre par une machine (`grep`, un outil de
supervision). On écrit chaque message sur **une ligne**, en paires `clé=valeur`. C'est le
format *logfmt* :

```
level=ERROR msg="Database initialization failed"
level=INFO msg=login user=alice request_id=7f3a
```

- **niveaux** (`DEBUG` < `INFO` < `WARNING` < `ERROR`) : on règle un niveau minimum, tout
  ce qui est en dessous est ignoré ;
- **champs** : `request_id=7f3a` permettra (avec le middleware du dimanche) de retrouver
  toutes les lignes d'une même requête avec `grep request_id=7f3a` ;
- **masquage** : un champ dont la clé contient `password`, `token` ou `secret` est
  remplacé par `***`. Un log finit souvent copié dans un ticket ou un outil externe : un
  mot de passe n'a rien à y faire ;
- **injection de logs** : si un utilisateur s'inscrit avec le nom
  `alice\nlevel=ERROR msg="disk full"`, un logger naïf écrirait **deux** lignes, dont une
  fausse. On échappe donc les retours à la ligne (`\n` devient les deux caractères `\`
  `n`) ;
- **pas d'horodatage** : Docker ajoute déjà l'heure de chaque ligne
  (`docker compose logs -t`). L'écrire nous-mêmes ferait doublon.

Le serveur Crow tourne sur plusieurs threads : deux messages écrits en même temps
pourraient se mélanger au milieu d'une ligne. `StreamLogger` construit la ligne entière,
puis l'écrit sous un `std::mutex`.

### 1.6 Ce qui reste hors de cpp-core

- Les `std::cout << "[INFO BDD] ..."` de `Database.cpp` : ils partiront avec cpp-pg
  (demain), qui recevra un `ILogger`.
- Les autres `std::cerr` des routes : remplacés au fil des guides, quand chaque partie
  recevra son `ILogger`.
- Déplacer `IClock` de cpp-auth vers cpp-core : tâche **bonus** du planning, pas faite
  ici.

---

## 2. Étape A : réparer le gabarit, puis partir propre

### 2.1 Un reste du guide 02

Ouvre `backend/libs/_template/tests/package-consumer/CMakeLists.txt` :

```cmake
find_package(cpp-template 1.0 CONFIG REQUIRED)
```

C'est le `1.0` du §9.3 du guide 02 (« casser exprès »), qui n'a pas été remis à `0.1`
avant le commit. `new-lib.sh` le recopierait dans cpp-core, et son package-consumer
échouerait : la lib est en `0.1.0`, majeur différent (guide 02, Q4).

Remets `0.1` :

```cmake
find_package(cpp-template 0.1 CONFIG REQUIRED)
```

Le gabarit n'est pas compilé par `make test` (guide 02, §1.9) : c'est exactement pour ça
que l'erreur est passée inaperçue.

### 2.2 Commit et branche

Depuis `~/matcha` (ce guide a déjà été commité avec le guide 03) :

```sh
git status
```

✅ **Attendu** : une seule ligne, ta correction.

```
modified:   backend/libs/_template/tests/package-consumer/CMakeLists.txt
```

Un commit à part, puis la branche (à partir de `build/presets`, où tu es) :

```sh
git add backend/libs/_template/tests/package-consumer/CMakeLists.txt
git commit -m "libs/_template : le package-consumer redemande la version 0.1"
git switch -c libs/cpp-core
```

Puis les deux terminaux habituels (`make up`, `make shell`).

---

## 3. Étape B : générer cpp-core et la brancher

### 3.1 Générer

Dans le conteneur (`/app`) :

```sh
sh libs/new-lib.sh cpp-core core
```

✅ **Attendu** :

```
Créé : libs/cpp-core
Reste à faire : ajouter add_subdirectory(cpp-core) dans libs/CMakeLists.txt
```

Le namespace est `core` : court, et il se lit bien (`core::Result`, `core::ConfigReader`).

### 3.2 La déclarer, au bon endroit

Dans `backend/libs/CMakeLists.txt`, ajoute la ligne **avant** `cpp-auth` :

```cmake
add_subdirectory(cpp-core)
add_subdirectory(cpp-auth)
```

C'est la réponse à la question 8 du guide 02 : cpp-core est en couche 0, et cpp-auth en
dépendra (tâche bonus « IClock »). On déclare donc les couches du bas vers le haut. Si un
jour cpp-auth teste `if(NOT TARGET cpp-core::cpp-core)`, l'ordre sera déjà bon.

### 3.3 Activer ses tests en dev

Dans `backend/CMakePresets.json`, preset `debug`, ajoute l'option de cpp-core à côté de
celle de cpp-auth (attention à la **virgule** en fin de ligne précédente, obligatoire en
JSON) :

```json
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CPP_AUTH_BUILD_TESTS": "ON",
                "CPP_CORE_BUILD_TESTS": "ON"
            }
```

Comme cpp-auth, cpp-core désactive ses tests quand elle est un sous-projet (gabarit,
guide 02). C'est le preset de dev qui les réactive. `sanitize` hérite de `debug` : il les
aura aussi.

### 3.4 Vérifier

```sh
cmake --preset debug
grep CPP_CORE_BUILD_TESTS build/debug/CMakeCache.txt
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R Version
```

✅ **Attendu** : `CPP_CORE_BUILD_TESTS:BOOL=ON`, puis
`100% tests passed, 0 tests failed out of 1`. C'est le test de version du gabarit,
devenu celui de cpp-core.

---

## 4. Étape C : `Error` et `Result`

### 4.1 `include/core/Error.hpp`

Crée `backend/libs/cpp-core/include/core/Error.hpp` :

```cpp
#pragma once
#include <string>
#include <string_view>

namespace core
{
	// Catégories d'erreur communes à toutes les libs. La couche qui parle à
	// l'extérieur (HTTP, ligne de commande...) choisit comment les traduire.
	enum class ErrorCode
	{
		Invalid,
		NotFound,
		Conflict,
		Unauthorized,
		Forbidden,
		Unavailable,
		Internal
	};

	struct Error
	{
		ErrorCode	code;
		std::string	message;
	};

	std::string_view	toString(ErrorCode code) noexcept;
}
```

**Pourquoi ces sept codes** : ce sont les **catégories** dont l'appelant a besoin pour
réagir, pas la liste de toutes les erreurs possibles.

| Code | Sens | Exemple | Deviendra en HTTP |
|---|---|---|---|
| `Invalid` | la demande est mal formée | email sans `@` | 400 |
| `NotFound` | ce qu'on cherche n'existe pas | compte 42 | 404 |
| `Conflict` | contraire à l'état actuel | email déjà utilisé | 409 |
| `Unauthorized` | on ne sait pas qui tu es | jeton expiré | 401 |
| `Forbidden` | on sait qui tu es, c'est non | modifier le profil d'un autre | 403 |
| `Unavailable` | temporaire, réessayer | base injoignable, JWKS en panne | 503 |
| `Internal` | bug | cas « impossible » | 500 |

La colonne HTTP n'est **pas** dans cpp-core : une lib ne connaît pas HTTP. C'est
l'adaptateur Crow (dimanche) qui fera la traduction. `message` est pour les **logs** et
les développeurs, pas forcément pour l'utilisateur final.

`enum class` (et pas `enum`) : les noms restent dans leur portée (`ErrorCode::Invalid`,
pas `Invalid` tout court), et pas de conversion silencieuse en `int`.

### 4.2 `src/Error.cpp`

```cpp
#include <core/Error.hpp>

std::string_view	core::toString(ErrorCode code) noexcept
{
	switch (code)
	{
		case ErrorCode::Invalid:		return "invalid";
		case ErrorCode::NotFound:		return "not_found";
		case ErrorCode::Conflict:		return "conflict";
		case ErrorCode::Unauthorized:	return "unauthorized";
		case ErrorCode::Forbidden:		return "forbidden";
		case ErrorCode::Unavailable:	return "unavailable";
		case ErrorCode::Internal:		return "internal";
	}
	return "internal";
}
```

- **Pas de `default:`** dans le `switch` : si un jour tu ajoutes un code à l'`enum` sans
  l'ajouter ici, `-Wall` (`-Wswitch`) le signale, et `-Werror` bloque la compilation. Un
  `default:` ferait taire cet avertissement utile.
- Le `return` après le `switch` : un `enum class` peut, en théorie, contenir une valeur
  hors liste (`static_cast<ErrorCode>(99)`). Sans ce `return`, `-Wreturn-type` râlerait.
- `string_view` sur une chaîne littérale : sans risque, une littérale vit pendant tout le
  programme.

### 4.3 `include/core/Result.hpp`

```cpp
#pragma once
#include <core/Error.hpp>
#include <expected>
#include <string>
#include <utility>

namespace core
{
	// Une valeur T, ou une Error expliquant pourquoi il n'y en a pas.
	template <class T>
	using Result = std::expected<T, Error>;

	// return core::fail(core::ErrorCode::NotFound, "account not found");
	inline std::unexpected<Error>	fail(ErrorCode code, std::string message)
	{
		return std::unexpected<Error>(Error{code, std::move(message)});
	}
}
```

- `template <class T> using Result = ...` : un **alias de template**. `core::Result<int>`
  est exactement `std::expected<int, core::Error>`, sans nouveau type à apprendre.
- `fail(...)` : raccourci pour éviter d'écrire
  `std::unexpected<core::Error>(core::Error{...})` à chaque erreur.
- `inline` : la fonction est **définie** dans un en-tête, donc dans chaque `.cpp` qui
  l'inclut. `inline` autorise ces définitions multiples (sinon, erreur d'édition de liens
  « *multiple definition* »). Compare avec le guide 03 §4.4 : là, le namespace anonyme
  créait une copie **privée** par fichier ; ici, `inline` dit « c'est la même fonction
  partout ».
- Pas de `Result.cpp` : il n'y a rien à compiler à part.

### 4.4 Les tests : `tests/ResultTests.cpp`

```cpp
#include <core/Result.hpp>
#include <gtest/gtest.h>

namespace
{
	core::Result<int>	parsePositive(int value)
	{
		if (value <= 0)
			return core::fail(core::ErrorCode::Invalid, "must be positive");
		return value;
	}
}

TEST(Result, HoldsValueOnSuccess)
{
	const auto result = parsePositive(42);
	ASSERT_TRUE(result);
	EXPECT_EQ(*result, 42);
}

TEST(Result, HoldsErrorOnFailure)
{
	const auto result = parsePositive(-1);
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().code, core::ErrorCode::Invalid);
	EXPECT_EQ(result.error().message, "must be positive");
}

TEST(Result, VoidResultHasNoValue)
{
	const core::Result<void> ok{};
	const core::Result<void> ko = core::fail(core::ErrorCode::Internal, "boom");
	EXPECT_TRUE(ok);
	EXPECT_FALSE(ko);
}

TEST(Error, CodesHaveStableNames)
{
	EXPECT_EQ(core::toString(core::ErrorCode::NotFound), "not_found");
	EXPECT_EQ(core::toString(core::ErrorCode::Unavailable), "unavailable");
}
```

- `parsePositive` montre l'usage typique : `return value;` construit un `Result` qui
  contient la valeur, `return core::fail(...)` un `Result` qui contient l'erreur.
- `ASSERT_TRUE` (et pas `EXPECT_TRUE`) avant `*result` : si l'assertion échoue, `ASSERT`
  **arrête** le test. Avec `EXPECT`, le test continuerait et ferait `*result` sur une
  erreur : comportement indéfini (§1.2). Règle : `ASSERT` quand la suite du test en
  dépend.
- `core::Result<void> ok{}` : un `expected<void, E>` construit par défaut contient « le
  succès ».

### 4.5 Les déclarer dans le `CMakeLists.txt`

Dans `backend/libs/cpp-core/CMakeLists.txt`, ajoute le fichier source à la lib :

```cmake
add_library(cpp-core STATIC
	src/Version.cpp
	src/Error.cpp
)
```

et le fichier de test à l'exécutable de tests :

```cmake
    add_executable(cpp-core_tests
        tests/VersionTests.cpp
        tests/ResultTests.cpp
    )
```

Tu compléteras ces deux listes à chaque étape. Vérifie :

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R "^(Result|Error)\."
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 4`.

`-R` prend une **expression régulière** comparée au nom complet `Suite.Test`. Sans le
`^` (début du nom) et le `\.`, `Error` attraperait aussi six tests de cpp-auth, comme
`AuthServiceTest.LoginReturnsInternalErrorWhenRepositoryFails`.

---

## 5. Étape D : `Environment`

### 5.1 `include/core/Environment.hpp`

```cpp
#pragma once
#include <map>
#include <optional>
#include <string>

namespace core
{
	// D'où viennent les variables d'environnement : le vrai environnement du
	// processus, ou une table en mémoire dans les tests.
	class IEnvironment
	{
		public:
			virtual ~IEnvironment() = default;
			// std::nullopt si la variable n'existe pas ou est vide.
			virtual std::optional<std::string>	get(const std::string &name) const = 0;
	};

	class SystemEnvironment : public IEnvironment
	{
		public:
			std::optional<std::string>	get(const std::string &name) const override;
	};

	class MapEnvironment : public IEnvironment
	{
		public:
			void						set(const std::string &name, std::string value);
			std::optional<std::string>	get(const std::string &name) const override;

		private:
			std::map<std::string, std::string>	_values;
	};
}
```

- `virtual ~IEnvironment() = default;` : obligatoire dans une classe de base polymorphe.
  Sans destructeur virtuel, détruire un `SystemEnvironment` via un pointeur
  `IEnvironment*` serait un comportement indéfini.
- `= 0` : méthode **virtuelle pure**, `IEnvironment` est une interface qu'on ne peut pas
  instancier.
- **« Vide » = « absent »** : `docker-compose.yml` produit souvent des variables vides
  (`${X:-}`). Pour la config, une variable vide n'a jamais de sens : on la traite comme
  absente, à un seul endroit.
- **Pourquoi `set()` et pas un constructeur qui prend une `std::map`** : avec un tel
  constructeur, `MapEnvironment env({{"HOST", "db"}});` ne compile pas : GCC répond
  `call of overloaded 'MapEnvironment(<brace-enclosed initializer list>)' is ambiguous`.
  Avec **une seule** paire, `{"HOST", "db"}` peut aussi se lire comme deux `const char*`,
  donc un **intervalle d'itérateurs** (constructeur `map(first, last)`). Le piège classique
  des accolades. `set()` évite toute ambiguïté et se lit mieux.

### 5.2 `src/Environment.cpp`

```cpp
#include <core/Environment.hpp>
#include <cstdlib>
#include <utility>

std::optional<std::string>	core::SystemEnvironment::get(const std::string &name) const
{
	const char *value = std::getenv(name.c_str());
	if (value == nullptr || *value == '\0')
		return std::nullopt;
	return std::string{value};
}

void	core::MapEnvironment::set(const std::string &name, std::string value)
{
	_values[name] = std::move(value);
}

std::optional<std::string>	core::MapEnvironment::get(const std::string &name) const
{
	const auto found = _values.find(name);
	if (found == _values.end() || found->second.empty())
		return std::nullopt;
	return found->second;
}
```

- `std::getenv` renvoie `nullptr` si la variable n'existe pas. On ne garde **jamais** le
  pointeur : on copie dans une `std::string`. La mémoire pointée appartient au système et
  peut changer au prochain `setenv`.
- `set(..., std::string value)` puis `std::move` : le paramètre est pris **par valeur**
  puis déplacé. Un appel avec une littérale ou une temporaire ne fait qu'une seule copie.
- `_values.find(...)` et pas `_values[name]` dans `get` : `operator[]` **insère** une
  entrée vide si la clé manque, et n'existe pas sur une `map` `const`.

Ajoute `src/Environment.cpp` à `add_library`. Pas de test séparé : `ConfigReaderTests`
les exerce (§6.3), y compris `SystemEnvironment`.

---

## 6. Étape E : `ConfigReader`

### 6.1 `include/core/ConfigReader.hpp`

```cpp
#pragma once
#include <core/Environment.hpp>
#include <core/Result.hpp>
#include <string>
#include <vector>

namespace core
{
	// Lit la configuration au démarrage. Une variable absente ou invalide ne
	// lève pas d'exception : l'erreur est notée, une valeur neutre est renvoyée,
	// et finish() les rapporte toutes ensemble.
	class ConfigReader
	{
		public:
			explicit ConfigReader(const IEnvironment &environment);

			std::string	required(const std::string &name);
			std::string	optional(const std::string &name, const std::string &fallback);
			int			requiredInt(const std::string &name);
			bool		optionalBool(const std::string &name, bool fallback);
			// Lit NAME, ou le contenu du fichier désigné par NAME_FILE (secrets Docker).
			std::string	requiredSecret(const std::string &name);

			Result<void>	finish() const;

		private:
			const IEnvironment			&_environment;
			std::vector<std::string>	_errors;
	};
}
```

- `explicit` : empêche une conversion implicite d'un `IEnvironment` en `ConfigReader`.
  Règle simple : tout constructeur à un seul paramètre est `explicit`, sauf raison
  précise.
- `const IEnvironment &` : une **référence**. `ConfigReader` ne possède pas
  l'environnement, il l'utilise. L'environnement doit donc **vivre plus longtemps** que
  le `ConfigReader` (on le verra dans `main.cpp`).
- Les getters **ne sont pas** `const` : ils ajoutent des erreurs dans `_errors`.
- Seulement les getters dont le backend a besoin aujourd'hui. Pas d'`optionalInt` ni de
  `requiredDouble` « au cas où » : on les ajoutera quand un vrai code en aura besoin.

### 6.2 `src/ConfigReader.cpp`

```cpp
#include <core/ConfigReader.hpp>
#include <charconv>
#include <fstream>
#include <iterator>

core::ConfigReader::ConfigReader(const IEnvironment &environment)
	: _environment(environment)
{
}

std::string	core::ConfigReader::required(const std::string &name)
{
	const auto value = _environment.get(name);
	if (!value)
	{
		_errors.push_back(name + " is required");
		return "";
	}
	return *value;
}

std::string	core::ConfigReader::optional(const std::string &name, const std::string &fallback)
{
	return _environment.get(name).value_or(fallback);
}

int	core::ConfigReader::requiredInt(const std::string &name)
{
	const auto value = _environment.get(name);
	if (!value)
	{
		_errors.push_back(name + " is required");
		return 0;
	}
	int number = 0;
	const char *end = value->data() + value->size();
	const auto [stop, error] = std::from_chars(value->data(), end, number);
	if (error != std::errc{} || stop != end)
	{
		_errors.push_back(name + " must be an integer, got '" + *value + "'");
		return 0;
	}
	return number;
}

bool	core::ConfigReader::optionalBool(const std::string &name, bool fallback)
{
	const auto value = _environment.get(name);
	if (!value)
		return fallback;
	if (*value == "true")
		return true;
	if (*value == "false")
		return false;
	_errors.push_back(name + " must be 'true' or 'false', got '" + *value + "'");
	return fallback;
}

std::string	core::ConfigReader::requiredSecret(const std::string &name)
{
	const std::string fileVariable = name + "_FILE";
	const auto direct = _environment.get(name);
	const auto path = _environment.get(fileVariable);
	if (direct && path)
	{
		_errors.push_back(name + " and " + fileVariable + " are both set, keep only one");
		return "";
	}
	if (direct)
		return *direct;
	if (!path)
	{
		_errors.push_back(name + " or " + fileVariable + " is required");
		return "";
	}
	std::ifstream file(*path, std::ios::binary);
	if (!file)
	{
		_errors.push_back(fileVariable + ": cannot read " + *path);
		return "";
	}
	std::string secret{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>()
	};
	while (!secret.empty() && (secret.back() == '\n' || secret.back() == '\r'))
		secret.pop_back();
	if (secret.empty())
		_errors.push_back(fileVariable + ": " + *path + " is empty");
	return secret;
}

core::Result<void>	core::ConfigReader::finish() const
{
	if (_errors.empty())
		return {};
	std::string message;
	for (const auto &error : _errors)
	{
		if (!message.empty())
			message += "; ";
		message += error;
	}
	return fail(ErrorCode::Invalid, message);
}
```

Les points qui méritent une explication :

- **`std::from_chars` plutôt que `std::stoi`** (utilisé aujourd'hui dans `main.cpp`) :
  - `std::stoi("63x")` renvoie `63` sans rien dire : il s'arrête au premier caractère
    non chiffre. `from_chars` renvoie dans `stop` l'endroit où il s'est arrêté : on
    exige `stop == end`, donc **tout** le texte doit être un nombre ;
  - `std::stoi` lève une exception en cas d'échec, `from_chars` renvoie un code d'erreur
    (`error != std::errc{}`), ce qui colle à notre principe (§1.3) ;
  - `const auto [stop, error] = ...` : une *structured binding*, qui découpe la structure
    renvoyée (`ptr` et `ec`) en deux variables nommées.
- **`optionalBool` strict** : `"yes"`, `"1"`, `"TRUE"` sont refusés. Une faute de frappe
  dans `AUTH_REFRESH_COOKIE_SECURE=ture` ne doit pas désactiver silencieusement une
  sécurité. On garde la règle actuelle de `main.cpp`.
- **`requiredSecret`** reprend `requiredSecretFileEnvironmentVariable` de `main.cpp` :
  - lecture du fichier entier avec deux `istreambuf_iterator` (début, fin) ;
  - suppression des `\n` et `\r` finaux : `echo motdepasse > fichier` ajoute un `\n`,
    qui ferait partie du mot de passe ;
  - `std::ios::binary` : on lit les octets tels quels.
- **Les messages d'erreur contiennent la valeur fautive** (`got '63x'`), mais **jamais**
  le contenu d'un secret : seulement le chemin du fichier.
- `finish()` est `const` : il lit `_errors` sans le modifier. Il renvoie un
  `Result<void>` : succès sans valeur, ou une `Error` `Invalid` qui liste tout.

### 6.3 Les tests : `tests/ConfigReaderTests.cpp`

```cpp
#include <core/ConfigReader.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
	std::filesystem::path	writeFile(const std::string &name, const std::string &content)
	{
		const auto path = std::filesystem::temp_directory_path() / name;
		std::ofstream(path, std::ios::binary) << content;
		return path;
	}
}

TEST(ConfigReader, ReadsRequiredValue)
{
	core::MapEnvironment env;
	env.set("HOST", "db");
	core::ConfigReader config(env);
	EXPECT_EQ(config.required("HOST"), "db");
	EXPECT_TRUE(config.finish());
}

TEST(ConfigReader, ReportsEveryMissingVariableAtOnce)
{
	core::MapEnvironment env;
	env.set("EMPTY", "");
	core::ConfigReader config(env);
	config.required("FIRST");
	config.required("EMPTY");
	config.requiredInt("PORT");
	const auto result = config.finish();
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().code, core::ErrorCode::Invalid);
	EXPECT_EQ(result.error().message,
		"FIRST is required; EMPTY is required; PORT is required");
}

TEST(ConfigReader, OptionalFallsBackToDefault)
{
	core::MapEnvironment env;
	core::ConfigReader config(env);
	EXPECT_EQ(config.optional("NAME", "matcha"), "matcha");
	EXPECT_TRUE(config.finish());
}

TEST(ConfigReader, ParsesIntegers)
{
	core::MapEnvironment env;
	env.set("OK", "6379");
	env.set("BAD", "63x");
	core::ConfigReader config(env);
	EXPECT_EQ(config.requiredInt("OK"), 6379);
	config.requiredInt("BAD");
	EXPECT_EQ(config.finish().error().message, "BAD must be an integer, got '63x'");
}

TEST(ConfigReader, ParsesBooleansStrictly)
{
	core::MapEnvironment env;
	env.set("ON", "true");
	env.set("OFF", "false");
	env.set("BAD", "yes");
	core::ConfigReader config(env);
	EXPECT_TRUE(config.optionalBool("ON", false));
	EXPECT_FALSE(config.optionalBool("OFF", true));
	EXPECT_TRUE(config.optionalBool("MISSING", true));
	config.optionalBool("BAD", false);
	EXPECT_EQ(config.finish().error().message, "BAD must be 'true' or 'false', got 'yes'");
}

TEST(ConfigReader, ReadsSecretFromVariable)
{
	core::MapEnvironment env;
	env.set("DB_PASSWORD", "direct");
	core::ConfigReader config(env);
	EXPECT_EQ(config.requiredSecret("DB_PASSWORD"), "direct");
	EXPECT_TRUE(config.finish());
}

TEST(ConfigReader, ReadsSecretFromFileWithoutTrailingNewline)
{
	const auto path = writeFile("cpp-core-secret", "s3cret\n");
	core::MapEnvironment env;
	env.set("DB_PASSWORD_FILE", path.string());
	core::ConfigReader config(env);
	EXPECT_EQ(config.requiredSecret("DB_PASSWORD"), "s3cret");
	EXPECT_TRUE(config.finish());
}

TEST(ConfigReader, RejectsBothSecretSources)
{
	core::MapEnvironment env;
	env.set("DB_PASSWORD", "a");
	env.set("DB_PASSWORD_FILE", "/x");
	core::ConfigReader config(env);
	config.requiredSecret("DB_PASSWORD");
	EXPECT_FALSE(config.finish());
}

TEST(ConfigReader, RejectsMissingOrEmptySecretFile)
{
	const auto empty = writeFile("cpp-core-empty-secret", "\n");
	core::MapEnvironment env;
	env.set("A_FILE", "/nonexistent/secret");
	env.set("B_FILE", empty.string());
	core::ConfigReader config(env);
	config.requiredSecret("A");
	config.requiredSecret("B");
	config.requiredSecret("C");
	EXPECT_EQ(config.finish().error().message,
		"A_FILE: cannot read /nonexistent/secret; B_FILE: " + empty.string() +
		" is empty; C or C_FILE is required");
}

TEST(SystemEnvironment, ReadsProcessEnvironment)
{
	::setenv("CPP_CORE_TEST_VARIABLE", "value", 1);
	::setenv("CPP_CORE_TEST_EMPTY", "", 1);
	const core::SystemEnvironment env;
	EXPECT_EQ(env.get("CPP_CORE_TEST_VARIABLE"), "value");
	EXPECT_EQ(env.get("CPP_CORE_TEST_EMPTY"), std::nullopt);
	EXPECT_EQ(env.get("CPP_CORE_TEST_MISSING"), std::nullopt);
}
```

- Chaque test a son propre `MapEnvironment` : aucun état partagé, l'ordre des tests n'a
  pas d'importance.
- `writeFile` écrit dans le dossier temporaire du système (`/tmp`). `std::ofstream(...)
  << content;` : le flux temporaire est fermé (et le fichier écrit) à la fin de la ligne.
- `ReportsEveryMissingVariableAtOnce` est **le** test de la règle n°2 du §1.3.
- `SystemEnvironment` est testé avec `setenv`, sur des noms préfixés `CPP_CORE_TEST_` qui
  ne gênent personne. C'est le seul test qui touche au vrai environnement.

Ajoute `src/ConfigReader.cpp` à `add_library` et `tests/ConfigReaderTests.cpp` à
`cpp-core_tests`, puis :

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R "ConfigReader|SystemEnvironment"
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 10`.

---

## 7. Étape F : `Logger`

### 7.1 `include/core/Logger.hpp`

```cpp
#pragma once
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace core
{
	enum class LogLevel
	{
		Debug,
		Info,
		Warning,
		Error
	};

	struct LogField
	{
		std::string	key;
		std::string	value;
	};

	using LogFields = std::vector<LogField>;

	class ILogger
	{
		public:
			virtual ~ILogger() = default;
			virtual void	log(LogLevel level, std::string_view message, const LogFields &fields) = 0;

			void	debug(std::string_view message, const LogFields &fields = {});
			void	info(std::string_view message, const LogFields &fields = {});
			void	warning(std::string_view message, const LogFields &fields = {});
			void	error(std::string_view message, const LogFields &fields = {});
	};

	// Une ligne logfmt par message : level=INFO msg="..." key=value
	// Les champs dont la clé contient password, token ou secret sont masqués.
	class StreamLogger : public ILogger
	{
		public:
			StreamLogger(std::ostream &out, LogLevel minimum);
			void	log(LogLevel level, std::string_view message, const LogFields &fields) override;

		private:
			std::ostream	&_out;
			LogLevel		_minimum;
			std::mutex		_mutex;
	};
}
```

- **Une seule méthode virtuelle**, `log`. `debug`/`info`/`warning`/`error` sont des
  raccourcis **non virtuels** qui l'appellent. Une implémentation (un futur logger JSON,
  un faux logger de test) n'a qu'une méthode à écrire.
- **Les valeurs par défaut (`= {}`) sont sur les raccourcis, pas sur `log`**. Un
  paramètre par défaut sur une méthode virtuelle est un piège connu : la valeur utilisée
  dépend du type **statique** de l'objet, pas de la classe réellement appelée.
- `LogFields` est un `std::vector`, donc on peut écrire `{{"port", "18080"}}` à l'appel :
  un vecteur d'un `LogField` construit par accolades.
- `enum class LogLevel` déclaré dans l'ordre de gravité : `level < _minimum` compare
  directement les niveaux (les `enum class` se comparent entre eux).
- **Pourquoi `StreamLogger` et pas `StderrLogger`** (nom du planning) : il écrit dans
  n'importe quel `std::ostream`. Le backend lui donne `std::cerr`, les tests un
  `std::ostringstream`, dont on lit le contenu. Même idée que `IEnvironment`.
- `std::mutex` rend `StreamLogger` **non copiable** : normal, deux copies d'un logger
  n'auraient pas de sens.

### 7.2 `src/Logger.cpp`

```cpp
#include <core/Logger.hpp>
#include <algorithm>
#include <array>
#include <cctype>

namespace
{
	std::string_view	levelName(core::LogLevel level)
	{
		switch (level)
		{
			case core::LogLevel::Debug:		return "DEBUG";
			case core::LogLevel::Info:		return "INFO";
			case core::LogLevel::Warning:	return "WARNING";
			case core::LogLevel::Error:		return "ERROR";
		}
		return "ERROR";
	}

	bool	isSensitive(std::string key)
	{
		std::transform(key.begin(), key.end(), key.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		constexpr std::array<std::string_view, 3> words{"password", "token", "secret"};
		return std::any_of(words.begin(), words.end(),
			[&key](std::string_view word) { return key.find(word) != std::string::npos; });
	}

	// Entre guillemets si besoin, et sans retour à la ligne : un message ne
	// peut pas fabriquer une fausse ligne de log.
	std::string	quote(std::string_view value)
	{
		const bool plain = !value.empty() && std::all_of(value.begin(), value.end(),
			[](unsigned char c) { return std::isgraph(c) && c != '"' && c != '=' && c != '\\'; });
		if (plain)
			return std::string{value};
		std::string out = "\"";
		for (const char c : value)
		{
			switch (c)
			{
				case '"':	out += "\\\""; break;
				case '\\':	out += "\\\\"; break;
				case '\n':	out += "\\n"; break;
				case '\r':	out += "\\r"; break;
				case '\t':	out += "\\t"; break;
				default:	out += c;
			}
		}
		return out + "\"";
	}
}

void	core::ILogger::debug(std::string_view message, const LogFields &fields)
{
	log(LogLevel::Debug, message, fields);
}

void	core::ILogger::info(std::string_view message, const LogFields &fields)
{
	log(LogLevel::Info, message, fields);
}

void	core::ILogger::warning(std::string_view message, const LogFields &fields)
{
	log(LogLevel::Warning, message, fields);
}

void	core::ILogger::error(std::string_view message, const LogFields &fields)
{
	log(LogLevel::Error, message, fields);
}

core::StreamLogger::StreamLogger(std::ostream &out, LogLevel minimum)
	: _out(out), _minimum(minimum)
{
}

void	core::StreamLogger::log(LogLevel level, std::string_view message, const LogFields &fields)
{
	if (level < _minimum)
		return;
	std::string line = "level=" + std::string{levelName(level)} + " msg=" + quote(message);
	for (const auto &field : fields)
		line += " " + field.key + "=" + (isSensitive(field.key) ? "***" : quote(field.value));
	line += '\n';
	const std::lock_guard lock(_mutex);
	_out << line << std::flush;
}
```

- **Le namespace anonyme, à sa place** : `levelName`, `isSensitive` et `quote` sont des
  détails privés de ce `.cpp` (la bonne version de ce que tu as corrigé au guide 03).
- `isSensitive` prend `key` **par valeur** : il la modifie (passage en minuscules) sans
  toucher à l'original. Ainsi `DB_SECRET`, `refreshToken` ou `PASSWORD` sont masqués.
- `unsigned char` dans les lambdas : `std::tolower` et `std::isgraph` ont un
  **comportement indéfini** si on leur passe un `char` négatif, ce qui arrive avec les
  octets UTF-8 (`é`). Passer par `unsigned char` est la règle.
- `std::isgraph(c)` : vrai pour un caractère visible, hors espace. Une valeur est écrite
  telle quelle seulement si elle n'a ni espace, ni `"`, ni `=`, ni `\`, ni caractère de
  contrôle. Sinon, entre guillemets et échappée.
- **La ligne entière est construite avant le verrou**, et le verrou ne couvre que
  l'écriture. On garde la section critique la plus courte possible, une règle que tu
  retrouveras dans le hub WebSocket (vendredi).
- `std::lock_guard lock(_mutex);` : verrouille à la construction, déverrouille à la
  destruction (fin de la fonction), même en cas d'exception. C'est du RAII.
- `std::flush` : force l'écriture immédiate. Un log d'erreur juste avant un plantage doit
  sortir, pas rester dans un tampon.

### 7.3 Les tests : `tests/LoggerTests.cpp`

```cpp
#include <core/Logger.hpp>
#include <gtest/gtest.h>
#include <sstream>

TEST(StreamLogger, WritesOneLogfmtLine)
{
	std::ostringstream out;
	core::StreamLogger logger(out, core::LogLevel::Debug);
	logger.info("Server started", {{"port", "18080"}, {"request_id", "abc"}});
	EXPECT_EQ(out.str(), "level=INFO msg=\"Server started\" port=18080 request_id=abc\n");
}

TEST(StreamLogger, SkipsMessagesBelowMinimumLevel)
{
	std::ostringstream out;
	core::StreamLogger logger(out, core::LogLevel::Warning);
	logger.info("hidden");
	logger.error("shown");
	EXPECT_EQ(out.str(), "level=ERROR msg=shown\n");
}

TEST(StreamLogger, EscapesNewlinesAndQuotes)
{
	std::ostringstream out;
	core::StreamLogger logger(out, core::LogLevel::Debug);
	logger.warning("bad\nlevel=ERROR msg=fake", {{"name", "say \"hi\""}});
	EXPECT_EQ(out.str(),
		"level=WARNING msg=\"bad\\nlevel=ERROR msg=fake\" name=\"say \\\"hi\\\"\"\n");
}

TEST(StreamLogger, MasksSensitiveFields)
{
	std::ostringstream out;
	core::StreamLogger logger(out, core::LogLevel::Debug);
	logger.info("login", {
		{"password", "hunter2"},
		{"refresh_token", "abc"},
		{"DB_SECRET", "x"},
		{"user", "alice"}
	});
	EXPECT_EQ(out.str(),
		"level=INFO msg=login password=*** refresh_token=*** DB_SECRET=*** user=alice\n");
}
```

- `EscapesNewlinesAndQuotes` est le test de **l'injection de logs** (§1.5) : le message
  essaie de fabriquer une fausse ligne `level=ERROR`. La sortie ne contient qu'**un**
  `\n`, le dernier.
- Dans une chaîne C++, `\\n` s'écrit pour obtenir les deux caractères `\` `n`, et `\"`
  pour un guillemet. Lis l'attendu lentement : c'est la partie la plus délicate du guide.

Ajoute `src/Logger.cpp` et `tests/LoggerTests.cpp`, puis :

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -R StreamLogger
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 4`.

Au total, cpp-core a maintenant 19 tests (1 + 4 + 10 + 4).

---

## 8. Étape G : installation, README, CHANGELOG

### 8.1 Le package-consumer

Remplace `backend/libs/cpp-core/tests/package-consumer/main.cpp` par :

```cpp
#include <core/Result.hpp>
#include <core/Version.hpp>

int main()
{
	const core::Result<int> answer = 42;
	return (answer && !core::version().empty()) ? 0 : 1;
}
```

Il vérifie, chez un projet extérieur, que `Result.hpp` est installé et que `<expected>`
compile : `cxx_std_23` est une propriété `PUBLIC` de la lib (gabarit, guide 02), donc
exportée dans `cpp-coreTargets.cmake`. Le consommateur reçoit `-std=gnu++23` sans rien
demander.

Puis la procédure de l'étape H du guide 02, avec `$T` (§1.7 du guide 03 : les presets ne
servent qu'au backend) :

```sh
T="-DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=$VCPKG_INSTALLED_DIR -DVCPKG_MANIFEST_INSTALL=OFF"
rm -rf /tmp/core /tmp/core-inst /tmp/core-cons
cmake -S libs/cpp-core -B /tmp/core $T
cmake --build /tmp/core -j"$(nproc)"
ctest --test-dir /tmp/core
cmake --install /tmp/core --prefix /tmp/core-inst
find /tmp/core-inst -type f | sort
cmake -S libs/cpp-core/tests/package-consumer -B /tmp/core-cons $T -DCMAKE_PREFIX_PATH=/tmp/core-inst
cmake --build /tmp/core-cons
/tmp/core-cons/package_consumer && echo OK
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 19` (seule, la lib est le
projet principal : ses tests sont actifs), puis 11 fichiers installés :

```
/tmp/core-inst/include/core/ConfigReader.hpp
/tmp/core-inst/include/core/Environment.hpp
/tmp/core-inst/include/core/Error.hpp
/tmp/core-inst/include/core/Logger.hpp
/tmp/core-inst/include/core/Result.hpp
/tmp/core-inst/include/core/Version.hpp
/tmp/core-inst/lib/cmake/cpp-core/cpp-coreConfig.cmake
/tmp/core-inst/lib/cmake/cpp-core/cpp-coreConfigVersion.cmake
/tmp/core-inst/lib/cmake/cpp-core/cpp-coreTargets-noconfig.cmake
/tmp/core-inst/lib/cmake/cpp-core/cpp-coreTargets.cmake
/tmp/core-inst/lib/libcpp-core.a
```

et enfin `OK`.

**Aucun `find_dependency`** dans `cmake/cpp-coreConfig.cmake.in` : cpp-core n'utilise que
la bibliothèque standard (guide 02, Q3 et Q6).

### 8.2 `README.md`

Remplace le contenu de `backend/libs/cpp-core/README.md` :

````markdown
# cpp-core

> Briques communes aux bibliothèques : erreurs typées (`Result<T>`), lecture de la
> configuration et des secrets, logs structurés.

## Exemple minimal

```cpp
#include <core/ConfigReader.hpp>
#include <core/Logger.hpp>
#include <iostream>

int main()
{
    core::StreamLogger logger(std::cerr, core::LogLevel::Info);
    const core::SystemEnvironment environment;
    core::ConfigReader config(environment);

    const std::string host = config.required("DB_HOST");
    const int port = config.requiredInt("DB_PORT");
    const std::string password = config.requiredSecret("DB_PASSWORD"); // ou DB_PASSWORD_FILE

    if (const auto checked = config.finish(); !checked)
    {
        logger.error("Invalid configuration", {{"errors", checked.error().message}});
        return 1;
    }
    logger.info("Configuration loaded", {{"host", host}, {"password", password}});
    // level=INFO msg="Configuration loaded" host=db password=***
}
```

## Utilisation

Dans le monorepo (`backend/libs/CMakeLists.txt`), avant les libs qui en dépendent :

```cmake
add_subdirectory(cpp-core)
```

Depuis un autre projet, une fois installée :

```cmake
find_package(cpp-core 0.1 CONFIG REQUIRED)
target_link_libraries(mon_app PRIVATE cpp-core::cpp-core)
```

Option : `CPP_CORE_BUILD_TESTS` (ON quand la lib est le projet principal, OFF sinon).
Nécessite C++23 (`std::expected`).

## Garanties

- `Result<T>` est un alias de `std::expected<T, core::Error>` : aucune exception pour les
  erreurs attendues.
- `ConfigReader` ne lève pas d'exception et rapporte toutes les erreurs d'un coup dans
  `finish()`. Une variable vide est traitée comme absente.
- `StreamLogger` est utilisable depuis plusieurs threads : une ligne n'est jamais coupée.
  Les champs dont la clé contient `password`, `token` ou `secret` sont masqués, et les
  retours à la ligne sont échappés.

## Limites

- Pas d'horodatage dans les logs (ajouté par Docker), pas de sortie JSON.
- `ConfigReader` ne lit que les types utilisés par le backend : chaîne, `int`, booléen
  (`true`/`false` uniquement), secret.
- Le masquage se fait sur le **nom** du champ : une valeur sensible sous un nom anodin
  n'est pas détectée.
````

Le bloc ci-dessus est entouré de **quatre** accents graves parce qu'il contient lui-même
des blocs de code. Ton fichier commence à la ligne `# cpp-core` et finit à la dernière
ligne des « Limites ».

### 8.3 `CHANGELOG.md`

Remplace la section `[0.1.0]` :

```markdown
## [0.1.0]

### Added
- `ErrorCode`, `Error` et `Result<T>` (sur `std::expected`), avec `fail()`.
- `IEnvironment`, `SystemEnvironment`, `MapEnvironment`.
- `ConfigReader` : variables requises et optionnelles, entiers, booléens stricts,
  secrets via `NAME` ou `NAME_FILE`, erreurs rapportées ensemble.
- `ILogger` et `StreamLogger` : niveaux, format logfmt, masquage des champs sensibles,
  échappement des retours à la ligne.
```

---

## 9. Étape H : cpp-core remplace du vrai code dans `main.cpp`

Règle du planning : **une bibliothèque n'est finie que si elle remplace du vrai code.** On
remplace les quatre fonctions de lecture de config de `main.cpp` et ses `std::cerr`.

### 9.1 Lier cpp-core

Dans `backend/CMakeLists.txt`, ajoute cpp-core aux bibliothèques de `matcha_backend` :

```cmake
target_link_libraries(matcha_backend
    PRIVATE cpp-core::cpp-core
    PRIVATE cpp-auth::cpp-auth
    ...
```

### 9.2 Les `#include` de `backend/src/main.cpp`

En haut du fichier, dans le groupe des en-têtes standards, **supprime** `<cstdlib>`,
`<fstream>` et `<iterator>` (ils ne servaient qu'aux fonctions qu'on va retirer). Il
reste :

```cpp
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <sw/redis++/redis++.h>
```

Et ajoute, juste avant `#include <auth/auth/Authenticator.hpp>` :

```cpp
#include <core/ConfigReader.hpp>
#include <core/Logger.hpp>

```

### 9.3 Supprimer les quatre fonctions

Supprime **tout** le bloc `namespace { ... }` qui suit les `#include` : il contient
`requiredEnvironmentVariable`, `optionalEnvironmentVariable`,
`optionalBooleanEnvironmentVariable` et `requiredSecretFileEnvironmentVariable`. Le
fichier passe directement des `#include` à `int main()`.

### 9.4 Le début de la fonction : la config lue d'un bloc

Remplace tout le début, de `int main()` jusqu'à la ligne **avant** `Database database;` :

```cpp
int main()
try
{
    const std::string issuer =
        requiredEnvironmentVariable("JWT_ISSUER");
    ...
    const bool secureRefreshCookie =
        optionalBooleanEnvironmentVariable(
            "AUTH_REFRESH_COOKIE_SECURE",
            false
        );

```

par :

```cpp
int run(core::ILogger &logger)
{
    const core::SystemEnvironment environment;
    core::ConfigReader config(environment);

    const std::string issuer = config.required("JWT_ISSUER");
    const std::string audience = config.required("JWT_AUDIENCE");
    const std::string jwksUrl = config.required("JWT_JWKS_URL");
    const std::string privateKeyPath = config.required("JWT_PRIVATE_KEY_PATH");
    const std::string keyId = config.required("JWT_KEY_ID");
    const bool allowInsecureJwksHttp =
        config.optionalBool("JWT_ALLOW_INSECURE_HTTP", false);
    const std::string redisHost = config.required("REDIS_HOST");
    const int redisPort = config.requiredInt("REDIS_PORT");
    const std::string redisPassword = config.requiredSecret("REDIS_PASSWORD");
    const std::string smtpUrl = config.required("EMAIL_SMTP_URL");
    const std::string emailFrom = config.required("EMAIL_FROM");
    const std::string smtpUsername = config.optional("EMAIL_SMTP_USERNAME", "");
    const std::string smtpPassword = smtpUsername.empty()
        ? ""
        : config.requiredSecret("EMAIL_SMTP_PASSWORD");
    const bool smtpTls = config.optionalBool("EMAIL_SMTP_TLS", false);
    const std::string emailVerificationUrl =
        config.required("EMAIL_VERIFICATION_URL");
    const std::string passwordResetUrl =
        config.required("EMAIL_PASSWORD_RESET_URL");
    const std::string allowedBrowserOrigin =
        config.required("AUTH_ALLOWED_ORIGIN");
    const bool secureRefreshCookie =
        config.optionalBool("AUTH_REFRESH_COOKIE_SECURE", false);
    const bool cleanupOrphanedImages =
        config.optionalBool("MATCHA_CLEANUP_ORPHANED_IMAGES", false);

    if (const auto checked = config.finish(); !checked)
    {
        logger.error("Invalid configuration", {{"errors", checked.error().message}});
        return 1;
    }

    const SmtpEmailSender emailSender({
        smtpUrl,
        emailFrom,
        smtpUsername,
        smtpPassword,
        smtpTls
    });

```

Ce qui change, et pourquoi :

- **`main` devient `run(logger)`** : `main` (écrite au §9.6) crée le logger et attrape les
  exceptions. Le logger doit exister **avant** le `try` pour servir dans le `catch`.
- **Toute la config est lue en haut.** Avant, `REDIS_HOST`, `REDIS_PORT`,
  `JWT_ALLOW_INSECURE_HTTP` et `MATCHA_CLEANUP_ORPHANED_IMAGES` étaient lues plus bas, au
  milieu de la construction des objets. Pour que `finish()` rapporte **tout**, toutes les
  lectures doivent avoir lieu avant lui.
- **`requiredSecret("REDIS_PASSWORD")`** sans `_FILE` : `ConfigReader` cherche lui-même
  `REDIS_PASSWORD` puis `REDIS_PASSWORD_FILE` (§6.2). `docker-compose.yml` fournit
  `REDIS_PASSWORD_FILE` : rien à changer côté Docker.
- **`emailSender` est construit après `finish()`**, et plus au milieu des lectures : avec
  une config invalide, on ne veut construire aucun objet à partir de valeurs neutres
  (`""`).
- `if (const auto checked = ...; !checked)` : un `if` avec **initialisation** (C++17).
  `checked` n'existe qu'à l'intérieur du `if`.
- `environment` est déclaré **avant** `config` : `config` en garde une référence (§6.1),
  il doit donc être détruit **après**. Les variables locales sont détruites dans l'ordre
  inverse de leur déclaration.

### 9.5 Quatre petits remplacements plus bas

Toujours dans `run`, dans l'ordre du fichier :

**1.** L'échec d'initialisation de la base :

```cpp
        std::cerr << "[ERROR BDD] Database initialization failed"
                  << std::endl;
```

devient :

```cpp
        logger.error("Database initialization failed");
```

**2.** Le nettoyage des images orphelines :

```cpp
    if (optionalBooleanEnvironmentVariable(
            "MATCHA_CLEANUP_ORPHANED_IMAGES", false))
    {
        profilePhotoService.cleanupOrphanedFiles();
        std::cout << "[INFO STORAGE] Orphaned profile images cleaned"
            << std::endl;
    }
```

devient :

```cpp
    if (cleanupOrphanedImages)
    {
        profilePhotoService.cleanupOrphanedFiles();
        logger.info("Orphaned profile images cleaned");
    }
```

**3.** Redis :

```cpp
    redisConnectionOptions.host =
        requiredEnvironmentVariable("REDIS_HOST");
    redisConnectionOptions.port = std::stoi(
        requiredEnvironmentVariable("REDIS_PORT")
    );
```

devient :

```cpp
    redisConnectionOptions.host = redisHost;
    redisConnectionOptions.port = redisPort;
```

**4.** Le client HTTP du JWKS :

```cpp
    httpClientConfig.allowInsecureHttp =
        optionalBooleanEnvironmentVariable(
            "JWT_ALLOW_INSECURE_HTTP",
            false
        );
```

devient :

```cpp
    httpClientConfig.allowInsecureHttp = allowInsecureJwksHttp;
```

### 9.6 La fin du fichier : le nouveau `main`

Remplace la fin, depuis `app` jusqu'à la dernière accolade :

```cpp
    app
        .port(18080)
        .multithreaded()
        .run();
}
catch (const std::exception &error)
{
    std::cerr << "[STARTUP ERROR] " << error.what() << std::endl;
    return 1;
}
```

par :

```cpp
    logger.info("Server starting", {{"port", "18080"}});
    app
        .port(18080)
        .multithreaded()
        .run();
    return 0;
}

int main()
{
    core::StreamLogger logger(std::cerr, core::LogLevel::Info);
    try
    {
        return run(logger);
    }
    catch (const std::exception &error)
    {
        logger.error("Startup failed", {{"reason", error.what()}});
        return 1;
    }
}
```

- `run` reçoit un `core::ILogger &` (l'interface), pas un `StreamLogger &` : `run` ne
  dépend pas de la façon dont les logs sont écrits.
- `LogLevel::Info` : les `debug(...)` sont ignorés en fonctionnement normal.
- `return 0;` à la fin de `run` : `main` est la seule fonction où l'oublier est permis ;
  `run` est une fonction normale, GCC signalerait l'absence de `return`.
- Le `catch` reste : les exceptions **inattendues** (Redis injoignable au démarrage, par
  exemple) finissent là, sur une ligne de log propre.

### 9.7 Compiler

```sh
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug
```

✅ **Attendu** : aucune ligne `warning` ni `error`, puis
`100% tests passed, 0 tests failed out of 193`.

Si tu as une erreur `'requiredEnvironmentVariable' was not declared`, c'est qu'un appel
aux anciennes fonctions est resté : cherche-le avec
`grep -n EnvironmentVariable src/main.cpp`.

---

## 10. Étape I : vérifier tout le parcours

### 10.1 Casser la config exprès

Toujours dans le conteneur, lance le binaire avec une variable vide et une invalide.
`VAR=valeur commande` ne modifie l'environnement **que pour cette commande** :

```sh
JWT_ISSUER= REDIS_PORT=abc ./build/debug/matcha_backend; echo "code de sortie : $?"
```

✅ **Attendu** : **une** ligne, avec les **deux** erreurs, puis le code 1.

```
level=ERROR msg="Invalid configuration" errors="JWT_ISSUER is required; REDIS_PORT must be an integer, got 'abc'"
code de sortie : 1
```

Avec l'ancien code, tu n'aurais vu que `JWT_ISSUER`. Et `REDIS_PORT=abc`… n'aurait été
détecté que **plus tard**, par une exception de `std::stoi`, après la connexion à la base.

Pour voir **toutes** les variables requises d'un coup, lance-le sans aucun environnement
(`env -i` vide l'environnement) :

```sh
env -i ./build/debug/matcha_backend
```

✅ **Attendu** : une ligne qui liste 13 erreurs, de `JWT_ISSUER is required` à
`AUTH_ALLOWED_ORIGIN is required`, dont `REDIS_PASSWORD or REDIS_PASSWORD_FILE is required`.

### 10.2 Le vrai serveur, les tests

`exit` du conteneur, puis depuis `~/matcha` :

```sh
make rebuild
make logs
```

✅ **Attendu** dans les logs (Ctrl+C pour quitter `make logs`) : après les messages
`[INFO BDD]` (encore l'ancien format, ils partiront avec cpp-pg) :

```
level=INFO msg="Server starting" port=18080
```

L'application fonctionne comme avant (connecte-toi depuis le front pour t'en assurer).

```sh
make test
make sanitize
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 193`, deux fois.

---

## 11. Étape J : commiter

Depuis `~/matcha` :

```sh
git add backend/libs/cpp-core backend/libs/CMakeLists.txt backend/CMakePresets.json \
    backend/CMakeLists.txt backend/src/main.cpp
git status
```

✅ **Attendu** : sous `Changes to be committed`, les nouveaux fichiers de
`backend/libs/cpp-core/` (22) et 4 fichiers modifiés, rien d'autre.

```sh
git commit -m "cpp-core : Error/Result, ConfigReader, Logger ; main.cpp les utilise"
git log --oneline -4
```

Coche les tâches « Error et Result », « Config » et « Logger » dans le planning. 🎉

---

## 12. Questions

Réponds sans regarder, puis relis la section indiquée si tu bloques. Écris sous chaque
**Ta réponse**, à la place de la ligne `…`. Préviens-moi quand tu as fini.

### Q1. Exceptions ou `Result` (§1.1, §1.2)

Donne un cas où une fonction de cpp-pg devra renvoyer un `Result` en erreur, et un cas où
une exception reste acceptable. Qu'est-ce qui les distingue ?

**Ta réponse :**

…

### Q2. `ASSERT` ou `EXPECT` (§4.4)

Dans `HoldsValueOnSuccess`, que se passerait-il si on remplaçait `ASSERT_TRUE(result)` par
`EXPECT_TRUE(result)` et que `parsePositive(42)` renvoyait une erreur à cause d'un bug ?

**Ta réponse :**

…

### Q3. Pas de `default:` (§4.2)

Pourquoi le `switch` de `toString` n'a-t-il pas de `default:` ? Que se passe-t-il si tu
ajoutes `ErrorCode::TooManyRequests` sans toucher à `Error.cpp` ?

**Ta réponse :**

…

### Q4. `from_chars` (§6.2)

Que renvoie `std::stoi("6379abc")` ? Et `requiredInt` avec la même valeur ? Pourquoi
cette différence compte-t-elle pour une configuration ?

**Ta réponse :**

…

### Q5. Les erreurs groupées (§1.3, §9.4)

Pourquoi a-t-il fallu remonter la lecture de `REDIS_HOST` et
`MATCHA_CLEANUP_ORPHANED_IMAGES` en haut de `run`, alors que l'ancien code les lisait plus
bas sans problème ?

**Ta réponse :**

…

### Q6. L'ordre de déclaration (§6.1, §9.4)

Dans `run`, que se passerait-il si on écrivait `core::ConfigReader config(core::SystemEnvironment{});`
(un environnement **temporaire**) au lieu de déclarer `environment` à part ?

**Ta réponse :**

…

### Q7. L'injection de logs (§1.5, §7.3)

Un utilisateur choisit le pseudo `bob\nlevel=ERROR msg="database corrupted"`. Le backend
logge `logger.info("signup", {{"user", pseudo}})`. Combien de lignes sont écrites, et
pourquoi est-ce important ?

**Ta réponse :**

…

### Q8. Le mutex (§7.2)

Pourquoi construire toute la ligne **avant** de prendre le verrou, plutôt que d'écrire
morceau par morceau dans `_out` sous le verrou ?

**Ta réponse :**

…

---

## 13. Correction

> Remplie après tes réponses.
>
> **Légende** : 🟢 juste et complet · 🟡 juste mais incomplet ou imprécis ·
> 🔴 faux ou à côté · ⏳ pas encore corrigé

| Question | Évaluation |
|---|---|
| Q1. Exceptions ou `Result` | ⏳ |
| Q2. `ASSERT` ou `EXPECT` | ⏳ |
| Q3. Pas de `default:` | ⏳ |
| Q4. `from_chars` | ⏳ |
| Q5. Les erreurs groupées | ⏳ |
| Q6. L'ordre de déclaration | ⏳ |
| Q7. L'injection de logs | ⏳ |
| Q8. Le mutex | ⏳ |
