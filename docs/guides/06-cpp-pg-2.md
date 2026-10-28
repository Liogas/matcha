# Guide 06 : cpp-pg (2/2) : QueryBuilder, migrations, branchement dans le backend

> Planning : mercredi, « cpp-pg » (QueryBuilder, migrations, migration des
> repositories, suppression de `src/database/`).
> Durée : environ 5 h, lecture comprise. Plus long que le 05, mais découpé en étapes
> courtes qui finissent toutes sur des tests verts.
>
> Prérequis : le guide 05 est terminé, **et la correction du guide 05 aussi** : le
> `RESOURCE_LOCK` mal orthographié est corrigé et commité (§13 du guide 05, 🐛).
>
> Même principe : je te dis **exactement** quoi faire et **pourquoi**, c'est toi qui
> tapes. Tout a été vérifié sur une copie de ton dépôt dans `matcha_backend` (GCC 12.2,
> CMake 3.25, PostgreSQL 17) : chaque étape compilée et testée, 243 tests en `debug` et
> en `sanitize`, trois fois de suite avec `ctest -j8`, cpp-pg installée seule avec son
> package-consumer, le front vérifié avec `tsc` et `eslint`. Chaque sortie « Attendu »
> ou « Sortie réelle » de ce guide a été copiée d'un vrai terminal.

## Sommaire

0. [Ce qu'on fait, et le résultat attendu](#0-ce-quon-fait-et-le-résultat-attendu)
1. [Comment ce guide est construit](#1-comment-ce-guide-est-construit)
2. [La carte des notions](#2-la-carte-des-notions)
3. [Étape A : partir propre](#3-étape-a--partir-propre)
4. [Étape B : `pg::Executor`, une seule porte vers la base](#4-étape-b--pgexecutor-une-seule-porte-vers-la-base)
5. [Étape C : `QueryBuilder`, des filtres optionnels sans injection](#5-étape-c--querybuilder-des-filtres-optionnels-sans-injection)
6. [Étape D : les migrations](#6-étape-d--les-migrations)
7. [Étape E : publier cpp-pg 0.2.0](#7-étape-e--publier-cpp-pg-020)
8. [Étape F : faire le ménage dans le backend](#8-étape-f--faire-le-ménage-dans-le-backend)
9. [Étape G : les repositories passent à cpp-pg](#9-étape-g--les-repositories-passent-à-cpp-pg)
10. [Étape H : des tests de repositories qui ne laissent aucune trace](#10-étape-h--des-tests-de-repositories-qui-ne-laissent-aucune-trace)
11. [Étape I : le like mutuel, enfin atomique](#11-étape-i--le-like-mutuel-enfin-atomique)
12. [Étape J : `main.cpp`, CMake et le front](#12-étape-j--maincpp-cmake-et-le-front)
13. [Étape K : vérifier tout le parcours](#13-étape-k--vérifier-tout-le-parcours)
14. [Étape L : commiter](#14-étape-l--commiter)
15. [Questions de synthèse](#15-questions-de-synthèse)
16. [Correction](#16-correction)

---

## 0. Ce qu'on fait, et le résultat attendu

Le guide 05 a construit cpp-pg **à côté** du backend. Celui-ci la finit, puis la
**branche** : à la fin, `src/database/` n'existe plus.

| Brique | Rôle | Corrige |
|---|---|---|
| `pg::Executor` | ce que `Connection`, `Transaction` et `ConnectionPool` ont en commun : exécuter une requête | des repositories qui ne pourraient pas travailler dans la transaction de quelqu'un d'autre |
| `pg::QueryBuilder` | assembler un `SELECT` dont les filtres sont optionnels, valeurs toujours en `$n`, tri par liste blanche | la future recherche avancée, et la pagination par `OFFSET` (audit 4.17) |
| `pg::migrate` | appliquer `migrations/*.sql` dans l'ordre, en une transaction, sous verrou, avec somme de contrôle | `DatabaseInitializer` (audit 4.5, 4.16) |
| repositories sur cpp-pg | accès **par nom de colonne**, erreurs en `core::Result` | l'accès par index (audit 4.4) |
| `MatchService` | like, match et conversation dans **une** transaction, sous un verrou par paire | le match jamais créé quand deux likes se croisent (audit 4.6), le match sans conversation et la resynchronisation au démarrage (audit 4.7) |
| ménage | migration `DROP TABLE users`, suppression de `UserRepository` et de ses tests | le code mort (audit 4.15) |

À la fin :

| Vérification | Résultat attendu |
|---|---|
| `make test` | 243 tests (225 − 6 supprimés + 19 de cpp-pg + 5 du backend) |
| `ctest -j8`, trois fois | 243 tests, trois fois |
| `make sanitize` | 243 tests |
| `src/database/` | n'existe plus |
| deux likes croisés lancés au même instant, 20 fois | exactement **un** match et **une** conversation à chaque fois |
| `matcha_backend --migrations-dry-run` sur une copie de ta base de dev | 7 sommes de contrôle enregistrées, `009` « would be applied », rien de modifié |
| cpp-pg seule, installée avec cpp-core | 16 fichiers, package-consumer `OK` |

---

## 1. Comment ce guide est construit

Au guide 05, toutes les notions étaient expliquées d'un bloc au §1, avant la moindre
ligne de code, et les questions arrivaient 2500 lignes plus loin. Tu me l'as dit :
trop de notions d'un coup, pas assez de situations pour comprendre à quoi elles
servent. Ce guide est construit autrement.

**Chaque étape suit le même chemin :**

1. 🎬 **La situation.** Un moment précis de la vie de matcha où quelque chose casse :
   qui fait quoi, dans quel ordre, et ce que l'utilisateur voit. Souvent avec la
   **sortie réelle** du bug, obtenue en cassant exprès le code.
2. 💡 **La notion.** Ce qu'il faut savoir pour que ça ne casse plus. Seulement ce qui
   sert à **cette** étape.
3. ⌨️ **Le code.** Ce que tu tapes, avec le pourquoi des lignes qui comptent.
4. ✅ **Vérifier.** La commande, et ce qu'elle doit afficher.
5. ❓ **Point d'arrêt.** Une ou deux questions courtes, **pendant que le code est sous
   tes yeux**. Réponds dans ta tête (ou sur papier), **puis** ouvre la réponse cachée
   sous la question. Ce ne sont pas des examens : c'est pour vérifier que la situation
   et la notion se sont bien accrochées avant de passer à la suite.

À la fin, **quatre questions de synthèse** (§15), que je corrige comme d'habitude. Elles
reprennent les situations du guide : tu les auras toutes déjà rencontrées.

**Un geste qui revient partout** (c'est celui qui t'a manqué en Q4, Q7 et Q8 du guide
05) : quand une question montre du code, **déroule-le ligne par ligne** et note, à
chaque `;` et à chaque `}`, quels objets meurent, quelles transactions sont ouvertes,
et sur quelle connexion passe chaque requête. Les situations de ce guide sont écrites
comme ça : imite-les.

---

## 2. La carte des notions

Une ligne par notion : le problème qu'elle résout, et où elle est expliquée. Pas
besoin de tout comprendre maintenant : chaque notion est détaillée **dans son étape**,
juste avant le code qui s'en sert.

| Notion | Le problème concret | Étape |
|---|---|---|
| interface (classe abstraite), `virtual`/`override` | un même repository doit marcher seul **et** dans la transaction d'un service | B |
| un `template` ne peut pas être `virtual` | `query(sql, args...)` doit être disponible partout | B |
| numérotation `?` → `$n` | des filtres optionnels décalent les numéros des paramètres | C |
| priorité `AND`/`OR` | `a AND b OR c` ne veut pas dire ce qu'on croit | C |
| un nom de colonne ne peut pas être un paramètre | `ORDER BY $1` ne trie pas | C |
| liste blanche | le tri vient de l'URL, donc d'un utilisateur | C |
| protocole simple (`PQexec`) contre étendu (`PQexecParams`) | un fichier de migration contient plusieurs instructions | D |
| DDL transactionnel | une migration sur cinq échoue au milieu du démarrage | D |
| verrou consultatif de transaction | deux serveurs démarrent en même temps | D |
| somme de contrôle | quelqu'un « corrige » une migration déjà appliquée | D |
| historique en ajout seul | supprimer `users`, documenter le trou du `006` | F |
| alias de colonne (`AS`) | lire par nom une colonne calculée | G |
| pagination par curseur (*keyset*) | « voir plus » saute un profil, et `OFFSET` relit tout | G |
| transaction annulée en fin de test | les tests écrivaient dans ta base de dev | H |
| `SAVEPOINT` | une erreur attendue empoisonne la transaction du test | H |
| photo par instruction en `READ COMMITTED` | deux likes croisés ne se voient pas | I |
| verrou par paire, `LEAST`/`GREATEST` | faire passer deux likes croisés l'un après l'autre | I |
| ordre de destruction | le pool doit survivre à tout ce qui l'utilise (Q7 du guide 05) | J |

---

## 3. Étape A : partir propre

Depuis `~/matcha`, sur la branche `libs/cpp-pg` :

```sh
git status
git log --oneline -3
```

✅ **Attendu** : `nothing to commit, working tree clean`, et en tête du log ton commit
de correction du `RESOURCE_LOCK`. S'il manque, reprends le 🐛 du §13 du guide 05 avant
d'aller plus loin : ce guide ajoute des tests à base, et ils doivent tourner un par un.

```sh
git switch -c libs/cpp-pg-2
```

Vérifie aussi que la base de test tourne :

```sh
make up        # si l'environnement n'est pas déjà lancé
docker compose -f docker-compose.yml -f docker-compose.dev.yml ps postgres-test
```

✅ **Attendu** : `postgres-test` est `Up (healthy)`.

---

## 4. Étape B : `pg::Executor`, une seule porte vers la base

### 🎬 La situation

Bob like Alice, qui l'avait déjà liké. Le serveur doit faire trois écritures :

1. `LikeRepository` : enregistrer le like de Bob ;
2. `LikeRepository` : créer le match ;
3. `MessagingRepository` : ouvrir leur conversation.

Les trois doivent réussir **ensemble** ou échouer **ensemble** : un match sans
conversation, c'est exactement le bug que `main.cpp` « répare » aujourd'hui à chaque
démarrage (audit 4.7). Il faut donc **une transaction**, et que **deux repositories
différents** écrivent dedans.

Mais qu'est-ce qu'un repository doit recevoir dans son constructeur ?

- **Le pool** (`pg::ConnectionPool &`) ? Chaque requête emprunterait **sa** connexion.
  Les écritures 1, 2 et 3 partiraient sur trois connexions différentes, donc dans
  trois transactions différentes. Raté.
- **Une transaction** (`pg::Transaction &`) ? Alors `GET /api/likes`, une simple
  lecture, devrait ouvrir une transaction pour rien. Et dans `main.cpp`, qui crée les
  repositories une fois pour toutes au démarrage, il n'y a pas de transaction à leur
  donner.

Il faudrait un type qui veut dire « **quelque chose qui sait exécuter une requête** »,
que ce soit le pool ou une transaction. Le repository n'aurait pas à savoir lequel.

### 💡 La notion : une interface

En C++, une **interface** est une classe qui déclare des fonctions **sans les
écrire**, et que d'autres classes **promettent** d'implémenter :

```cpp
class Executor
{
public:
    virtual core::Result<Rows> execute(std::string_view sql, const Params &params = {}) = 0;
};

class Transaction : public Executor           // « je suis un Executor »
{
public:
    core::Result<Rows> execute(std::string_view sql, const Params &params = {}) override;
};
```

- `virtual` : l'appel `executor.execute(...)` est **aiguillé à l'exécution** vers la
  version de l'objet réel. Si `executor` désigne une `Transaction`, c'est
  `Transaction::execute` qui tourne ; si c'est le pool, `ConnectionPool::execute`.
- `= 0` : pas de version par défaut. Une classe qui en a une est **abstraite** : on ne
  peut pas créer un `Executor` tout seul, seulement des classes filles.
- `override` : « je remplace une fonction virtuelle de ma mère ». Si la signature ne
  correspond pas (une faute de frappe, un `const` oublié), c'est une **erreur de
  compilation** au lieu d'une nouvelle fonction qui ne remplace rien.

Avec ça, `LikeRepository` reçoit un `pg::Executor &` :

```cpp
LikeRepository likes(*pool);   // dans main.cpp : chaque requête emprunte une connexion
LikeRepository likes(*tx);     // dans MatchService : tout passe par la transaction
```

**Trois détails qui comptent, et que le code va traiter :**

**1. `query(sql, args...)` ne peut pas être virtuelle.** C'est un `template` : le
compilateur en fabrique une version **par liste de types d'arguments**
(`query<int>`, `query<std::string, int>`…), autant qu'il en faut, au fil du code.
Une fonction virtuelle, elle, occupe une case **fixe** dans la table des fonctions
virtuelles de la classe, connue une fois pour toutes. Les deux sont incompatibles :
`template` + `virtual` ne compile pas. La solution : `query` reste un template
**non virtuel**, écrit **une seule fois** dans `Executor`, et appelle la fonction
**virtuelle** `execute` :

```cpp
template <class... Args>
core::Result<Rows> query(std::string_view sql, const Args &...args)
{
    return this->execute(sql, pg::params(args...));   // execute : virtuelle
}
```

Conséquence : `Connection`, `Transaction` et `ConnectionPool` doivent **retirer** leur
propre `query`. Sinon, la leur **masquerait** celle d'`Executor` (en C++, un nom
déclaré dans une classe fille cache **tous** les noms identiques de la mère).

**2. Le destructeur d'`Executor` est `protected` et non virtuel.** La règle habituelle
dit : « une classe avec des fonctions virtuelles a un destructeur virtuel ». Elle
existe pour ce cas :

```cpp
pg::Executor *executor = new pg::Transaction(...);
delete executor;   // sans destructeur virtuel : seul ~Executor tourne, ~Transaction jamais
```

Mais **personne** ne détruira jamais un objet **à travers** un `Executor *` : on ne fait
que le **passer par référence** à des repositories. Un destructeur `protected` rend ce
`delete` **impossible à compiler**, ce qui supprime le risque à la racine.

**3. Les copies et déplacements sont remis par défaut.** Déclarer un destructeur
(même `= default`) supprime le constructeur de déplacement **implicite** d'une classe.
`Executor` n'a pas de données, mais `Connection` et `Transaction` doivent rester
**déplaçables** (`Connection::open` renvoie une `Connection` par valeur). On redéclare
donc les cinq opérations `= default`, `protected` elles aussi.

### ⌨️ Le code

**Nouveau fichier `libs/cpp-pg/include/pg/Executor.hpp`** :

```cpp
#pragma once
#include <core/Result.hpp>
#include <pg/Params.hpp>
#include <pg/Rows.hpp>
#include <string_view>

namespace pg
{
	// Ce qui sait exécuter une requête : une Connection, une Transaction, ou le
	// ConnectionPool (qui emprunte une connexion le temps de la requête).
	// Un repository reçoit un Executor & : il ne sait pas, et n'a pas à savoir,
	// s'il travaille dans une transaction ou non.
	class Executor
	{
	public:
		// Une seule instruction SQL. Les valeurs passent en $1, $2…
		virtual core::Result<Rows> execute(std::string_view sql, const Params &params = {}) = 0;

		// executor.query("SELECT ... WHERE id = $1", id)
		template <class... Args>
		core::Result<Rows> query(std::string_view sql, const Args &...args)
		{
			return this->execute(sql, pg::params(args...));
		}

	protected:
		// Protégé et non virtuel : on ne détruit jamais un objet à travers un
		// Executor *. Les copies et déplacements restent ceux des classes filles.
		Executor() = default;
		Executor(const Executor &) = default;
		Executor(Executor &&) = default;
		Executor &operator=(const Executor &) = default;
		Executor &operator=(Executor &&) = default;
		~Executor() = default;
	};
}
```

**`include/pg/Connection.hpp`** : deux changements.

1. Les includes : `Executor.hpp` remplace `Params.hpp` et `Rows.hpp` (il les inclut
   lui-même).

   ```cpp
   #pragma once
   #include <core/Result.hpp>
   #include <pg/Executor.hpp>
   #include <pg/Settings.hpp>
   #include <memory>
   #include <string_view>
   ```

2. La classe devient un `Executor` : `class Connection : public Executor`, `override`
   sur `execute`, et **supprime** le template `query` (il est maintenant dans
   `Executor`). À sa place, une nouvelle fonction, `executeScript`, qui servira aux
   migrations (étape D, on l'écrit dès maintenant pour ne plus toucher ce fichier) :

   ```cpp
   	class Connection : public Executor
   	{
   	public:
   		static core::Result<Connection> open(const ConnectionSettings &settings);

   		// Une seule instruction SQL. Les valeurs passent en $1, $2…,
   		// jamais collées dans le texte de la requête.
   		core::Result<Rows> execute(std::string_view sql, const Params &params = {}) override;

   		// Un script de plusieurs instructions (un fichier de migration), sans
   		// paramètres. À réserver à du SQL de confiance, jamais à une saisie.
   		core::Result<void> executeScript(std::string_view sql);

   		bool isHealthy() const;		// la connexion au serveur est-elle ouverte ?
   ```

   Le reste de la classe ne change pas.

> Le paramètre par défaut `= {}` est **répété** dans chaque `override`. C'est voulu :
> pour une fonction virtuelle, la valeur par défaut est choisie d'après le type
> **statique** (celui écrit dans le code), pas d'après l'objet réel. Avec la même
> valeur partout, la question ne se pose pas.

**`src/Connection.cpp`** : on fabrique l'erreur à deux endroits maintenant (`execute`
et `executeScript`), on la sort donc dans une fonction. Dans le `namespace` anonyme
du haut, **sous** `trimmed` :

```cpp
	// L'erreur portée par un résultat en échec, traduite pour cpp-core.
	core::Error failureOf(const pg_result *raw)
	{
		const char *state = PQresultErrorField(raw, PG_DIAG_SQLSTATE);
		if (state == nullptr) // erreur produite par libpq elle-même : connexion perdue
			return {core::ErrorCode::Unavailable, trimmed(PQresultErrorMessage(raw))};
		return {pg::errorCodeFor(state),
				trimmed(PQresultErrorField(raw, PG_DIAG_MESSAGE_PRIMARY)) + " (SQLSTATE " + state + ")"};
	}
```

Puis la fin d'`execute` devient :

```cpp
	const ExecStatusType status = PQresultStatus(raw);
	if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK)
		return rows;
	return std::unexpected(failureOf(raw));
}
```

Et, juste après `execute`, la nouvelle fonction :

```cpp
core::Result<void> pg::Connection::executeScript(std::string_view sql)
{
	const std::string text(sql);
	// PQexec, et non PQexecParams : le seul appel de libpq qui accepte
	// plusieurs instructions séparées par des « ; ». Il renvoie le résultat de
	// la dernière, ou de la première qui échoue (les suivantes ne sont pas exécutées).
	pg_result *raw = PQexec(this->_connection.get(), text.c_str());
	if (raw == nullptr)
		return core::fail(core::ErrorCode::Unavailable,
						  trimmed(PQerrorMessage(this->_connection.get())));
	const Rows rows(raw);
	const ExecStatusType status = PQresultStatus(raw);
	if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK || status == PGRES_EMPTY_QUERY)
		return {};
	return std::unexpected(failureOf(raw));
}
```

- `const Rows rows(raw);` ne sert qu'à **posséder** le résultat : son destructeur
  appellera `PQclear` à la sortie de la fonction, quelle que soit la branche (RAII,
  guide 05 §1.3). On ne lit aucune ligne.
- `PGRES_EMPTY_QUERY` : un fichier vide, ou qui ne contient que des commentaires. Pas
  une erreur.

**`include/pg/Pool.hpp`** : même traitement pour `Transaction` et `ConnectionPool`.

```cpp
	class Transaction : public Executor
	{
	public:
		Transaction(Transaction &&other) noexcept;
		Transaction &operator=(Transaction &&) = delete;
		~Transaction();

		core::Result<Rows> execute(std::string_view sql, const Params &params = {}) override;
		core::Result<void> executeScript(std::string_view sql); // voir Connection

		core::Result<void> commit();
		core::Result<void> rollback();
```

Et dans `class ConnectionPool : public Executor`, le template `query` est remplacé
par :

```cpp
		// Une requête seule, hors transaction : emprunte une connexion le temps de la requête.
		core::Result<Rows> execute(std::string_view sql, const Params &params = {}) override;
```

**`src/Pool.cpp`** : deux fonctions à ajouter. Avant `pg::Transaction::commit` :

```cpp
core::Result<void> pg::Transaction::executeScript(std::string_view sql)
{
	this->requireOpen();
	return this->_connection->executeScript(sql);
}
```

Et avant `pg::ConnectionPool::idleCount` (c'est le corps de l'ancien template `query`,
qui prend maintenant des `Params` déjà convertis) :

```cpp
core::Result<pg::Rows> pg::ConnectionPool::execute(std::string_view sql, const Params &params)
{
	auto connection = this->acquire();
	if (!connection)
		return std::unexpected(connection.error());
	return (*connection)->execute(sql, params);
}
```

Regarde où meurt la connexion empruntée : `connection` est une variable locale, donc
détruite à la `}` de la fonction, **après** que `execute` a renvoyé son résultat. Le
`PooledConnection` qu'elle contient se rend alors au pool. Et le `Rows` renvoyé reste
valide : il possède son `PGresult` (un `shared_ptr`, guide 05 Q4), il ne dépend pas
de la connexion.

**Un test**, à la fin de `tests/TransactionTests.cpp`. Il prouve qu'une même fonction,
qui ne connaît qu'un `Executor`, travaille **dans** la transaction qu'on lui donne :

```cpp
// Le même code « repository » (il ne connaît qu'un Executor) tourne seul, ou
// dans une transaction qu'il ne contrôle pas.
namespace
{
	bool insertItem(pg::Executor &database, const std::string &name)
	{
		return database.query("INSERT INTO items (name) VALUES ($1)", name).has_value();
	}
}

TEST_F(TransactionTest, ExecutorRunsInsideTheCallersTransaction)
{
	{
		auto tx = this->pool->begin();
		ASSERT_TRUE(tx) << tx.error().message;
		ASSERT_TRUE(insertItem(*tx, "in-transaction"));
	} // ROLLBACK : la ligne disparaît avec la transaction
	EXPECT_EQ(countItems(*this->pool), 0);
	ASSERT_TRUE(insertItem(*this->pool, "alone"));
	EXPECT_EQ(countItems(*this->pool), 1);
}
```

### ✅ Vérifier

Dans le conteneur (`make shell`), depuis `/app` :

```sh
cmake --build --preset debug -j"$(nproc)" --target cpp-pg_tests
ctest --preset debug -R '^(Query|TransactionTest|Pool)\.'
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 18` (8 + 6 + 4). Les tests du guide 05
n'ont pas bougé d'une ligne (ils appellent `query` sur des `Connection`,
`Transaction` et `ConnectionPool`, qui la trouvent maintenant dans `Executor`), plus
le nouveau.

### ❓ Point d'arrêt B

**B1.** Cette fonction est appelée deux fois : `f(*pool)`, puis `f(*tx)` (où `tx` est
une transaction ouverte). Combien de connexions sont empruntées au pool pendant
chaque appel, et les trois requêtes voient-elles les écritures des précédentes ?

```cpp
void f(pg::Executor &db)
{
    db.query("INSERT INTO items (name) VALUES ('a')");
    db.query("INSERT INTO items (name) VALUES ('b')");
    db.query("SELECT count(*) AS n FROM items");
}
```

<details><summary>Réponse</summary>

- `f(*pool)` : `ConnectionPool::execute` emprunte une connexion **par requête**, et la
  rend à sa `}`. Trois emprunts successifs (peut-être la même connexion trois fois,
  peut-être pas). Chaque `INSERT` est validé tout seul (hors transaction, PostgreSQL
  valide chaque instruction immédiatement). Le `SELECT` voit donc `a` et `b`.
- `f(*tx)` : **zéro** nouvel emprunt. La transaction tient déjà **sa** connexion depuis
  `pool->begin()`, et tout passe par elle. Le `SELECT` voit aussi `a` et `b` : une
  transaction voit ses propres écritures. Mais **personne d'autre** ne les voit avant
  `commit()`.

</details>

**B2.** Un collègue laisse le template `query` dans `Transaction` en plus de celui
d'`Executor`. Ça compile. Qu'est-ce qui a changé, et pourquoi c'est un piège ?

<details><summary>Réponse</summary>

Le `query` de `Transaction` **masque** celui d'`Executor` quand on l'appelle sur une
`Transaction` (type statique). Sur un `Executor &` qui désigne une transaction, c'est
celui d'`Executor` qui est appelé. Deux codes différents pour la même opération : le
jour où on en corrige un, l'autre garde le bug, et le comportement dépend du type
**écrit** dans l'appelant. Une seule définition, dans `Executor`, évite ça.

</details>

---

## 5. Étape C : `QueryBuilder`, des filtres optionnels sans injection

### 🎬 La situation

Le sujet demande une **recherche avancée** (IV.4) : âge minimum et maximum, popularité,
ville, tags… **chacun optionnel**, et un tri au choix de l'utilisateur. Le code naïf
assemble la requête à la main :

```cpp
std::string sql = "SELECT p.account_id FROM matcha_profiles p WHERE p.account_id <> $1";
std::vector<std::string> params = {std::to_string(viewer)};
if (minAge)
{
    sql += " AND age >= $2";
    params.push_back(std::to_string(*minAge));
}
if (city)
{
    sql += " AND p.city = $3";                 // $3 ?… ou $2 si minAge est absent !
    params.push_back(*city);
}
sql += " ORDER BY " + sortFromUrl;             // ?sort=… vient de l'URL
```

Trois bugs dans ces quelques lignes :

1. **La numérotation.** Sans `minAge`, la ville part en `$3` alors qu'il n'y a que deux
   paramètres : PostgreSQL refuse la requête. Il faudrait un compteur qu'on incrémente
   à chaque filtre, et ne jamais l'oublier.
2. **L'injection.** `sortFromUrl` vaut ce que l'utilisateur tape :
   `?sort=p.account_id; DROP TABLE auth_accounts`. Ici, `PQexecParams` refuserait les
   deux instructions (guide 05 §1.2), mais
   `?sort=(SELECT CASE WHEN … THEN 1 ELSE 2 END)` passerait, et permettrait de deviner
   le contenu de la base une question à la fois.
3. **La tentation.** Au troisième filtre, quelqu'un écrira
   `sql += " AND p.city = '" + *city + "'"`, parce que c'est plus court. Et c'est une
   injection.

`QueryBuilder` rend ces trois bugs **impossibles à écrire** :

```cpp
pg::QueryBuilder builder("SELECT p.account_id FROM matcha_profiles p");
builder.where("p.account_id <> ?", viewer)
    .whereIf(minAge, "age >= ?")
    .whereIf(city, "p.city = ?")
    .orderBy(sortFromUrl, allowedSorts)
    .limit(20);
const auto query = builder.build();
```

### 💡 La notion

**1. `?` devient `$n`, numéroté au moment de l'ajout.** Le builder garde la liste des
valeurs (`Params`, guide 05). Quand on ajoute une condition avec 2 valeurs alors que
la liste en contient déjà 3, ses `?` deviennent `$4` et `$5`. Le compteur, c'est la
taille de la liste : impossible de l'oublier.

**2. Chaque condition est mise entre parenthèses.** En SQL, `AND` passe **avant** `OR`
(comme `*` avant `+`). Sans parenthèses :

```sql
WHERE p.account_id <> $1 AND p.city = $2 OR p.city = $3
--    se lit : (p.account_id <> $1 AND p.city = $2) OR (p.city = $3)
--    → l'utilisateur se voit lui-même dans les résultats s'il habite la ville $3
```

Sortie réelle du test `NumbersPlaceholdersInOrder` quand j'ai retiré les parenthèses
du builder :

```
Expected equality of these values:
  query->sql
    Which is: "SELECT id FROM items WHERE score >= $1 AND name = $2 OR name = $3"
  "SELECT id FROM items WHERE (score >= $1) AND (name = $2 OR name = $3)"
```

**3. Un nom de colonne ne peut pas être un paramètre.** `$1` remplace une **valeur**,
jamais un morceau de syntaxe (nom de table, de colonne, mot-clé). Et le piège : ça ne
fait pas d'erreur. Vérifié dans `psql` :

```sql
PREPARE sorted(text) AS SELECT x FROM (VALUES (2), (1), (3)) AS v(x) ORDER BY $1;
EXECUTE sorted('x');
```

```
 x
---
 2
 1
 3
```

Pas trié. `ORDER BY $1` trie par la **chaîne** `'x'`, la même pour toutes les lignes.
Pour un tri choisi par l'utilisateur, il ne reste qu'une solution : une **liste
blanche**. L'URL donne une **clé** (`?sort=newest`), le code la cherche dans une table
**écrite par nous**, et seul le SQL de cette table entre dans la requête :

```cpp
const pg::SortWhitelist sorts{{"newest", "p.account_id DESC"},
                              {"age", "p.date_of_birth DESC, p.account_id"}};
```

Une clé inconnue ne touche jamais le SQL.

**4. Deux sortes d'erreurs, deux traitements** (la règle de la question 1 du guide 04) :

| Erreur | D'où elle vient | Traitement |
|---|---|---|
| `where("a BETWEEN ? AND ?", 1)` : 2 `?`, 1 valeur | du **code** : un bug | `std::logic_error`, tout de suite |
| `orderBy("prix", sorts)` : clé inconnue | de l'**URL** : une saisie | noté, puis `build()` renvoie une erreur `Invalid` (un futur 400) |

Le second cas reprend l'idée de `ConfigReader` (guide 04) : on **note** l'erreur pour
pouvoir continuer à enchaîner les appels (`.orderBy(...).limit(20)`), et on la
**rapporte** à un seul endroit, `build()`.

**5. Pourquoi `whereIf(std::optional, condition)`, et pas `whereIf(bool, condition,
valeur)`.** Imagine la version avec un booléen :

```cpp
builder.whereIf(minAge.has_value(), "age >= ?", *minAge);
```

En C++, **tous les arguments sont évalués avant l'appel**. Si `minAge` est vide,
`*minAge` est lu quand même, avant même que `whereIf` regarde le booléen :
comportement indéfini (guide 04 §1.2). En passant l'`optional` lui-même, c'est
`whereIf` qui décide s'il faut le lire.

**6. `std::less<>` dans `SortWhitelist`.** La clé de l'URL arrive en
`std::string_view`. Un `std::map<std::string, std::string>` normal exigerait de
construire un `std::string` juste pour chercher. Avec `std::less<>` (un comparateur
« transparent »), `find` accepte directement le `string_view`.

### ⌨️ Le code

**`include/pg/QueryBuilder.hpp`** :

```cpp
#pragma once
#include <core/Result.hpp>
#include <pg/Params.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pg
{
	// Une requête prête à partir : executor.execute(query.sql, query.params).
	struct Query
	{
		std::string sql;
		Params params;
	};

	// Les tris qu'un appelant a le droit de demander : une clé publique (venue
	// de l'URL, donc d'un utilisateur) vers un ORDER BY écrit par nous.
	// std::less<> : on cherche directement avec un std::string_view.
	using SortWhitelist = std::map<std::string, std::string, std::less<>>;

	// Construit une requête SELECT dont les conditions dépendent des filtres.
	// Les valeurs passent toujours en paramètres : chaque « ? » d'une condition
	// devient $1, $2… dans l'ordre. Ce n'est pas un ORM : le SQL reste écrit à
	// la main, le builder ne fait que l'assembler.
	//
	//   pg::QueryBuilder query("SELECT id FROM profiles p");
	//   query.where("p.age >= ?", 18).whereIf(city, "p.city = ?").limit(20);
	//   auto built = query.build();   // WHERE p.age >= $1 AND p.city = $2 LIMIT $3
	class QueryBuilder
	{
	public:
		// Le début de la requête : SELECT … FROM … (et ses JOIN).
		explicit QueryBuilder(std::string select);

		// Ajoute une condition (reliée aux autres par AND). Autant d'arguments
		// que de « ? » : sinon c'est un bug, et std::logic_error.
		template <class... Args>
		QueryBuilder &where(std::string_view condition, const Args &...args)
		{
			const std::size_t first = this->_params.size() + 1;
			(this->_params.add(args), ...);
			this->addCondition(condition, first, sizeof...(Args));
			return *this;
		}

		// La condition n'est ajoutée que si value contient une valeur, qui
		// remplace l'unique « ? ». whereIf(minAge, "p.age >= ?")
		template <class T>
		QueryBuilder &whereIf(const std::optional<T> &value, std::string_view condition)
		{
			if (value)
				this->where(condition, *value);
			return *this;
		}

		// Le tri demandé par key, s'il est dans allowed. Une clé inconnue n'est
		// pas un bug (elle vient d'une URL) : build() renverra une erreur Invalid.
		QueryBuilder &orderBy(std::string_view key, const SortWhitelist &allowed);

		QueryBuilder &limit(std::int64_t count);

		core::Result<Query> build() const;

	private:
		void addCondition(std::string_view condition, std::size_t first, std::size_t count);

		std::string _select;
		std::vector<std::string> _conditions;
		std::string _orderBy;
		std::optional<std::string> _error;
		std::optional<std::size_t> _limitParam;
		Params _params;
	};
}
```

- Dans `where`, `first` est calculé **avant** d'ajouter les valeurs : c'est le numéro
  du premier `?` de cette condition.
- `(this->_params.add(args), ...)` : la *fold expression* du guide 05 §1.6, un `add`
  par argument, dans l'ordre.
- `sizeof...(Args)` : le **nombre** d'arguments du pack, connu à la compilation.
- `_limitParam` retient le **numéro** du paramètre de `LIMIT` : il peut être ajouté
  avant ou après les conditions, il garde son numéro.

**`src/QueryBuilder.cpp`** :

```cpp
#include <pg/QueryBuilder.hpp>
#include <stdexcept>
#include <utility>

pg::QueryBuilder::QueryBuilder(std::string select)
	: _select(std::move(select))
{
}

void pg::QueryBuilder::addCondition(std::string_view condition, std::size_t first, std::size_t count)
{
	std::string numbered;
	std::size_t next = first;
	for (const char character : condition)
	{
		if (character != '?')
		{
			numbered += character;
			continue;
		}
		numbered += '$' + std::to_string(next);
		++next;
	}
	if (next - first != count)
		throw std::logic_error("pg::QueryBuilder: '" + std::string(condition) + "' has " + std::to_string(next - first) + " '?' but " + std::to_string(count) + " values");
	// Entre parenthèses : « a = $1 OR b = $2 » reste un seul bloc face aux AND.
	this->_conditions.push_back("(" + numbered + ")");
}

pg::QueryBuilder &pg::QueryBuilder::orderBy(std::string_view key, const SortWhitelist &allowed)
{
	const auto found = allowed.find(key);
	if (found == allowed.end())
		this->_error = "unknown sort key '" + std::string(key) + "'";
	else
		this->_orderBy = found->second;
	return *this;
}

pg::QueryBuilder &pg::QueryBuilder::limit(std::int64_t count)
{
	this->_params.add(count);
	this->_limitParam = this->_params.size();
	return *this;
}

core::Result<pg::Query> pg::QueryBuilder::build() const
{
	if (this->_error)
		return core::fail(core::ErrorCode::Invalid, *this->_error);
	std::string sql = this->_select;
	for (std::size_t i = 0; i < this->_conditions.size(); ++i)
		sql += (i == 0 ? " WHERE " : " AND ") + this->_conditions[i];
	if (!this->_orderBy.empty())
		sql += " ORDER BY " + this->_orderBy;
	if (this->_limitParam)
		sql += " LIMIT $" + std::to_string(*this->_limitParam);
	return Query{std::move(sql), this->_params};
}
```

**`tests/QueryBuilderTests.cpp`**, des tests **purs** (aucune base) : on compare le
SQL et les paramètres produits.

```cpp
#include <pg/QueryBuilder.hpp>
#include <gtest/gtest.h>
#include <stdexcept>

// Tests purs : on compare le SQL et les paramètres produits, sans base.

namespace
{
	std::vector<std::string> texts(const pg::Params &params)
	{
		std::vector<std::string> out;
		for (const pg::Text &value : params.values())
			out.push_back(value.value_or("NULL"));
		return out;
	}
}

TEST(QueryBuilder, WithoutConditionsHasNoWhere)
{
	const auto query = pg::QueryBuilder("SELECT id FROM items").build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items");
	EXPECT_EQ(query->params.size(), 0u);
}

TEST(QueryBuilder, NumbersPlaceholdersInOrder)
{
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.where("score >= ?", 10).where("name = ? OR name = ?", "a", "b");
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items WHERE (score >= $1) AND (name = $2 OR name = $3)");
	EXPECT_EQ(texts(query->params), (std::vector<std::string>{"10", "a", "b"}));
}

TEST(QueryBuilder, WhereIfSkipsEmptyOptional)
{
	const std::optional<int> minScore;
	const std::optional<std::string> name = "a";
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.whereIf(minScore, "score >= ?").whereIf(name, "name = ?");
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items WHERE (name = $1)");
	EXPECT_EQ(texts(query->params), (std::vector<std::string>{"a"}));
}

TEST(QueryBuilder, ValuesNeverEnterTheSql)
{
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.where("name = ?", "x' OR '1'='1");
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items WHERE (name = $1)");
	EXPECT_EQ(texts(query->params), (std::vector<std::string>{"x' OR '1'='1"}));
}

TEST(QueryBuilder, WrongNumberOfValuesIsABug)
{
	pg::QueryBuilder builder("SELECT id FROM items");
	EXPECT_THROW(builder.where("score BETWEEN ? AND ?", 1), std::logic_error);
	EXPECT_THROW(builder.where("score = 1", 1), std::logic_error);
}

TEST(QueryBuilder, KeysetConditionOnTwoColumns)
{
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.where("(score, id) < (?, ?)", 42, 7).limit(20);
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items WHERE ((score, id) < ($1, $2)) LIMIT $3");
	EXPECT_EQ(texts(query->params), (std::vector<std::string>{"42", "7", "20"}));
}

TEST(QueryBuilder, OrderByComesFromTheWhitelist)
{
	const pg::SortWhitelist sorts{{"newest", "id DESC"}, {"score", "score DESC, id DESC"}};
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.orderBy("score", sorts).limit(5);
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items ORDER BY score DESC, id DESC LIMIT $1");
}

TEST(QueryBuilder, UnknownSortKeyIsInvalid)
{
	const pg::SortWhitelist sorts{{"newest", "id DESC"}};
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.orderBy("id; DROP TABLE items", sorts);
	const auto query = builder.build();
	ASSERT_FALSE(query);
	EXPECT_EQ(query.error().code, core::ErrorCode::Invalid);
	EXPECT_EQ(query.error().message, "unknown sort key 'id; DROP TABLE items'");
}
```

Et un test avec la base, pour vérifier que PostgreSQL accepte bien ce que le builder
produit. À la fin de `tests/QueryTests.cpp`, et ajoute `#include <pg/QueryBuilder.hpp>`
sous `#include "support/PgTest.hpp"` :

```cpp
TEST_F(Query, RunsABuiltQuery)
{
	ASSERT_TRUE(this->connection->query("INSERT INTO items (name, score) VALUES ('a', 1), ('b', 5), ('c', 9)"));
	const std::optional<int> minScore = 4;
	pg::QueryBuilder builder("SELECT name FROM items");
	builder.whereIf(minScore, "score >= ?").orderBy("name", {{"name", "name"}}).limit(10);
	const auto query = builder.build();
	ASSERT_TRUE(query) << query.error().message;
	const auto rows = this->connection->execute(query->sql, query->params);
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 2u);
	EXPECT_EQ((*rows)[0].get<std::string>("name"), "b");
	EXPECT_EQ((*rows)[1].get<std::string>("name"), "c");
}
```

**`libs/cpp-pg/CMakeLists.txt`** : ajoute `src/QueryBuilder.cpp` à la liste de
`add_library(cpp-pg STATIC …)` (après `src/Pool.cpp`), et
`tests/QueryBuilderTests.cpp` à celle de `add_executable(cpp-pg_tests …)` (après
`tests/PoolTests.cpp`).

### ✅ Vérifier

```sh
cmake --build --preset debug -j"$(nproc)" --target cpp-pg_tests
ctest --preset debug -R '^(QueryBuilder|Query)\.'
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 17` (8 tests purs + 9 de
`Query`).

### ❓ Point d'arrêt C

**C1.** Écris le SQL et la liste des paramètres que produit ce code, avec
`city = "Paris"` et `minAge` vide :

```cpp
pg::QueryBuilder builder("SELECT p.account_id FROM matcha_profiles p");
builder.whereIf(minAge, "age >= ?")
    .where("p.account_id <> ?", 7)
    .whereIf(city, "p.city = ? OR p.city IS NULL")
    .limit(20);
```

<details><summary>Réponse</summary>

```sql
SELECT p.account_id FROM matcha_profiles p
WHERE (p.account_id <> $1) AND (p.city = $2 OR p.city IS NULL) LIMIT $3
```

Paramètres : `["7", "Paris", "20"]`. `minAge` vide : sa condition n'existe pas, et ne
consomme aucun numéro. Les parenthèses gardent le `OR` à l'intérieur de son bloc.

</details>

**C2.** Le front appelle `/api/profiles?sort=fame` alors que `fame` n'est pas (encore)
dans la liste blanche. Que se passe-t-il, depuis `orderBy` jusqu'à la réponse HTTP ?
Et pourquoi `orderBy` ne lève-t-il pas d'exception ?

<details><summary>Réponse</summary>

`orderBy` ne trouve pas la clé : il **note** l'erreur et rend `*this` (on peut
continuer à enchaîner). `build()` voit l'erreur notée et renvoie
`core::fail(Invalid, "unknown sort key 'fame'")`. La requête n'est pas exécutée, et une
route bien écrite répond 400 avec ce message. Pas d'exception, parce que ce n'est pas un bug du code : c'est une **saisie**
(l'URL), un cas **prévisible** que l'appelant doit traiter. Une exception est réservée
aux erreurs qui ne peuvent venir que d'un code faux (le nombre de `?`).

</details>

---

## 6. Étape D : les migrations

Aujourd'hui, `src/database/DatabaseInitializer.cpp` applique les fichiers de
`backend/migrations/` au démarrage. On le remplace par `pg::migrate`, dans cpp-pg.
Cette étape a **cinq** situations, parce que démarrer un serveur sur une base qui
évolue cache cinq pièges différents.

### 🎬 Les situations

**Situation 1 : deux serveurs démarrent en même temps.** En production avec deux
instances, ou pendant un redémarrage où l'ancienne n'est pas encore arrêtée. Les deux
lisent `schema_migrations`, les deux voient que `009` manque, les deux l'exécutent :

```
serveur 1                                  serveur 2
SELECT version FROM schema_migrations     SELECT version FROM schema_migrations
  → 009 manque                              → 009 manque
DROP TABLE users  ✔                       DROP TABLE users  ✘ ou pire…
```

J'ai fabriqué la situation : le test `TwoMigratorsApplyOnce` (plus bas) lance deux
migrateurs sur deux threads. Sortie réelle, verrou retiré :

```
MigrationTests.cpp:172: Failure
duplicate key value violates unique constraint "pg_type_typname_nsp_index" (SQLSTATE 23505)
[  FAILED  ] Migrations.TwoMigratorsApplyOnce
```

Même `CREATE TABLE IF NOT EXISTS` ne suffit pas : les deux voient « n'existe pas » au
même instant, et le second se heurte au premier dans le catalogue de PostgreSQL.

**Situation 2 : la migration 3 sur 5 échoue.** Avec `DatabaseInitializer`, chaque
fichier a sa propre transaction : 1 et 2 sont validées, 3 est annulée, le serveur
s'arrête. La base est **à moitié migrée** : ni l'ancien schéma, ni le nouveau.

**Situation 3 : quelqu'un « corrige » une migration déjà appliquée.** Une faute dans
un `CHECK` de `002`, corrigée directement dans le fichier. Sur ta base, `002` est
marquée appliquée : elle ne sera **jamais** relancée, ta base garde l'ancienne
version. Sur une base neuve (celle du correcteur !), c'est la nouvelle. Deux schémas
différents, et personne ne le sait.

**Situation 4 : `10_` avant `9_`.** `DatabaseInitializer` range les fichiers dans un
`std::set<std::filesystem::path>`, donc par **nom**. Caractère par caractère, `"1"` <
`"9"` : `10_seed.sql` passe avant `9_create.sql`. Tes fichiers sont numérotés sur trois
chiffres, ça tient… jusqu'au jour où quelqu'un écrit `10_` au lieu de `010_`.

**Situation 5 : essayer avant d'appliquer.** Avant une démo, tu veux savoir ce que le
prochain démarrage va faire à la base, **sans** le faire.

### 💡 La notion

**1. Le protocole simple (`PQexec`) contre le protocole étendu (`PQexecParams`).** Un
fichier de migration contient **plusieurs** instructions. `PQexecParams` (tout cpp-pg
jusqu'ici) n'en accepte qu'**une**, c'est une protection contre l'injection (guide 05
§1.2). `PQexec` accepte un script entier, mais **sans** paramètres `$n`. D'où
`executeScript` (étape B), réservée à du SQL **de confiance** : nos fichiers, jamais une
saisie.

**2. Le DDL est transactionnel dans PostgreSQL** (situation 2). `CREATE TABLE`,
`ALTER TABLE`, `DROP TABLE` s'annulent avec `ROLLBACK`, comme un `INSERT`. Ce n'est
pas le cas partout (MySQL valide chaque `CREATE` tout de suite). On en profite :
**toutes** les migrations en attente tournent dans **une seule** transaction. Une
échoue, tout est annulé, la base reste exactement comme avant. Une seule exception :
les instructions interdites en transaction. Vérifié :

```
ERROR:  CREATE INDEX CONCURRENTLY cannot run inside a transaction block
```

Le README de `migrations/` le dira.

**3. Le verrou consultatif de transaction** (situation 1). PostgreSQL fournit des
verrous **sans table** : on choisit un nombre, et une seule transaction à la fois peut
tenir le verrou de ce nombre. Les autres **attendent**.

```sql
SELECT pg_advisory_xact_lock(hashtextextended('schema_migrations', 0));
```

- `hashtextextended(texte, 0)` transforme un texte en un nombre de 64 bits : le nom de
  la table de suivi sert de clé. Un autre projet (ou un test) qui utilise une autre
  table n'attend pas le nôtre.
- `_xact_` : le verrou est rendu **tout seul** au `COMMIT` ou au `ROLLBACK`. L'autre
  variante, `pg_advisory_lock`, est liée à la **session** (la connexion) et doit être
  rendue à la main. `DatabaseInitializer` utilisait celle-là, avec une classe RAII pour
  la rendre. Avec un **pool**, ce serait un piège : une connexion rendue au pool sans
  avoir rendu son verrou le garderait, et le prochain serveur qui démarre attendrait…
  jusqu'à l'arrêt du premier.
- Et après l'attente ? Le serveur 2 obtient le verrou quand le serveur 1 a validé. En
  `READ COMMITTED` (le défaut, guide 05 §1.5), **chaque instruction** voit tout ce qui a
  été validé avant **son** début. Le `SELECT version FROM schema_migrations` qui suit le
  verrou voit donc `009` : rien à faire. C'est ce que vérifie `TwoMigratorsApplyOnce`.

**4. La somme de contrôle** (situation 3). Pour chaque migration appliquée, on
enregistre le **SHA-256** de son contenu. Au démarrage suivant, on recalcule : si le
fichier a changé, on **refuse de démarrer**. Ce n'est pas une sécurité contre un
attaquant, c'est une alarme contre une erreur humaine. Le calcul, on le demande à
PostgreSQL (pas besoin d'ajouter une bibliothèque de hachage à cpp-pg) :

```sql
SELECT encode(sha256(convert_to($1, 'UTF8')), 'hex') AS checksum
```

`convert_to` transforme le texte en octets, `sha256` les hache, `encode(…, 'hex')` donne
les 64 caractères hexadécimaux.

Ta base de dev a déjà une `schema_migrations` **sans** colonne `checksum`. La première
fois, `migrate` ajoute la colonne, et **adopte** les lignes existantes : il enregistre
la somme du fichier actuel, en le signalant dans les logs.

**5. Trier par numéro** (situation 4) : on lit le numéro avec `std::from_chars`
(guide 04, Q4) et on trie sur lui. Deux fichiers avec le même numéro : erreur.

**6. Le mode `dryRun`** (situation 5) : grâce au point 2, c'est presque gratuit. On fait
**tout** (vraiment, y compris exécuter les migrations : on vérifie donc qu'elles
passent), puis on ne valide pas : le destructeur de la transaction fait `ROLLBACK`.

**7. Le nom de la table entre dans le SQL.** Comme pour `ORDER BY` (étape C), un nom de
table ne peut pas être un `$n`. La table de suivi est configurable (les tests en
utilisent une autre), donc on **valide** son nom : uniquement `[a-z_][a-z0-9_]*`. Rien
d'autre ne peut passer.

### ⌨️ Le code

**`include/pg/Migrations.hpp`** :

```cpp
#pragma once
#include <core/Logger.hpp>
#include <core/Result.hpp>
#include <pg/Pool.hpp>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace pg
{
	// Un fichier de migration : 007_create_likes.sql → version 7.
	struct MigrationFile
	{
		int version;
		std::string name; // le nom du fichier, pour les messages
		std::filesystem::path path;
	};

	// Les fichiers NNN_nom.sql d'un dossier, triés par numéro (pas par nom :
	// 10_x.sql vient après 9_y.sql). Invalid si un nom ne suit pas ce format,
	// ou si deux fichiers portent le même numéro.
	core::Result<std::vector<MigrationFile>> listMigrations(const std::filesystem::path &directory);

	struct MigrationOptions
	{
		std::filesystem::path directory = "migrations";
		std::string table = "schema_migrations"; // [a-z_][a-z0-9_]*
		bool dryRun = false;					 // tout appliquer, puis ROLLBACK
	};

	struct MigrationReport
	{
		std::vector<std::string> applied; // appliquées par cet appel (ou qui l'auraient été)
		std::size_t upToDate = 0;		  // déjà appliquées, somme de contrôle vérifiée
	};

	// Applique les migrations manquantes, dans l'ordre, en UNE transaction :
	// toutes ou aucune. Un verrou consultatif empêche deux serveurs de migrer en
	// même temps. Une migration déjà appliquée dont le fichier a changé depuis
	// est refusée (Invalid) : on ne modifie pas l'histoire, on ajoute un fichier.
	core::Result<MigrationReport> migrate(ConnectionPool &pool, const MigrationOptions &options,
										  core::ILogger &logger);
}
```

**`src/Migrations.cpp`** :

```cpp
#include <pg/Migrations.hpp>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>

namespace
{
	// Le nom de la table est collé dans le SQL (un nom ne peut pas être un
	// paramètre $n) : on n'accepte que des caractères sans danger.
	bool isPlainIdentifier(std::string_view name)
	{
		if (name.empty() || name.size() > 63 || (name[0] >= '0' && name[0] <= '9'))
			return false;
		return std::ranges::all_of(name, [](char c)
								   { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
	}

	// "007_create_likes.sql" → 7 ; nullopt si le nom ne suit pas NNN_nom.sql.
	std::optional<int> versionOf(const std::string &filename)
	{
		const std::size_t underscore = filename.find('_');
		if (underscore == 0 || underscore == std::string::npos || underscore + 1 >= filename.size() - 4)
			return std::nullopt;
		int version = 0;
		const char *end = filename.data() + underscore;
		const auto [stop, error] = std::from_chars(filename.data(), end, version);
		if (error != std::errc{} || stop != end)
			return std::nullopt;
		return version;
	}

	core::Result<std::string> readFile(const std::filesystem::path &path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			return core::fail(core::ErrorCode::Internal, "cannot read " + path.string());
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}
}

core::Result<std::vector<pg::MigrationFile>> pg::listMigrations(const std::filesystem::path &directory)
{
	std::error_code error;
	std::filesystem::directory_iterator entries(directory, error);
	if (error)
		return core::fail(core::ErrorCode::Internal,
						  "cannot open migration directory " + directory.string() + ": " + error.message());
	std::vector<MigrationFile> files;
	for (const auto &entry : entries)
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".sql")
			continue;
		const std::string name = entry.path().filename().string();
		const auto version = versionOf(name);
		if (!version)
			return core::fail(core::ErrorCode::Invalid,
							  "invalid migration file name '" + name + "' (expected NNN_name.sql)");
		files.push_back({*version, name, entry.path()});
	}
	std::ranges::sort(files, {}, &MigrationFile::version);
	for (std::size_t i = 1; i < files.size(); ++i)
		if (files[i].version == files[i - 1].version)
			return core::fail(core::ErrorCode::Invalid,
							  "migrations " + files[i - 1].name + " and " + files[i].name + " have the same version");
	return files;
}

core::Result<pg::MigrationReport> pg::migrate(ConnectionPool &pool, const MigrationOptions &options,
											  core::ILogger &logger)
{
	if (!isPlainIdentifier(options.table))
		return core::fail(core::ErrorCode::Invalid, "invalid migrations table name '" + options.table + "'");
	const auto files = listMigrations(options.directory);
	if (!files)
		return std::unexpected(files.error());

	auto tx = pool.begin();
	if (!tx)
		return std::unexpected(tx.error());
	// Un seul migrateur à la fois. Le verrou est rendu tout seul au COMMIT ou au ROLLBACK.
	if (const auto locked = tx->query("SELECT pg_advisory_xact_lock(hashtextextended($1, 0))", options.table); !locked)
		return std::unexpected(locked.error());

	const std::string &table = options.table;
	if (const auto created = tx->execute("CREATE TABLE IF NOT EXISTS " + table + " ("
																				 "version INTEGER PRIMARY KEY, "
																				 "applied_at TIMESTAMPTZ NOT NULL DEFAULT now())");
		!created)
		return std::unexpected(created.error());
	// Les tables créées avant les sommes de contrôle n'ont pas cette colonne.
	if (const auto altered = tx->execute("ALTER TABLE " + table + " ADD COLUMN IF NOT EXISTS checksum TEXT"); !altered)
		return std::unexpected(altered.error());

	const auto rows = tx->execute("SELECT version, checksum FROM " + table);
	if (!rows)
		return std::unexpected(rows.error());
	std::map<int, std::optional<std::string>> applied;
	for (const Row row : *rows)
		applied[row.get<int>("version")] = row.get<std::optional<std::string>>("checksum");

	MigrationReport report;
	for (const MigrationFile &file : *files)
	{
		const auto sql = readFile(file.path);
		if (!sql)
			return std::unexpected(sql.error());
		// La base calcule le SHA-256 : pas de dépendance à une lib de hachage.
		const auto hashed = tx->query("SELECT encode(sha256(convert_to($1, 'UTF8')), 'hex') AS checksum", *sql);
		if (!hashed)
			return std::unexpected(hashed.error());
		const auto checksum = (*hashed)[0].get<std::string>("checksum");

		if (const auto known = applied.find(file.version); known != applied.end())
		{
			const std::optional<std::string> recordedChecksum = known->second;
			applied.erase(known); // ce qui restera : des versions appliquées sans fichier
			if (!recordedChecksum)
			{
				// Appliquée avant que les sommes de contrôle existent : on l'adopte.
				const auto recorded = tx->query("UPDATE " + table + " SET checksum = $2 WHERE version = $1",
												file.version, checksum);
				if (!recorded)
					return std::unexpected(recorded.error());
				logger.info("Migration checksum recorded", {{"migration", file.name}});
			}
			else if (*recordedChecksum != checksum)
				return core::fail(core::ErrorCode::Invalid,
								  "migration " + file.name + " was modified after being applied");
			++report.upToDate;
			continue;
		}

		logger.info(options.dryRun ? "Migration would be applied" : "Applying migration",
					{{"migration", file.name}});
		if (const auto ran = tx->executeScript(*sql); !ran)
			return core::fail(ran.error().code, "migration " + file.name + " failed: " + ran.error().message);
		const auto recorded = tx->query("INSERT INTO " + table + " (version, checksum) VALUES ($1, $2)",
										file.version, checksum);
		if (!recorded)
			return std::unexpected(recorded.error());
		report.applied.push_back(file.name);
	}
	for (const auto &[version, checksum] : applied)
		logger.warning("Applied migration has no file", {{"version", std::to_string(version)}});

	if (options.dryRun)
		return report; // le destructeur de tx fait ROLLBACK
	if (const auto committed = tx->commit(); !committed)
		return std::unexpected(committed.error());
	return report;
}
```

Les lignes qui méritent un arrêt :

- **`listMigrations`** : `std::ranges::sort(files, {}, &MigrationFile::version)` trie
  par le **membre** `version` (une *projection* : « compare les `version` »). Puis deux
  voisins de même numéro, c'est une erreur.
- **Le verrou est la première requête de la transaction**, avant même de créer la table
  de suivi. Tout ce qui suit est protégé.
- **`applied`** est une `std::map<int, std::optional<std::string>>` : version → somme
  enregistrée (`std::nullopt` pour une ligne de l'ancienne table, colonne `NULL`).
- **`recordedChecksum` est copié avant `applied.erase(known)`.** Après `erase`,
  l'itérateur `known` désigne un élément **détruit** : le lire serait le même bug que
  Q7 du guide 05 (lire un objet mort). Copier d'abord, effacer ensuite. Ce qui reste
  dans `applied` à la fin de la boucle, ce sont des versions appliquées dont le fichier
  a disparu : un avertissement.
- **En `dryRun`**, la fonction rend le rapport **sans** `commit()` : `tx` meurt à la `}`
  et fait `ROLLBACK`.

**`tests/MigrationTests.cpp`** : trois tests **sans** base (lire le dossier), six
**avec**.

```cpp
#include "support/PgTest.hpp"
#include <pg/Migrations.hpp>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace
{
	// Un dossier de migrations jetable, propre à chaque test.
	class MigrationDirectory
	{
	public:
		MigrationDirectory()
			: _path(fs::temp_directory_path() / ("cpp-pg-" + std::string(testing::UnitTest::GetInstance()->current_test_info()->name())))
		{
			fs::remove_all(this->_path);
			fs::create_directories(this->_path);
		}
		~MigrationDirectory() { fs::remove_all(this->_path); }

		void write(const std::string &name, const std::string &sql) const
		{
			std::ofstream(this->_path / name) << sql;
		}
		const fs::path &path() const { return this->_path; }

	private:
		fs::path _path;
	};
}

// ---- Sans base : lire le dossier ----

TEST(MigrationFiles, SortedByNumberNotByName)
{
	const MigrationDirectory directory;
	directory.write("10_second.sql", "");
	directory.write("9_first.sql", "");
	directory.write("notes.txt", ""); // pas en .sql : ignoré
	const auto files = pg::listMigrations(directory.path());
	ASSERT_TRUE(files) << files.error().message;
	ASSERT_EQ(files->size(), 2u);
	EXPECT_EQ((*files)[0].version, 9);
	EXPECT_EQ((*files)[1].name, "10_second.sql");
}

TEST(MigrationFiles, RejectsBadNames)
{
	const MigrationDirectory directory;
	directory.write("create_users.sql", "");
	const auto files = pg::listMigrations(directory.path());
	ASSERT_FALSE(files);
	EXPECT_EQ(files.error().code, core::ErrorCode::Invalid);
}

TEST(MigrationFiles, RejectsDuplicateVersions)
{
	const MigrationDirectory directory;
	directory.write("004_add_bio.sql", "");
	directory.write("004_add_city.sql", "");
	const auto files = pg::listMigrations(directory.path());
	ASSERT_FALSE(files);
	EXPECT_EQ(files.error().message, "migrations 004_add_bio.sql and 004_add_city.sql have the same version");
}

// ---- Avec base ----

class Migrations : public PgTest
{
protected:
	void SetUp() override
	{
		PgTest::SetUp();
		if (this->IsSkipped() || this->HasFatalFailure())
			return;
		ASSERT_TRUE(this->connection->executeScript("DROP TABLE IF EXISTS test_migrations, mig_items"));
		this->options.directory = this->directory.path();
		this->options.table = "test_migrations";
	}

	bool tableExists(const std::string &name)
	{
		const auto rows = this->connection->query("SELECT to_regclass($1) IS NOT NULL AS found", name);
		return rows && (*rows)[0].get<bool>("found");
	}

	MigrationDirectory directory;
	pg::MigrationOptions options;
};

TEST_F(Migrations, AppliesPendingFilesOnce)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	this->directory.write("002_fill.sql", "INSERT INTO mig_items VALUES (1);\nINSERT INTO mig_items VALUES (2);");
	const auto first = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_TRUE(first) << first.error().message;
	EXPECT_EQ(first->applied, (std::vector<std::string>{"001_create.sql", "002_fill.sql"}));

	const auto second = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_TRUE(second) << second.error().message;
	EXPECT_TRUE(second->applied.empty());
	EXPECT_EQ(second->upToDate, 2u);
	const auto rows = this->connection->query("SELECT count(*) AS n FROM mig_items");
	ASSERT_TRUE(rows);
	EXPECT_EQ((*rows)[0].get<std::int64_t>("n"), 2);
}

TEST_F(Migrations, FailureAppliesNothing)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	this->directory.write("002_broken.sql", "INSERT INTO mig_items VALUES ('not a number');");
	const auto result = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().code, core::ErrorCode::Invalid);
	EXPECT_NE(result.error().message.find("002_broken.sql"), std::string::npos);
	EXPECT_FALSE(this->tableExists("mig_items")); // 001 annulée aussi
	EXPECT_FALSE(this->tableExists("test_migrations"));
}

TEST_F(Migrations, RefusesAModifiedFile)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	ASSERT_TRUE(pg::migrate(*this->pool, this->options, this->logger));
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id BIGINT);");
	const auto result = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().message, "migration 001_create.sql was modified after being applied");
}

TEST_F(Migrations, DryRunChangesNothing)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	this->options.dryRun = true;
	const auto result = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_TRUE(result) << result.error().message;
	EXPECT_EQ(result->applied, (std::vector<std::string>{"001_create.sql"}));
	EXPECT_FALSE(this->tableExists("mig_items"));
}

TEST_F(Migrations, AdoptsRowsWithoutChecksum)
{
	// La table telle que l'écrivait l'ancien DatabaseInitializer.
	ASSERT_TRUE(this->connection->executeScript(
		"CREATE TABLE test_migrations (version INTEGER PRIMARY KEY, applied_at TIMESTAMPTZ NOT NULL DEFAULT now());"
		"INSERT INTO test_migrations (version) VALUES (1);"
		"CREATE TABLE mig_items (id INT);"));
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	const auto result = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_TRUE(result) << result.error().message;
	EXPECT_EQ(result->upToDate, 1u);
	const auto rows = this->connection->query("SELECT checksum FROM test_migrations WHERE version = 1");
	ASSERT_TRUE(rows);
	EXPECT_EQ((*rows)[0].get<std::string>("checksum").size(), 64u); // SHA-256 en hexadécimal
}

// Deux serveurs qui démarrent en même temps : le verrou consultatif fait
// attendre le second, qui trouve ensuite tout déjà appliqué.
TEST_F(Migrations, TwoMigratorsApplyOnce)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT); SELECT pg_sleep(0.2);");
	core::Result<pg::MigrationReport> results[2] = {core::fail(core::ErrorCode::Internal, ""),
													  core::fail(core::ErrorCode::Internal, "")};
	std::thread first([&]
					  { results[0] = pg::migrate(*this->pool, this->options, this->logger); });
	std::thread second([&]
					   { results[1] = pg::migrate(*this->pool, this->options, this->logger); });
	first.join();
	second.join();
	ASSERT_TRUE(results[0]) << results[0].error().message;
	ASSERT_TRUE(results[1]) << results[1].error().message;
	EXPECT_EQ(results[0]->applied.size() + results[1]->applied.size(), 1u);
	EXPECT_EQ(results[0]->upToDate + results[1]->upToDate, 1u);
}
```

- `MigrationDirectory` crée un dossier temporaire **par test** (nommé d'après le test)
  et le supprime à sa destruction : RAII, encore.
- Le `SetUp` de `Migrations` appelle d'abord celui de `PgTest`, puis s'arrête si le
  test a été sauté (pas de `PG_TEST_HOST`) ou a déjà échoué.
- `TwoMigratorsApplyOnce` : le fichier contient `SELECT pg_sleep(0.2)`, pour que le
  premier migrateur garde le verrou assez longtemps et que le second **doive**
  attendre. `results` est initialisé avec des erreurs, parce qu'un `core::Result` n'a
  pas de constructeur par défaut.

**`libs/cpp-pg/CMakeLists.txt`** : ajoute `src/Migrations.cpp` après
`src/QueryBuilder.cpp`, et `tests/MigrationTests.cpp` après
`tests/QueryBuilderTests.cpp`.

### ✅ Vérifier

```sh
cmake --build --preset debug -j"$(nproc)" --target cpp-pg_tests
ctest --preset debug -R '^(MigrationFiles|Migrations)\.' -j8
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 9`. Avec `-j8` : grâce au
`RESOURCE_LOCK` corrigé, les six tests à base passent quand même un par un.

### ❓ Point d'arrêt D

**D1.** Tu ajoutes `010_index.sql` qui contient
`CREATE INDEX CONCURRENTLY profiles_city_idx ON matcha_profiles (city);`, et tu
redémarres. Que se passe-t-il pour `010`, et pour les autres migrations en attente ?

<details><summary>Réponse</summary>

`executeScript` reçoit l'erreur
`CREATE INDEX CONCURRENTLY cannot run inside a transaction block`. `migrate` renvoie
`migration 010_index.sql failed: …`, la transaction est annulée : **aucune** des
migrations en attente n'est appliquée (toutes ou aucune), et le serveur ne démarre pas.
Il faut enlever `CONCURRENTLY` (sur une petite table, un index normal se crée en un
instant).

</details>

**D2.** Pourquoi le verrou est-il `pg_advisory_xact_lock` et pas `pg_advisory_lock` ?
Imagine que `migrate` prenne un `pg_advisory_lock` (de session), puis échoue sur une
migration et sorte avec `return core::fail(...)` sans le rendre.

<details><summary>Réponse</summary>

Le verrou de session appartient à la **connexion**, pas à la transaction. Le `ROLLBACK`
ne le rend pas. La connexion retourne dans le pool **avec** le verrou. Ce serveur-là ne
le remarque pas (un verrou se reprend sans attendre par la même session), mais un
**autre** serveur qui démarre reste bloqué sur `pg_advisory_lock` tant que le premier
tourne. Le verrou `_xact_` disparaît avec la transaction, quelle que soit la façon dont
elle se termine : rien à oublier.

</details>

---

## 7. Étape E : publier cpp-pg 0.2.0

Pas de nouvelle notion ici : c'est la définition de terminé (guide 02), appliquée à une
**nouvelle version**. On a **ajouté** des choses sans rien casser pour qui utilisait la
0.1 (`query` existe toujours, au même endroit pour l'appelant) : en SemVer, c'est une
version **mineure**, 0.1.0 → 0.2.0.

### ⌨️ Le code

**`libs/cpp-pg/CMakeLists.txt`** : la version.

```cmake
project(cpp-pg VERSION 0.2.0 LANGUAGES CXX)
```

**`tests/package-consumer/CMakeLists.txt`** : il redemande la version qu'il teste
(c'est ce qu'on avait fait pour le gabarit, guide 02).

```cmake
find_package(cpp-pg 0.2 CONFIG REQUIRED)
```

**`tests/package-consumer/main.cpp`** : il utilise aussi les nouveaux en-têtes, pour
vérifier qu'ils sont bien installés et se compilent seuls.

```cpp
#include <pg/Migrations.hpp>
#include <pg/Pool.hpp>
#include <pg/QueryBuilder.hpp>

// Compile avec les en-têtes installés, et se lie à libpq : Connection::open
// vers un port fermé doit échouer proprement.
int main()
{
	pg::ConnectionSettings settings;
	settings.host = "127.0.0.1";
	settings.port = "1";
	const auto connection = pg::Connection::open(settings);
	const bool unavailable = !connection
		&& connection.error().code == core::ErrorCode::Unavailable;

	pg::QueryBuilder builder("SELECT 1");
	builder.where("1 = ?", 1);
	const auto query = builder.build();
	return (unavailable && query && query->params.size() == 1) ? 0 : 1;
}
```

**`README.md`** : quatre ajouts.

1. La description, en tête :

   ```markdown
   > Accès à PostgreSQL au-dessus de libpq : pool de connexions, transactions RAII,
   > paramètres typés, lecture des colonnes par leur nom, requêtes à filtres
   > optionnels et migrations.
   ```

2. Deux sections, entre « Exemple minimal » et « Utilisation » :

   ````markdown
   ## Requêtes à filtres optionnels

   ```cpp
   const pg::SortWhitelist sorts{{"newest", "p.id DESC"}, {"age", "p.birth DESC, p.id"}};
   pg::QueryBuilder builder("SELECT p.id FROM profiles p");
   builder.where("p.id <> ?", viewerId)
       .whereIf(city, "p.city = ?")          // std::optional : ajouté seulement s'il a une valeur
       .orderBy(sortFromUrl, sorts)          // clé inconnue : build() renvoie Invalid
       .limit(20);
   const auto query = builder.build();        // core::Result<pg::Query>
   if (query)
       rows = pool->execute(query->sql, query->params);
   ```

   ## Migrations

   ```cpp
   pg::MigrationOptions options;            // dossier "migrations", table "schema_migrations"
   const auto report = pg::migrate(*pool, options, logger);
   ```

   Fichiers `NNN_nom.sql`, appliqués par numéro, tous dans une seule transaction, sous
   un verrou consultatif. Le SHA-256 de chaque fichier appliqué est enregistré : un
   fichier modifié après coup est refusé. `options.dryRun` applique puis annule.
   ````

3. Dans « Utilisation », `find_package(cpp-pg 0.2 CONFIG REQUIRED)`.

4. À la fin de « Garanties » :

   ```markdown
   - `Connection`, `Transaction` et `ConnectionPool` sont des `pg::Executor` : un code
     qui reçoit un `Executor &` tourne aussi bien seul que dans une transaction.
   - `QueryBuilder` ne met jamais une valeur dans le texte SQL ; un tri ne peut venir que
     d'une `SortWhitelist`.
   ```

   Et à la fin de « Limites » :

   ```markdown
   - `QueryBuilder` ne sait faire que des `SELECT` ; un « ? » littéral (dans une chaîne
     SQL, ou l'opérateur `jsonb ?`) ne peut pas apparaître dans une condition.
   - Les migrations tournent dans une transaction : pas de `CREATE INDEX CONCURRENTLY`.
   ```

**`CHANGELOG.md`** : sous `## [Unreleased]` (qui reste vide, prêt pour la suite) :

```markdown
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
```

### ✅ Vérifier

La lib **seule, installée**, comme au guide 05 §9.2. C'est la vérification la plus
complète : elle compile cpp-pg hors du monorepo, lance **tous** ses tests, puis
construit le package-consumer contre les fichiers installés.

```sh
T="-DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=$VCPKG_INSTALLED_DIR -DVCPKG_MANIFEST_INSTALL=OFF"
rm -rf /tmp/core /tmp/pg /tmp/libs-inst /tmp/pg-cons
cmake -S libs/cpp-core -B /tmp/core $T -DCPP_CORE_BUILD_TESTS=OFF
cmake --build /tmp/core -j"$(nproc)"
cmake --install /tmp/core --prefix /tmp/libs-inst
cmake -S libs/cpp-pg -B /tmp/pg $T -DCMAKE_PREFIX_PATH=/tmp/libs-inst
cmake --build /tmp/pg -j"$(nproc)"
ctest --test-dir /tmp/pg -j8
cmake --install /tmp/pg --prefix /tmp/libs-inst
find /tmp/libs-inst -type f -path '*pg*' | sort
cmake -S libs/cpp-pg/tests/package-consumer -B /tmp/pg-cons $T -DCMAKE_PREFIX_PATH=/tmp/libs-inst
cmake --build /tmp/pg-cons
/tmp/pg-cons/package_consumer && echo OK
rm -rf /tmp/core /tmp/pg /tmp/libs-inst /tmp/pg-cons
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 51` (32 + 19), puis 16
fichiers (les 13 du guide 05, plus `Executor.hpp`, `Migrations.hpp` et
`QueryBuilder.hpp`), puis `OK`.

### Commit 1 : la lib

Sur l'hôte, depuis `~/matcha` :

```sh
git add backend/libs/cpp-pg
git status
```

✅ **Attendu** : sous `Changes to be committed`, 7 nouveaux fichiers (`Executor.hpp`,
`QueryBuilder.hpp`, `Migrations.hpp`, leurs deux `.cpp`, et les deux fichiers de tests)
et 11 fichiers modifiés, tous dans `backend/libs/cpp-pg/`.

```sh
git commit -m "cpp-pg 0.2.0 : Executor, QueryBuilder, migrations (audit 4.16, 4.17)"
```

La lib est finie. La suite branche le backend dessus.

---

## 8. Étape F : faire le ménage dans le backend

Avant de migrer les repositories, on retire ce qui n'a plus besoin de l'être : moins de
code à porter.

### 🎬 La situation

Un correcteur ouvre `backend/` pendant la soutenance. Il trouve une table `users`
(migration `001`) **et** une table `auth_accounts` (`002`), un `UserRepository` **et**
un `AccountRepository`, et des tests pour les deux. Il demande : « lequel sert à la
connexion ? ». La réponse est « `auth_accounts`, l'autre est un reste », mais rien dans
le dépôt ne le dit. Et chaque `make test` compile et lance des tests sur un chemin que
le serveur n'emprunte plus (audit 4.15).

### 💡 La notion : l'historique des migrations ne s'écrit qu'en ajoutant

On pourrait être tenté de supprimer `001_create_users.sql`. Déroulons ce qui se
passerait :

- **sur ta base de dev**, la version 1 est marquée appliquée : la table `users` existe
  et **restera** (plus aucun fichier ne la supprime), et `migrate` affichera
  `Applied migration has no file` à chaque démarrage ;
- **sur une base neuve**, `users` ne sera jamais créée.

Deux schémas différents selon l'âge de la base : la situation 3 de l'étape D, en pire.
La règle : **on ne modifie ni ne supprime une migration appliquée. On en ajoute une.**
Supprimer une table, c'est une migration comme une autre, `009`.

`DROP TABLE IF EXISTS` plutôt que `DROP TABLE` : si quelqu'un a déjà supprimé `users` à
la main sur sa base, la migration passe quand même au lieu d'empêcher le serveur de
démarrer.

**Et le trou du `006`** (audit 4.16) ? Ne le **comble pas**. Un nouveau `006_x.sql`
serait appliqué **entre 005 et 007** sur une base neuve, mais **après 009** sur ta base
(où 007 à 009 sont déjà passées). Le même fichier, deux ordres d'exécution différents.
Un trou ne gêne personne : on le **documente**.

### ⌨️ Le code

**Nouveau fichier `backend/migrations/009_drop_legacy_users.sql`** :

```sql
-- La table users (001) servait à l'ancien chemin d'authentification
-- (UserRepository), remplacé par auth_accounts (002). Plus rien ne la lit.
DROP TABLE IF EXISTS users;
```

**Nouveau fichier `backend/migrations/README.md`** : les règles du dossier, pour le
prochain qui y touchera (toi dans trois mois compris).

```markdown
# Migrations

Appliquées au démarrage du serveur par `pg::migrate` (cpp-pg), dans l'ordre de
leur numéro, en une seule transaction.

- **Nom** : `NNN_description.sql`, numéro sur 3 chiffres. Deux fichiers ne
  peuvent pas avoir le même numéro.
- **Ne jamais modifier un fichier déjà appliqué** : sa somme de contrôle
  (SHA-256) est enregistrée dans `schema_migrations`, et le serveur refuse de
  démarrer si le fichier a changé. Pour corriger, ajouter un nouveau fichier.
- **Essayer sans rien appliquer** : `matcha_backend --migrations-dry-run`
  applique les migrations en attente puis annule tout (`ROLLBACK`).
- **Pas de `BEGIN`/`COMMIT` dans un fichier** : le runner ouvre déjà la
  transaction. Donc pas non plus d'instruction interdite en transaction
  (`CREATE INDEX CONCURRENTLY`, `VACUUM`).

## Numéros absents

- **006** : n'a jamais existé dans ce dépôt (le numéro a été sauté). Aucune
  conséquence : seul l'ordre compte. Ne pas réutiliser ce numéro, il serait
  appliqué après 009 sur une base existante… et avant 007 sur une base neuve.
```

**Supprimer l'ancien chemin**, depuis `~/matcha` :

```sh
git rm backend/src/repositories/UserRepository.hpp backend/src/repositories/UserRepository.cpp \
    backend/tests/integration/UserRepositoryIntegrationTests.cpp \
    backend/tests/integration/AuthIntegrationTests.cpp \
    backend/tests/integration/AuthAuthentificationIntegrationTests.cpp
```

`AuthAuthentificationIntegrationTests.cpp` n'utilise pas la base : il teste
`auth::Authenticator` avec un faux validateur. Mais cpp-auth a déjà ce test, chez elle
(`AuthenticatorTest.DelegatesTokenValidation`). Il partait avec l'exécutable
`auth_integration_tests`, il part avec lui.

**`backend/CMakeLists.txt`** : supprime les blocs des deux exécutables qui
n'existent plus.

1. Tout le bloc `add_executable(user_repository_integration_tests …)`, avec son
   `target_include_directories` et son `target_link_libraries`.
2. Tout le bloc `add_executable(auth_integration_tests …)`, avec son
   `target_include_directories` et son `target_link_libraries`.
3. En bas, les deux lignes :

   ```cmake
   gtest_discover_tests(auth_integration_tests)
   gtest_discover_tests(user_repository_integration_tests)
   ```

### ✅ Vérifier

```sh
cmake --preset debug
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -N | tail -1
```

✅ **Attendu** : `Total Tests: 238` : 225 + 19 (cpp-pg 0.2) − 6 (4 de
`UserRepositoryIntegrationTest`, `AuthIntegrationTest` et
`AuthAuthenticationIntegrationTest`).

La migration `009`, elle, ne s'appliquera qu'à l'étape J, quand le serveur utilisera
`pg::migrate`.

### ❓ Point d'arrêt F

**F1.** Dans trois semaines, tu veux ajouter une colonne `fame_rating` aux profils. Tu
hésites entre ajouter la ligne dans `002_create_account_and_profile_schema.sql` (là où
la table est créée, « c'est plus propre ») et créer `010_add_fame_rating.sql`. Que se
passe-t-il au démarrage suivant dans chaque cas ?

<details><summary>Réponse</summary>

- **Modifier `002`** : sa somme de contrôle change. `migrate` refuse de démarrer :
  `migration 002_create_account_and_profile_schema.sql was modified after being
  applied`. Et sans somme de contrôle, ce serait pire : `002` ne serait jamais relancée
  sur ta base, la colonne n'existerait que sur les bases neuves.
- **Créer `010`** : `010` est la seule migration en attente, elle est appliquée, la
  colonne existe partout. C'est la seule bonne réponse.

</details>

---

## 9. Étape G : les repositories passent à cpp-pg

C'est l'étape la plus longue **à taper**, mais la plus mécanique : la logique SQL ne
change pas (les requêtes sont les mêmes, souvent au caractère près). Ce qui change, c'est
**comment** on envoie les valeurs et **comment** on lit les lignes.

> ⚠️ Entre cette étape et l'étape J, l'exécutable `matcha_backend` **ne compile plus** :
> `main.cpp` construit encore les repositories avec un `Database`. C'est normal, on le
> rebranche en J. D'ici là, on compile et on teste les repositories à travers
> l'exécutable de tests de l'étape H.

### 🎬 La situation 1 : la colonne qui décale tout

Ouvre l'actuel `ProfileRepository.cpp`. `readProfile` lit les colonnes **par numéro** :

```cpp
const auto orientation = result.getString(row, 6);
const auto completed = result.getBool(row, 9);
...
result.getString(row, 7), result.getString(row, 8), *completed, result.getInt64(row, 10)
```

Et **deux** requêtes différentes (`findProfile` et `listCompletedProfiles`) doivent
produire leurs colonnes **exactement dans cet ordre** (dans la seconde, la colonne 9 est
un `TRUE` écrit en dur). Le jour où quelqu'un ajoute `p.location_source` en position 5
dans l'une des deux :

- `getString(row, 6)` lit… `gender`, rangé dans `sexualOrientation` ;
- `getString(row, 7)` lit `sexual_orientation`, rangé dans `biography` ;
- `getBool(row, 9)` lit `city` (`"Paris"`), n'arrive pas à en faire un booléen, renvoie
  `nullopt`, et `readProfile` renvoie `std::nullopt` : **le profil disparaît** de la
  liste, sans erreur ni log.

### 🎬 La situation 2 : « voir plus » saute un profil

La page Découvrir charge les profils 24 par 24 avec `LIMIT 24 OFFSET n`. Prenons des
pages de 2 pour suivre à la main :

```
profils complets, triés :   A  B  C  D  E
page 1 : OFFSET 0 LIMIT 2   A  B           ← affichés
                                          … A supprime sa photo : son profil n'est plus complet
profils complets, triés :      B  C  D  E
page 2 : OFFSET 2 LIMIT 2         ·  D  E  ← C n'est jamais affiché
```

`OFFSET 2` veut dire « saute les 2 premières lignes **de la liste actuelle** », pas « reprends
après B ». La liste a bougé entre les deux pages : C est passé à la position 1, et il a
été sauté. Et un second défaut : `OFFSET 10000`, c'est 10 000 lignes que PostgreSQL lit,
trie, puis **jette**, à chaque page.

### 💡 La notion

**1. Le nom d'une colonne du résultat.** C'est le nom de la colonne de la table
(`p.username` → `username`), ou l'**alias** donné avec `AS`. Une **expression** sans
alias reçoit un nom fabriqué. Vérifié :

```sql
SELECT CASE WHEN true THEN 1 END, (SELECT 2);
```

```
 case | ?column?
------+----------
    1 |        2
```

Donc : **chaque expression lue par nom reçoit un `AS`**. Exemples dans ce qui suit :
`CASE … END AS other_account_id`, `(SELECT photo.id …) AS primary_photo_id`,
`m.read_at IS NOT NULL AS read`. Oublier l'alias n'est pas silencieux : `get` lève une
`pg::RowError` (« unknown column 'read' ») dès le premier test (guide 05 §1.8).

**2. `NULL` : la colonne le dit.** Une colonne `NOT NULL` se lit avec `get<T>` ; une
colonne qui peut être `NULL` (ou un `LEFT JOIN`, ou une sous-requête qui peut ne rien
trouver) avec `get<std::optional<T>>`. Les anciens `if (!id || !username …) continue;`
disparaissent : ils **cachaient** les incohérences en sautant des lignes. Un `NULL`
inattendu est maintenant une `RowError`, donc un bug visible.

**3. La pagination par curseur** (*keyset*). Au lieu de « saute n lignes », on dit
« **reprends après** la dernière ligne que j'ai vue » :

```sql
WHERE p.account_id > $after ORDER BY p.account_id LIMIT 24
```

Dans la situation 2 : la page 2 demande « après B », et obtient C, D. Que A ait disparu
ne change rien. Et PostgreSQL saute directement au bon endroit dans l'index de la clé
primaire, au lieu de lire et jeter les lignes d'avant.

Deux conditions pour que ça marche :

- le tri doit être **total** : deux lignes ne doivent jamais être à égalité. Ici
  `account_id`, unique. Pour un tri par popularité, on ajouterait l'id pour
  départager : `ORDER BY fame DESC, account_id DESC`, et un curseur à **deux** valeurs :
  `WHERE (fame, account_id) < ($1, $2)` (comparaison de lignes : d'abord `fame`, puis
  `account_id` en cas d'égalité ; c'est le test `KeysetConditionOnTwoColumns`) ;
- le curseur, c'est la **valeur** de la dernière ligne, pas sa position. L'API change :
  `/api/profiles?after=42` au lieu de `?offset=24`. Le front suivra à l'étape J.

L'ancien tri était `ORDER BY p.created_at, p.account_id`. Le profil est créé dans la
même requête que son compte, donc l'ordre des `account_id` est celui des dates de
création : trier sur `account_id` seul donne le même ordre, avec une seule colonne de
curseur.

**4. Le tableau de conversion**, pour tout ce qui reste :

| Avant (`Database`, `QueryResult`) | Après (cpp-pg) |
|---|---|
| `Database &_database` | `pg::Executor &_database` |
| `executeParams(sql, {std::to_string(id), email})` | `query(sql, id, email)` |
| `result.isError()` | `!result` |
| `result.rowCount()` | `result->size()` |
| `result.getInt64(0, 0)` + test du `optional` | `(*result)[0].get<std::int64_t>("id")` |
| `result.sqlState() == "23505"` | `result.error().code == core::ErrorCode::Conflict` |
| `return !result.isError();` | `return result.has_value();` |
| `std::to_string(lifetime.count())` | `lifetime.count()` (un entier : `toText` le convertit) |

### ⌨️ Le code

**`src/repositories/AccountRepository.hpp`** : trois changements.

1. Sous `#include <auth/credentials/IUserLookupRepository.hpp>`, ajoute
   `#include <pg/Executor.hpp>`.
2. Supprime la ligne `class Database;`.
3. `explicit AccountRepository(pg::Executor &database);` et, en bas,
   `pg::Executor &_database;`.

**`src/repositories/AccountRepository.cpp`**, en entier. Les requêtes sont celles
d'avant ; compare `createPendingAccountAndProfile` avec ton fichier actuel pour voir le
tableau de conversion à l'œuvre, puis le reste suit le même modèle.

```cpp
#include "AccountRepository.hpp"

AccountRepository::AccountRepository(pg::Executor &database):
	_database(database)
{}

AccountCreationResult AccountRepository::createPendingAccountAndProfile(
	const std::string &email,
	const std::string &passwordHash,
	const std::string &username,
	const std::string &firstName,
	const std::string &lastName,
	const std::string &verificationTokenHash,
	std::chrono::seconds verificationLifetime
)
{
	if (verificationLifetime <= std::chrono::seconds::zero())
		return {
			AccountCreationResult::Status::DatabaseError,
			0
		};

	const auto result = this->_database.query(
		"WITH account AS ("
		"  INSERT INTO auth_accounts "
		"    (email, password_hash, email_verification_last_sent_at) "
		"  VALUES ($1, $2, NOW()) "
		"  RETURNING id"
		"), profile AS ("
		"  INSERT INTO matcha_profiles "
		"    (account_id, username, first_name, last_name) "
		"  SELECT id, $3, $4, $5 FROM account "
		"  RETURNING account_id"
		"), verification_token AS ("
		"  INSERT INTO email_verification_tokens "
		"    (account_id, token_hash, expires_at) "
		"  SELECT account_id, $6, "
		"    NOW() + ($7 || ' seconds')::interval "
		"  FROM profile "
		"  RETURNING account_id"
		") "
		"SELECT account_id FROM verification_token",
		email,
		passwordHash,
		username,
		firstName,
		lastName,
		verificationTokenHash,
		verificationLifetime.count()
	);

	if (!result)
	{
		if (result.error().code == core::ErrorCode::Conflict)
			return {
				AccountCreationResult::Status::IdentifierAlreadyExists,
				0
			};
		return {
			AccountCreationResult::Status::DatabaseError,
			0
		};
	}

	if (result->size() != 1)
		return {
			AccountCreationResult::Status::DatabaseError,
			0
		};

	return {
		AccountCreationResult::Status::Success,
		(*result)[0].get<std::int64_t>("account_id")
	};
}

EmailVerificationResult AccountRepository::verifyEmailToken(
	const std::string &tokenHash
)
{
	const auto result = this->_database.query(
		"WITH matched AS ("
		"  SELECT id, account_id FROM email_verification_tokens "
		"  WHERE token_hash = $1 AND consumed_at IS NULL "
		"    AND expires_at > NOW() "
		"  FOR UPDATE"
		"), consumed AS ("
		"  UPDATE email_verification_tokens t "
		"  SET consumed_at = NOW() "
		"  FROM matched m "
		"  WHERE t.id = m.id "
		"  RETURNING t.account_id"
		"), verified AS ("
		"  UPDATE auth_accounts a "
		"  SET email_verified_at = NOW(), updated_at = NOW() "
		"  FROM consumed c "
		"  WHERE a.id = c.account_id AND a.email_verified_at IS NULL "
		"  RETURNING a.id"
		") "
		"SELECT id FROM verified",
		tokenHash
	);

	if (!result)
		return {EmailVerificationResult::Status::DatabaseError};
	if (result->size() == 1)
		return {EmailVerificationResult::Status::Verified};
	return {EmailVerificationResult::Status::InvalidOrExpired};
}

ResendVerificationResult AccountRepository::issueEmailVerificationToken(
	const std::string &email,
	const std::string &tokenHash,
	std::chrono::seconds tokenLifetime
)
{
	if (tokenLifetime <= std::chrono::seconds::zero())
		return {ResendVerificationResult::Status::DatabaseError, {}};

	const auto result = this->_database.query(
		"WITH eligible AS ("
		"  UPDATE auth_accounts "
		"  SET email_verification_last_sent_at = NOW(), updated_at = NOW() "
		"  WHERE LOWER(email) = LOWER($1) "
		"    AND email_verified_at IS NULL "
		"    AND (email_verification_last_sent_at IS NULL "
		"      OR email_verification_last_sent_at <= NOW() - INTERVAL '60 seconds') "
		"  RETURNING id, email"
		"), consumed AS ("
		"  UPDATE email_verification_tokens t "
		"  SET consumed_at = NOW() "
		"  FROM eligible e "
		"  WHERE t.account_id = e.id AND t.consumed_at IS NULL "
		"  RETURNING t.id"
		"), inserted AS ("
		"  INSERT INTO email_verification_tokens "
		"    (account_id, token_hash, expires_at) "
		"  SELECT e.id, $2, NOW() + ($3 || ' seconds')::interval "
		"  FROM eligible e "
		"  WHERE (SELECT COUNT(*) FROM consumed) >= 0 "
		"  RETURNING account_id"
		") "
		"SELECT e.email FROM eligible e "
		"JOIN inserted i ON i.account_id = e.id",
		email,
		tokenHash,
		tokenLifetime.count()
	);

	if (!result)
		return {ResendVerificationResult::Status::DatabaseError, {}};
	if (result->size() != 1)
		return {ResendVerificationResult::Status::Suppressed, {}};
	return {
		ResendVerificationResult::Status::Issued,
		(*result)[0].get<std::string>("email")
	};
}

SessionResult AccountRepository::createSession(
	std::int64_t accountId,
	const std::string &refreshTokenHash,
	std::chrono::seconds idleLifetime,
	std::chrono::seconds absoluteLifetime
)
{
	if (idleLifetime <= std::chrono::seconds::zero() ||
		absoluteLifetime < idleLifetime)
		return {SessionResult::Status::DatabaseError, 0, {}};

	const auto result = this->_database.query(
		"WITH account AS ("
		"  SELECT id, email FROM auth_accounts "
		"  WHERE id = $1 AND email_verified_at IS NOT NULL"
		"), session AS ("
		"  INSERT INTO user_sessions "
		"    (account_id, idle_expires_at, absolute_expires_at) "
		"  SELECT id, NOW() + ($3 || ' seconds')::interval, "
		"    NOW() + ($4 || ' seconds')::interval "
		"  FROM account "
		"  RETURNING id, account_id, idle_expires_at"
		"), refresh AS ("
		"  INSERT INTO refresh_tokens (session_id, token_hash, expires_at) "
		"  SELECT id, $2, idle_expires_at FROM session "
		"  RETURNING session_id"
		") "
		"SELECT a.id, a.email FROM account a "
		"JOIN session s ON s.account_id = a.id "
		"JOIN refresh r ON r.session_id = s.id",
		accountId,
		refreshTokenHash,
		idleLifetime.count(),
		absoluteLifetime.count()
	);
	if (!result)
		return {SessionResult::Status::DatabaseError, 0, {}};
	if (result->size() != 1)
		return {SessionResult::Status::InvalidToken, 0, {}};

	const pg::Row row = (*result)[0];
	return {
		SessionResult::Status::Success,
		row.get<std::int64_t>("id"),
		row.get<std::string>("email")
	};
}

SessionResult AccountRepository::rotateRefreshToken(
	const std::string &currentTokenHash,
	const std::string &replacementTokenHash,
	std::chrono::seconds idleLifetime
)
{
	if (idleLifetime <= std::chrono::seconds::zero())
		return {SessionResult::Status::DatabaseError, 0, {}};

	const auto result = this->_database.query(
		"WITH candidate AS ("
		"  SELECT t.id AS token_id, t.session_id, t.expires_at AS token_expires_at, "
		"    t.consumed_at, s.account_id, s.absolute_expires_at, "
		"    s.idle_expires_at, s.revoked_at "
		"  FROM refresh_tokens t "
		"  JOIN user_sessions s ON s.id = t.session_id "
		"  WHERE t.token_hash = $1 "
		"  FOR UPDATE OF t, s"
		"), valid AS ("
		"  SELECT * FROM candidate "
		"  WHERE consumed_at IS NULL AND token_expires_at > NOW() "
		"    AND idle_expires_at > NOW() AND absolute_expires_at > NOW() "
		"    AND revoked_at IS NULL"
		"), consumed AS ("
		"  UPDATE refresh_tokens t SET consumed_at = NOW() "
		"  FROM valid v WHERE t.id = v.token_id "
		"  RETURNING t.session_id"
		"), extended AS ("
		"  UPDATE user_sessions s "
		"  SET last_used_at = NOW(), "
		"    idle_expires_at = LEAST("
		"      NOW() + ($3 || ' seconds')::interval, "
		"      s.absolute_expires_at"
		"    ) "
		"  FROM consumed c WHERE s.id = c.session_id "
		"  RETURNING s.id, s.account_id, s.idle_expires_at"
		"), inserted AS ("
		"  INSERT INTO refresh_tokens (session_id, token_hash, expires_at) "
		"  SELECT id, $2, idle_expires_at FROM extended "
		"  RETURNING session_id"
		"), replayed AS ("
		"  UPDATE user_sessions s SET revoked_at = NOW() "
		"  FROM candidate c "
		"  WHERE s.id = c.session_id AND c.consumed_at IS NOT NULL "
		"    AND s.revoked_at IS NULL "
		"  RETURNING s.id"
		") "
		"SELECT a.id, a.email FROM inserted i "
		"JOIN user_sessions s ON s.id = i.session_id "
		"JOIN auth_accounts a ON a.id = s.account_id",
		currentTokenHash,
		replacementTokenHash,
		idleLifetime.count()
	);
	if (!result)
		return {SessionResult::Status::DatabaseError, 0, {}};
	if (result->size() != 1)
		return {SessionResult::Status::InvalidToken, 0, {}};

	const pg::Row row = (*result)[0];
	return {
		SessionResult::Status::Success,
		row.get<std::int64_t>("id"),
		row.get<std::string>("email")
	};
}

bool AccountRepository::revokeSessionByRefreshToken(
	const std::string &refreshTokenHash
)
{
	return this->_database.query(
		"UPDATE user_sessions s SET revoked_at = NOW() "
		"FROM refresh_tokens t "
		"WHERE t.token_hash = $1 AND t.session_id = s.id "
		"AND s.revoked_at IS NULL",
		refreshTokenHash
	).has_value();
}

bool AccountRepository::revokeAllSessions(std::int64_t accountId)
{
	return this->_database.query(
		"UPDATE user_sessions SET revoked_at = NOW() "
		"WHERE account_id = $1 AND revoked_at IS NULL",
		accountId
	).has_value();
}

PasswordResetRequestResult AccountRepository::issuePasswordResetToken(
	const std::string &email,
	const std::string &tokenHash,
	std::chrono::seconds tokenLifetime
)
{
	if (tokenLifetime <= std::chrono::seconds::zero())
		return {PasswordResetRequestResult::Status::DatabaseError, {}};

	const auto result = this->_database.query(
		"WITH eligible AS ("
		"  UPDATE auth_accounts "
		"  SET password_reset_last_sent_at = NOW(), updated_at = NOW() "
		"  WHERE LOWER(email) = LOWER($1) "
		"    AND email_verified_at IS NOT NULL "
		"    AND (password_reset_last_sent_at IS NULL "
		"      OR password_reset_last_sent_at <= NOW() - INTERVAL '60 seconds') "
		"  RETURNING id, email"
		"), consumed AS ("
		"  UPDATE password_reset_tokens t SET consumed_at = NOW() "
		"  FROM eligible e "
		"  WHERE t.account_id = e.id AND t.consumed_at IS NULL "
		"  RETURNING t.id"
		"), inserted AS ("
		"  INSERT INTO password_reset_tokens "
		"    (account_id, token_hash, expires_at) "
		"  SELECT e.id, $2, NOW() + ($3 || ' seconds')::interval "
		"  FROM eligible e "
		"  WHERE (SELECT COUNT(*) FROM consumed) >= 0 "
		"  RETURNING account_id"
		") "
		"SELECT e.email FROM eligible e "
		"JOIN inserted i ON i.account_id = e.id",
		email,
		tokenHash,
		tokenLifetime.count()
	);
	if (!result)
		return {PasswordResetRequestResult::Status::DatabaseError, {}};
	if (result->size() != 1)
		return {PasswordResetRequestResult::Status::Suppressed, {}};
	return {
		PasswordResetRequestResult::Status::Issued,
		(*result)[0].get<std::string>("email")
	};
}

PasswordResetResult AccountRepository::resetPassword(
	const std::string &tokenHash,
	const std::string &passwordHash
)
{
	const auto result = this->_database.query(
		"WITH candidate AS ("
		"  SELECT t.id AS token_id, t.account_id "
		"  FROM password_reset_tokens t "
		"  JOIN auth_accounts a ON a.id = t.account_id "
		"  WHERE t.token_hash = $1 AND t.consumed_at IS NULL "
		"    AND t.expires_at > NOW() AND a.email_verified_at IS NOT NULL "
		"  FOR UPDATE OF t, a"
		"), updated_account AS ("
		"  UPDATE auth_accounts a "
		"  SET password_hash = $2, updated_at = NOW() "
		"  FROM candidate c WHERE a.id = c.account_id "
		"  RETURNING a.id"
		"), consumed_tokens AS ("
		"  UPDATE password_reset_tokens t "
		"  SET consumed_at = NOW() "
		"  FROM updated_account a "
		"  WHERE t.account_id = a.id AND t.consumed_at IS NULL "
		"  RETURNING t.account_id"
		"), revoked_sessions AS ("
		"  UPDATE user_sessions s SET revoked_at = NOW() "
		"  FROM updated_account a "
		"  WHERE s.account_id = a.id AND s.revoked_at IS NULL "
		"  RETURNING s.id"
		") "
		"SELECT id FROM updated_account",
		tokenHash,
		passwordHash
	);
	if (!result)
		return {PasswordResetResult::Status::DatabaseError};
	if (result->size() == 1)
		return {PasswordResetResult::Status::Reset};
	return {PasswordResetResult::Status::InvalidOrExpired};
}

namespace
{
	auth::UserLookupResult userFrom(const core::Result<pg::Rows> &result)
	{
		if (!result)
			return {
				auth::UserLookupResult::Status::DatabaseError,
				std::nullopt
			};
		if (result->empty())
			return {
				auth::UserLookupResult::Status::NotFound,
				std::nullopt
			};
		const pg::Row row = (*result)[0];
		return {
			auth::UserLookupResult::Status::Found,
			auth::User{
				row.get<std::int64_t>("id"),
				row.get<std::string>("email"),
				row.get<std::string>("password_hash")
			}
		};
	}
}

auth::UserLookupResult AccountRepository::findByEmail(
	const std::string &email
)
{
	return userFrom(this->_database.query(
		"SELECT id, email, password_hash FROM auth_accounts "
		"WHERE LOWER(email) = LOWER($1) AND email_verified_at IS NOT NULL",
		email
	));
}

auth::UserLookupResult AccountRepository::findById(
	const std::int64_t id
)
{
	return userFrom(this->_database.query(
		"SELECT id, email, password_hash FROM auth_accounts "
		"WHERE id = $1 AND email_verified_at IS NOT NULL",
		id
	));
}
```

- `findByEmail` et `findById` lisaient leur résultat avec le même code, copié deux
  fois : il est maintenant dans `userFrom`, dans un `namespace` anonyme.
- `createPendingAccountAndProfile` : un email ou un nom d'utilisateur déjà pris, c'est
  `23505`, que cpp-pg traduit en `Conflict` (guide 05 §1.7).

**`src/repositories/LikeRepository.hpp`**, en entier :

```cpp
#pragma once

#include <core/Result.hpp>
#include <pg/Executor.hpp>

#include <cstdint>
#include <optional>
#include <vector>

struct LikeResult
{
	bool liked;
	bool matched;
	std::optional<std::int64_t> matchId;
};

struct MatchRecord
{
	std::int64_t id;
	std::int64_t otherAccountId;
};

class LikeRepository
{
	public:
		explicit LikeRepository(pg::Executor &database);

		// À appeler dans la transaction de MatchService : seul, un like
		// simultané dans l'autre sens peut faire rater le match.
		core::Result<LikeResult> like(std::int64_t likerAccountId,
			std::int64_t likedAccountId);
		// true si un like a été supprimé.
		core::Result<bool> unlike(std::int64_t likerAccountId,
			std::int64_t likedAccountId);
		std::vector<std::int64_t> listLikedAccountIds(
			std::int64_t likerAccountId);
		std::vector<std::int64_t> listIncomingLikeAccountIds(
			std::int64_t likedAccountId);
		std::vector<MatchRecord> listMatches(std::int64_t accountId);

	private:
		pg::Executor &_database;
};
```

Trois changements de **contrat**, pas seulement de syntaxe :

- `like` et `unlike` renvoient un `core::Result` au lieu de lever une exception. Leur
  seul appelant sera `MatchService` (étape I), qui doit pouvoir **annuler sa
  transaction** proprement en cas d'erreur.
- `unlike` renvoie `core::Result<bool>` : l'erreur, ou « un like a-t-il été supprimé ? ».
- `listAllMatches` disparaît. Il ne servait qu'à la resynchronisation des conversations
  au démarrage, qui disparaît à l'étape J.

**`src/repositories/LikeRepository.cpp`**, en entier :

```cpp
#include "LikeRepository.hpp"

#include <stdexcept>

namespace
{
	std::vector<std::int64_t> accountIds(const pg::Rows &rows, const char *column)
	{
		std::vector<std::int64_t> ids;
		for (const pg::Row row : rows)
			ids.push_back(row.get<std::int64_t>(column));
		return ids;
	}
}

LikeRepository::LikeRepository(pg::Executor &database):
	_database(database)
{
}

core::Result<LikeResult> LikeRepository::like(std::int64_t likerAccountId,
	std::int64_t likedAccountId)
{
	const auto result = _database.query(R"(
		WITH inserted_like AS (
			INSERT INTO profile_likes (liker_account_id, liked_account_id)
			VALUES ($1, $2)
			ON CONFLICT DO NOTHING
		), ensured_match AS (
			INSERT INTO profile_matches (first_account_id, second_account_id)
			SELECT LEAST($1::BIGINT, $2::BIGINT),
				GREATEST($1::BIGINT, $2::BIGINT)
			WHERE EXISTS (
				SELECT 1 FROM profile_likes
				WHERE liker_account_id = $2 AND liked_account_id = $1
			)
			ON CONFLICT (first_account_id, second_account_id)
			DO UPDATE SET created_at = profile_matches.created_at
			RETURNING id
		)
		SELECT id FROM ensured_match
	)", likerAccountId, likedAccountId);
	if (!result)
		return std::unexpected(result.error());
	if (result->empty())
		return LikeResult{true, false, std::nullopt};
	return LikeResult{true, true, (*result)[0].get<std::int64_t>("id")};
}

std::vector<std::int64_t> LikeRepository::listIncomingLikeAccountIds(
	std::int64_t likedAccountId)
{
	const auto result = _database.query(
		"SELECT liker_account_id FROM profile_likes "
		"WHERE liked_account_id = $1 ORDER BY created_at DESC",
		likedAccountId);
	if (!result)
		throw std::runtime_error("Unable to list incoming profile likes");
	return accountIds(*result, "liker_account_id");
}

core::Result<bool> LikeRepository::unlike(std::int64_t likerAccountId,
	std::int64_t likedAccountId)
{
	const auto result = _database.query(R"(
		WITH deleted_like AS (
			DELETE FROM profile_likes
			WHERE liker_account_id = $1 AND liked_account_id = $2
			RETURNING 1
		), deleted_match AS (
			DELETE FROM profile_matches
			WHERE first_account_id = LEAST($1::BIGINT, $2::BIGINT)
				AND second_account_id = GREATEST($1::BIGINT, $2::BIGINT)
				AND EXISTS (SELECT 1 FROM deleted_like)
			RETURNING 1
		)
		SELECT EXISTS (SELECT 1 FROM deleted_like) AS removed
	)", likerAccountId, likedAccountId);
	if (!result)
		return std::unexpected(result.error());
	return (*result)[0].get<bool>("removed");
}

std::vector<std::int64_t> LikeRepository::listLikedAccountIds(
	std::int64_t likerAccountId)
{
	const auto result = _database.query(
		"SELECT liked_account_id FROM profile_likes "
		"WHERE liker_account_id = $1 ORDER BY created_at DESC",
		likerAccountId);
	if (!result)
		throw std::runtime_error("Unable to list profile likes");
	return accountIds(*result, "liked_account_id");
}

std::vector<MatchRecord> LikeRepository::listMatches(std::int64_t accountId)
{
	const auto result = _database.query(R"(
		SELECT id,
			CASE WHEN first_account_id = $1
				THEN second_account_id ELSE first_account_id END
				AS other_account_id
		FROM profile_matches
		WHERE first_account_id = $1 OR second_account_id = $1
		ORDER BY created_at DESC
	)", accountId);
	if (!result)
		throw std::runtime_error("Unable to list profile matches");
	std::vector<MatchRecord> matches;
	for (const pg::Row row : *result)
		matches.push_back({row.get<std::int64_t>("id"),
			row.get<std::int64_t>("other_account_id")});
	return matches;
}
```

`accountIds` lit une colonne d'identifiants pour toutes les lignes : les deux listes de
likes (envoyés, reçus) l'utilisent. Note le `AS other_account_id` dans `listMatches` et
le `AS removed` dans `unlike`.

**`src/messaging/MessagingRepository.hpp`** : `#include <pg/Executor.hpp>` après
`#pragma once`, supprime `class Database;`, et `pg::Executor &` dans le constructeur et
le membre (comme pour `AccountRepository`).

**`src/messaging/MessagingRepository.cpp`**, en entier :

```cpp
#include "MessagingRepository.hpp"

#include <stdexcept>

namespace
{
	MessagingMessage readMessage(const pg::Row &row, std::int64_t conversationId)
	{
		return MessagingMessage{
			row.get<std::int64_t>("id"),
			conversationId,
			row.get<std::int64_t>("sender_account_id"),
			row.get<std::string>("body"),
			row.get<std::string>("created_at"),
			row.get<bool>("read")};
	}
}

MessagingRepository::MessagingRepository(pg::Executor &database):
	_database(database)
{}

std::optional<std::int64_t> MessagingRepository::ensureDirectConversation(
	std::int64_t firstAccountId, std::int64_t secondAccountId)
{
	if (firstAccountId <= 0 || secondAccountId <= 0 ||
		firstAccountId == secondAccountId)
		return std::nullopt;
	const auto result = _database.query(R"(
		WITH locked AS (
			SELECT pg_advisory_xact_lock(
				hashtextextended(
					LEAST($1::BIGINT, $2::BIGINT)::TEXT || ':' ||
					GREATEST($1::BIGINT, $2::BIGINT)::TEXT,
					0
				)
			)
		), existing AS (
			SELECT c.id
			FROM messaging_conversations c
			JOIN messaging_conversation_participants p
				ON p.conversation_id = c.id
			CROSS JOIN locked
			WHERE p.account_id IN ($1, $2)
			GROUP BY c.id
			HAVING COUNT(DISTINCT p.account_id) = 2
			AND COUNT(*) = 2
			LIMIT 1
		), created AS (
			INSERT INTO messaging_conversations
			SELECT FROM (SELECT 1) AS seed
			WHERE NOT EXISTS (SELECT 1 FROM existing)
			RETURNING id
		), participants AS (
			INSERT INTO messaging_conversation_participants
				(conversation_id, account_id)
			SELECT id, $1 FROM created
			UNION ALL
			SELECT id, $2 FROM created
			RETURNING conversation_id
		), selected AS (
			SELECT id FROM existing
			UNION ALL
			SELECT conversation_id FROM participants
		)
		SELECT id FROM selected
		LIMIT 1
	)", firstAccountId, secondAccountId);
	if (!result || result->size() != 1)
		return std::nullopt;
	return (*result)[0].get<std::int64_t>("id");
}

bool MessagingRepository::removeDirectConversation(
	std::int64_t firstAccountId, std::int64_t secondAccountId)
{
	const auto result = _database.query(R"(
		DELETE FROM messaging_conversations c
		WHERE c.id IN (
			SELECT p.conversation_id
			FROM messaging_conversation_participants p
			WHERE p.account_id IN ($1, $2)
			GROUP BY p.conversation_id
			HAVING COUNT(DISTINCT p.account_id) = 2 AND COUNT(*) = 2
		)
	)", firstAccountId, secondAccountId);
	return result.has_value();
}

std::vector<MessagingConversation> MessagingRepository::listConversations(
	std::int64_t accountId)
{
	const auto result = _database.query(R"(
		SELECT c.id,
			(SELECT p2.account_id
			 FROM messaging_conversation_participants p2
			 WHERE p2.conversation_id = c.id AND p2.account_id <> $1
			 LIMIT 1) AS other_participant_id,
			(SELECT COUNT(*) FROM messaging_messages m
			 WHERE m.conversation_id = c.id
			 AND m.sender_account_id <> $1 AND m.read_at IS NULL)
			 AS unread_count
		FROM messaging_conversations c
		JOIN messaging_conversation_participants p
			ON p.conversation_id = c.id AND p.account_id = $1
		ORDER BY c.id DESC
	)", accountId);
	if (!result)
		throw std::runtime_error("Unable to list conversations");
	std::vector<MessagingConversation> conversations;
	for (const pg::Row row : *result)
	{
		// NULL si l'autre participant a supprimé son compte : la conversation est ignorée.
		const auto other = row.get<std::optional<std::int64_t>>("other_participant_id");
		if (other)
			conversations.push_back({row.get<std::int64_t>("id"), *other,
				row.get<std::int64_t>("unread_count")});
	}
	return conversations;
}

std::vector<MessagingMessage> MessagingRepository::listMessages(
	std::int64_t accountId, std::int64_t conversationId,
	std::int64_t beforeId, std::int32_t limit)
{
	const auto result = _database.query(R"(
		SELECT m.id, m.sender_account_id, m.body,
			m.created_at::text AS created_at, m.read_at IS NOT NULL AS read
		FROM messaging_messages m
		JOIN messaging_conversation_participants p
			ON p.conversation_id = m.conversation_id
			AND p.account_id = $2
		WHERE m.conversation_id = $1
		AND ($3 = 0 OR m.id < $3)
		ORDER BY m.id DESC LIMIT $4
	)", conversationId, accountId, beforeId, limit);
	if (!result)
		throw std::runtime_error("Unable to list messages");
	std::vector<MessagingMessage> messages;
	for (const pg::Row row : *result)
		messages.push_back(readMessage(row, conversationId));
	return messages;
}

std::optional<MessagingMessage> MessagingRepository::sendMessage(
	std::int64_t accountId, std::int64_t conversationId,
	const std::string &body)
{
	const auto result = _database.query(R"(
		WITH participant AS (
			SELECT conversation_id FROM messaging_conversation_participants
			WHERE conversation_id = $1 AND account_id = $2
		), rate AS (
			SELECT COUNT(*) AS count FROM messaging_messages
			WHERE sender_account_id = $2
			AND created_at > NOW() - INTERVAL '1 minute'
		), inserted AS (
			INSERT INTO messaging_messages
				(conversation_id, sender_account_id, body)
			SELECT p.conversation_id, $2, BTRIM($3)
			FROM participant p, rate WHERE rate.count < 30
			RETURNING id, sender_account_id, body,
				created_at::text AS created_at, false AS read
		)
		SELECT * FROM inserted
	)", conversationId, accountId, body);
	if (!result)
		throw std::runtime_error("Unable to send message");
	if (result->size() != 1)
		return std::nullopt;
	return readMessage((*result)[0], conversationId);
}

std::optional<std::int64_t> MessagingRepository::otherParticipant(
	std::int64_t accountId, std::int64_t conversationId)
{
	const auto result = _database.query(R"(
		SELECT p.account_id
		FROM messaging_conversation_participants p
		WHERE p.conversation_id = $1 AND p.account_id <> $2
		AND EXISTS (
			SELECT 1 FROM messaging_conversation_participants own
			WHERE own.conversation_id = p.conversation_id
			AND own.account_id = $2
		)
		LIMIT 1
	)", conversationId, accountId);
	if (!result || result->size() != 1)
		return std::nullopt;
	return (*result)[0].get<std::int64_t>("account_id");
}

bool MessagingRepository::markRead(
	std::int64_t accountId, std::int64_t conversationId)
{
	const auto result = _database.query(R"(
		UPDATE messaging_messages m SET read_at = NOW()
		WHERE m.conversation_id = $1 AND m.sender_account_id <> $2
		AND m.read_at IS NULL
		AND EXISTS (
			SELECT 1 FROM messaging_conversation_participants p
			WHERE p.conversation_id = m.conversation_id
			AND p.account_id = $2
		)
	)", conversationId, accountId);
	return result.has_value();
}
```

- `listConversations` : la sous-requête qui cherche l'autre participant peut ne rien
  trouver (il a supprimé son compte : `ON DELETE CASCADE` a retiré sa ligne de
  participant). D'où `get<std::optional<std::int64_t>>`, et la conversation ignorée,
  comme avant, mais maintenant **explicitement**.
- `created_at::text AS created_at` : on garde le texte que renvoyait l'ancien code
  (l'API JSON le transmet tel quel). Lire un `pg::Timestamp` serait plus propre, mais
  changerait le format envoyé au front : pas dans ce guide.

**`src/repositories/ProfileRepository.hpp`** : les mêmes trois changements, plus la
signature de `listCompletedProfiles` :

```cpp
		// Pagination par curseur : afterAccountId est le dernier compte de la
		// page précédente (rien pour la première page).
		std::vector<ProfileData> listCompletedProfiles(
			std::int64_t excludingAccountId,
			std::int32_t limit,
			std::optional<std::int64_t> afterAccountId
		);
```

**`src/repositories/ProfileRepository.cpp`**, en entier :

```cpp
#include "ProfileRepository.hpp"

#include <pg/QueryBuilder.hpp>

ProfileRepository::ProfileRepository(pg::Executor &database):
	_database(database)
{}

namespace
{
	// Les colonnes lues par readProfile, dans toutes les requêtes de profil.
	constexpr const char *PROFILE_COLUMNS =
		"p.account_id, p.username, p.first_name, p.last_name, "
		"p.date_of_birth::text AS date_of_birth, p.gender, p.sexual_orientation, "
		"p.biography, p.city, "
		"(SELECT photo.id FROM matcha_profile_photos photo "
		"WHERE photo.profile_account_id = p.account_id "
		"AND photo.is_primary LIMIT 1) AS primary_photo_id";

	// Un profil est complet quand ces conditions sont toutes vraies.
	constexpr const char *PROFILE_COMPLETED =
		"p.date_of_birth IS NOT NULL "
		"AND p.date_of_birth <= CURRENT_DATE - INTERVAL '18 years' "
		"AND p.gender IS NOT NULL "
		"AND NULLIF(BTRIM(p.biography), '') IS NOT NULL "
		"AND p.city IS NOT NULL "
		"AND p.city ~ '^[[:alpha:]][[:alpha:] .''-]*$' "
		"AND EXISTS (SELECT 1 FROM matcha_profile_photos photo "
		"WHERE photo.profile_account_id = p.account_id "
		"AND photo.is_primary)";

	// La recherche avancée ajoutera ici d'autres tris (âge, distance, popularité).
	const pg::SortWhitelist PROFILE_SORTS{{"account", "p.account_id"}};

	ProfileData readProfile(const pg::Row &row)
	{
		return ProfileData{
			row.get<std::int64_t>("account_id"),
			row.get<std::string>("username"),
			row.get<std::string>("first_name"),
			row.get<std::string>("last_name"),
			row.get<std::optional<std::string>>("date_of_birth"),
			row.get<std::optional<std::string>>("gender"),
			row.get<std::string>("sexual_orientation"),
			row.get<std::optional<std::string>>("biography"),
			row.get<std::optional<std::string>>("city"),
			row.get<bool>("completed"),
			row.get<std::optional<std::int64_t>>("primary_photo_id")
		};
	}
}

std::optional<ProfileData> ProfileRepository::findProfile(
	std::int64_t accountId
)
{
	const auto result = _database.query(
		std::string("SELECT ") + PROFILE_COLUMNS + ", (" + PROFILE_COMPLETED +
		") AS completed FROM matcha_profiles p WHERE p.account_id = $1",
		accountId
	);
	if (!result || result->size() != 1)
		return std::nullopt;
	return readProfile((*result)[0]);
}

std::vector<ProfileData> ProfileRepository::listCompletedProfiles(
	std::int64_t excludingAccountId,
	std::int32_t limit,
	std::optional<std::int64_t> afterAccountId
)
{
	pg::QueryBuilder builder(std::string("SELECT ") + PROFILE_COLUMNS +
		", TRUE AS completed FROM matcha_profiles p");
	builder.where("p.account_id <> ?", excludingAccountId)
		.where(PROFILE_COMPLETED)
		.whereIf(afterAccountId, "p.account_id > ?")
		.orderBy("account", PROFILE_SORTS)
		.limit(limit);
	const auto query = builder.build();
	std::vector<ProfileData> profiles;
	if (!query)
		return profiles;
	const auto result = _database.execute(query->sql, query->params);
	if (!result)
		return profiles;
	for (const pg::Row row : *result)
		profiles.push_back(readProfile(row));
	return profiles;
}

std::optional<std::int64_t> ProfileRepository::findPhotoOwner(
	std::int64_t photoId
)
{
	const auto result = _database.query(
		"SELECT profile_account_id FROM matcha_profile_photos "
		"WHERE id = $1",
		photoId
	);
	if (!result || result->size() != 1)
		return std::nullopt;
	return (*result)[0].get<std::int64_t>("profile_account_id");
}

bool ProfileRepository::updateProfile(
	std::int64_t accountId,
	const std::string &dateOfBirth,
	const std::string &gender,
	const std::string &sexualOrientation,
	const std::string &biography,
	const std::string &city
)
{
	const auto result = _database.query(
		"UPDATE matcha_profiles SET date_of_birth = NULLIF($2, '')::date, "
		"gender = NULLIF($3, ''), sexual_orientation = $4, "
		"biography = NULLIF($5, ''), city = NULLIF($6, ''), "
		"completed_at = CASE WHEN NULLIF($2, '') IS NOT NULL "
		"AND NULLIF($3, '') IS NOT NULL "
		"AND NULLIF(BTRIM($5), '') IS NOT NULL "
		"AND NULLIF($6, '') IS NOT NULL "
		"AND NULLIF(BTRIM($6), '') ~ '^[[:alpha:]][[:alpha:] .''-]*$' "
		"AND NULLIF($2, '')::date <= CURRENT_DATE - INTERVAL '18 years' "
		"AND EXISTS (SELECT 1 FROM matcha_profile_photos "
		"WHERE profile_account_id = $1 AND is_primary) "
		"THEN COALESCE(completed_at, NOW()) ELSE NULL END, "
		"updated_at = NOW() WHERE account_id = $1 "
		"RETURNING account_id",
		accountId,
		dateOfBirth,
		gender,
		sexualOrientation,
		biography,
		city
	);
	return result && result->size() == 1;
}

std::vector<ProfilePhoto> ProfileRepository::listPhotos(
	std::int64_t accountId
)
{
	const auto result = _database.query(
		"SELECT id, storage_key, position, is_primary, content_type, "
		"file_size, width, height, file_hash "
		"FROM matcha_profile_photos WHERE profile_account_id = $1 "
		"ORDER BY position",
		accountId
	);
	std::vector<ProfilePhoto> photos;
	if (!result)
		return photos;
	for (const pg::Row row : *result)
	{
		photos.push_back({
			row.get<std::int64_t>("id"),
			row.get<std::int16_t>("position"),
			row.get<bool>("is_primary"),
			row.get<std::string>("storage_key"),
			{row.get<std::string>("content_type"),
				row.get<std::size_t>("file_size"),
				row.get<std::uint32_t>("width"),
				row.get<std::uint32_t>("height"),
				row.get<std::string>("file_hash")}
		});
	}
	return photos;
}

std::optional<ProfilePhoto> ProfileRepository::addPhoto(
	std::int64_t accountId,
	const std::string &storageKey,
	const ImageMetadata &metadata,
	std::int16_t position,
	bool isPrimary
)
{
	const auto result = _database.query(
		"INSERT INTO matcha_profile_photos "
		"(profile_account_id, storage_key, position, is_primary, content_type, "
		"file_size, width, height, file_hash) VALUES "
		"($1, $2, $3, $4, $5, $6, $7, $8, $9) "
		"RETURNING id",
		accountId, storageKey, position, isPrimary, metadata.contentType,
		metadata.fileSize, metadata.width, metadata.height, metadata.sha256
	);
	if (!result || result->size() != 1)
		return std::nullopt;
	return ProfilePhoto{(*result)[0].get<std::int64_t>("id"), position,
		isPrimary, storageKey, metadata};
}

bool ProfileRepository::updatePhoto(
	std::int64_t accountId,
	std::int64_t photoId,
	const std::string &storageKey,
	const ImageMetadata &metadata
)
{
	const auto result = _database.query(
		"UPDATE matcha_profile_photos SET storage_key = $3, "
		"content_type = $4, file_size = $5, width = $6, height = $7, "
		"file_hash = $8 WHERE id = $1 AND profile_account_id = $2 "
		"RETURNING id",
		photoId, accountId, storageKey, metadata.contentType,
		metadata.fileSize, metadata.width, metadata.height, metadata.sha256
	);
	return result && result->size() == 1;
}

bool ProfileRepository::deletePhoto(std::int64_t accountId, std::int64_t photoId)
{
	const auto result = _database.query(
		"DELETE FROM matcha_profile_photos "
		"WHERE id = $1 AND profile_account_id = $2 RETURNING id",
		photoId, accountId
	);
	return result && result->size() == 1;
}

bool ProfileRepository::setPrimaryPhoto(
	std::int64_t accountId,
	std::int64_t photoId
)
{
	const auto result = _database.query(
		"WITH target AS ("
		"SELECT id FROM matcha_profile_photos "
		"WHERE id = $2 AND profile_account_id = $1"
		"), reset AS ("
		"UPDATE matcha_profile_photos SET is_primary = false "
		"WHERE profile_account_id = $1 "
		"AND EXISTS (SELECT 1 FROM target)"
		"), selected AS ("
		"UPDATE matcha_profile_photos SET is_primary = true "
		"WHERE id IN (SELECT id FROM target) RETURNING id"
		") SELECT id FROM selected",
		accountId, photoId
	);
	return result && result->size() == 1;
}

bool ProfileRepository::hasPrimaryPhoto(std::int64_t accountId)
{
	const auto result = _database.query(
		"SELECT 1 FROM matcha_profile_photos "
		"WHERE profile_account_id = $1 AND is_primary",
		accountId
	);
	return result && result->size() == 1;
}

bool ProfileRepository::refreshCompletion(std::int64_t accountId)
{
	const auto result = _database.query(
		std::string("UPDATE matcha_profiles p SET completed_at = CASE WHEN ") +
		PROFILE_COMPLETED +
		" THEN COALESCE(p.completed_at, NOW()) ELSE NULL END, "
		"updated_at = NOW() WHERE p.account_id = $1 RETURNING p.account_id",
		accountId
	);
	return result && result->size() == 1;
}

std::unordered_set<std::string> ProfileRepository::listStorageKeys()
{
	const auto result = _database.query(
		"SELECT storage_key FROM matcha_profile_photos");
	std::unordered_set<std::string> keys;
	if (!result)
		return keys;
	for (const pg::Row row : *result)
		keys.insert(row.get<std::string>("storage_key"));
	return keys;
}
```

- **`PROFILE_COLUMNS`** : les colonnes de profil, écrites **une fois**, avec leurs
  alias. `readProfile` les lit par nom : l'ordre ne compte plus (situation 1 réglée).
- **`PROFILE_COMPLETED`** : la règle « profil complet » était écrite **trois fois**
  (`findProfile`, `listCompletedProfiles`, `refreshCompletion`). Une seule copie
  maintenant. `updateProfile` garde la sienne : elle teste les **nouvelles** valeurs
  (`$2`, `$3`…), pas les colonnes.
- **`listCompletedProfiles`** utilise le `QueryBuilder` : la règle de complétude est une
  condition sans `?` (donc sans valeur), le curseur une condition **optionnelle**.
  `PROFILE_SORTS` ne contient qu'une clé pour l'instant : la recherche avancée y
  ajoutera les siennes, et la route les acceptera depuis l'URL sans risque.
- **`listPhotos`** lit directement les bons types : `get<std::int16_t>("position")`
  (`SMALLINT`), `get<std::size_t>("file_size")`, `get<std::uint32_t>("width")`. Plus de
  `static_cast` : `fromText` refuse une valeur qui ne tient pas dans le type demandé.

**`src/http/ProfileRoutes.cpp`**, route `GET /api/profiles` : `offset` devient `after`.

```cpp
		const auto limitValue = request.url_params.get("limit");
		const auto afterValue = request.url_params.get("after");
		int limit = 24;
		std::optional<std::int64_t> after;
```

puis, à la place du bloc `if (offsetValue) { … }` :

```cpp
		if (afterValue)
		{
			std::int64_t cursor = 0;
			const auto parsed = std::from_chars(
				afterValue, afterValue + std::strlen(afterValue), cursor);
			if (parsed.ec != std::errc{} || cursor < 1)
				return auth_routes::jsonError(400, "After must be a positive account id");
			after = cursor;
		}
```

et l'appel devient `profileRepository.listCompletedProfiles(*id, limit, after)`.

### ✅ Vérifier

Rien à lancer encore : ces fichiers seront compilés et testés par l'exécutable de tests
de l'étape H.

### ❓ Point d'arrêt G

**G1.** La recherche avancée trie par popularité décroissante. Écris l'appel au
`QueryBuilder` pour la page **suivante**, sachant que la dernière ligne affichée avait
`fame = 42` et `account_id = 7`, et que l'entrée `"fame"` de la liste blanche est
`"p.fame DESC, p.account_id DESC"`.

<details><summary>Réponse</summary>

```cpp
builder.where("(p.fame, p.account_id) < (?, ?)", 42, 7)
    .orderBy("fame", sorts)
    .limit(24);
```

`<` parce que le tri est **décroissant** : la suite, ce sont les lignes « plus petites ».
La comparaison de lignes départage les `fame` égales par `account_id`. Et la clé du
curseur contient exactement les colonnes du tri, dans le même ordre.

</details>

**G2.** Dans `listMatches`, tu retires `AS other_account_id`. Ça compile. Que se
passe-t-il, et quand ?

<details><summary>Réponse</summary>

La colonne s'appelle `case`. Au premier match lu, `row.get<std::int64_t>("other_account_id")`
lève `pg::RowError` (`unknown column 'other_account_id'`) : `GET /api/matches` répond
500. Rien de silencieux : c'est un bug **visible**, que le premier test (ou le premier
clic) trouve. Avec l'ancien accès par index, la même erreur de requête aurait pu passer
inaperçue.

</details>

---

## 10. Étape H : des tests de repositories qui ne laissent aucune trace

### 🎬 La situation 1 : les tests écrivaient dans ta base de dev

L'ancien `AccountRepositoryIntegrationTests.cpp` construit un `Database`, qui lit les
variables `DB_*` : celles de **ta base de dev**, `matcha`. Chaque test crée un compte,
vérifie, puis appelle `deleteTestAccount` à la dernière ligne. Mais un `ASSERT_*` qui
échoue **arrête le test** (guide 04, Q2) : la dernière ligne n'est jamais atteinte, et
le compte reste. Au moment où j'écris ce guide, dans ta base de dev :

```sql
SELECT count(*) AS comptes_de_test FROM auth_accounts WHERE email LIKE '%@example.test';
```

```
 comptes_de_test
-----------------
              10
```

### 💡 La notion 1 : la transaction annulée

C'est la technique que le guide 05 avait gardée pour les repositories (Q8) :

```
SetUp     : BEGIN, sur la base de TEST
le test   : toutes ses requêtes passent par cette transaction
TearDown  : ROLLBACK               → la base est exactement comme avant
```

Elle marche ici, alors qu'elle ne marchait pas pour les tests de cpp-pg, parce qu'un
repository **ne valide jamais lui-même** : il reçoit un `pg::Executor &` (étape B) et
s'en sert. On lui donne la transaction du test, il écrit dedans sans le savoir, et le
`ROLLBACK` efface tout, **même si le test échoue en route** (le destructeur de la
transaction tourne quoi qu'il arrive).

Et la base de test a besoin du **schéma** de matcha : la fixture appelle `pg::migrate`
avec le dossier `backend/migrations/` (étape D) avant chaque test. Quand tout est déjà
appliqué, c'est rapide : une somme de contrôle par fichier.

### 🎬 La situation 2 : l'erreur attendue qui empoisonne le test

Le test `RejectsDuplicateEmailOrUsername` crée un compte, puis essaie de créer un
**doublon** et vérifie qu'on obtient `IdentifierAlreadyExists`. Ensuite, il essaie un
doublon de nom d'utilisateur, puis compte les lignes.

Dans une transaction, le premier doublon (`23505`) **empoisonne** la transaction (guide
05 §1.5) : toutes les requêtes suivantes sont refusées. J'ai lancé la fixture **sans**
la parade qui suit. Sortie réelle :

```
[ RUN      ] AccountRepositoryIntegrationTest.RejectsDuplicateEmailOrUsername
AccountRepositoryIntegrationTests.cpp:396: Failure
    Which is: 4-byte object <02-00 00-00>
    Which is: 4-byte object <01-00 00-00>
AccountRepositoryIntegrationTests.cpp:404: Failure
  Actual: false
current transaction is aborted, commands ignored until end of transaction block (SQLSTATE 25P02)
[  FAILED  ] AccountRepositoryIntegrationTest.RejectsDuplicateEmailOrUsername
```

Ligne 396 : le doublon de nom d'utilisateur renvoie `DatabaseError` (le `2` de l'enum)
au lieu de `IdentifierAlreadyExists` (`1`), parce que sa requête est refusée en `25P02`
avant même d'être examinée. Ligne 404 : le comptage final aussi.

En production, ce test passerait : chaque requête du repository, passée au **pool**, est
sa propre transaction. C'est la fixture qui change les règles.

### 💡 La notion 2 : `SAVEPOINT`

Un **point de sauvegarde** à l'intérieur d'une transaction :

```sql
SAVEPOINT statement;                 -- on note l'état actuel
INSERT … ;                           -- échoue (doublon)
ROLLBACK TO SAVEPOINT statement;     -- on revient à l'état noté : la transaction n'est plus empoisonnée
RELEASE SAVEPOINT statement;         -- on oublie le point (s'il a réussi, ses écritures restent)
```

`ROLLBACK TO SAVEPOINT` n'annule **que** ce qui suit le point, et rend la transaction
utilisable. On l'applique à **chaque** requête du test : un `Executor` qui entoure
chaque `execute` d'un `SAVEPOINT`. Pour le repository, chaque requête se comporte à
nouveau comme si elle était seule, comme en production. Et le `ROLLBACK` final efface
toujours tout.

> **À savoir : `now()` est figé dans une transaction.** `now()` renvoie l'heure du
> **début de la transaction**, pas l'heure actuelle (vérifié : après un `pg_sleep(1.1)`,
> `now()` n'a pas bougé ; `clock_timestamp()`, si). Dans un test enfermé dans une
> transaction, tous les `NOW()` des repositories renvoient donc la même valeur. Nos tests
> ne s'en servent pas pour mesurer une durée, mais si un jour un test « attend 60 s de
> cooldown », il faudra modifier la date en base (comme le fait déjà
> `ResendsVerificationAfterCooldownAndRotatesToken`), pas attendre.

### ⌨️ Le code

**Nouveau fichier `backend/tests/support/DatabaseTest.hpp`** : deux fixtures et
l'`Executor` à savepoints.

```cpp
#pragma once
#include <core/Logger.hpp>
#include <pg/Executor.hpp>
#include <pg/Pool.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <sstream>

// Base des tests qui parlent à la base de test (variables PG_TEST_*), avec le
// schéma de matcha appliqué par pg::migrate. Sans PG_TEST_HOST : sautés.
class DatabaseTest : public testing::Test
{
protected:
	void SetUp() override;
	virtual std::size_t poolSize() const { return 2; }

	// Détruits dans l'ordre inverse : le pool avant le logger qu'il utilise.
	std::ostringstream logs;
	core::StreamLogger logger{logs, core::LogLevel::Warning};
	std::unique_ptr<pg::ConnectionPool> pool;
};

// Chaque requête passe par un SAVEPOINT. Si elle échoue, ROLLBACK TO SAVEPOINT
// n'annule qu'elle : la transaction du test reste utilisable, comme si la
// requête avait été envoyée seule.
class SavepointExecutor : public pg::Executor
{
public:
	explicit SavepointExecutor(pg::Transaction &transaction);
	core::Result<pg::Rows> execute(std::string_view sql, const pg::Params &params = {}) override;

private:
	pg::Transaction &_transaction;
};

// Pour tester un repository : tout le test se passe dans une transaction
// annulée à la fin. Il ne laisse aucune trace, même s'il échoue en route.
class RepositoryTest : public DatabaseTest
{
protected:
	void SetUp() override;
	pg::Executor &database() { return *this->_statements; }

private:
	// Après pool : détruits avant lui. La transaction fait ROLLBACK, puis rend sa connexion.
	std::optional<pg::Transaction> _transaction;
	std::optional<SavepointExecutor> _statements;
};
```

- **`DatabaseTest`** : un pool sur la base de test, schéma appliqué. Pour les tests qui
  doivent **valider** pour de vrai (l'étape I en a besoin).
- **`RepositoryTest`** : en plus, une transaction ouverte, et `database()`, l'`Executor`
  à donner aux repositories.
- **L'ordre des membres est l'ordre inverse des destructions** (la Q7 du guide 05, sous
  une autre forme) : `_statements` meurt en premier (il référence la transaction),
  puis `_transaction` (`ROLLBACK`, et sa connexion rentre au pool), puis `pool`, puis
  `logger`. Dans l'autre sens, la transaction rendrait sa connexion à un pool déjà
  détruit.
- `poolSize()` est **virtuelle** : `MatchServiceTest` (étape I) demandera 4 connexions.
  Elle est appelée dans `SetUp`, pas dans le constructeur : dans un constructeur, un
  appel virtuel n'atteint pas encore la classe fille.

**Nouveau fichier `backend/tests/support/DatabaseTest.cpp`** :

```cpp
#include "DatabaseTest.hpp"

#include <core/ConfigReader.hpp>
#include <pg/Migrations.hpp>

void DatabaseTest::SetUp()
{
	const core::SystemEnvironment environment;
	if (!environment.get("PG_TEST_HOST"))
		GTEST_SKIP() << "PG_TEST_HOST is not set: no test database";
	core::ConfigReader config(environment);
	const pg::ConnectionSettings settings = pg::readSettings(config, "PG_TEST_");
	const auto checked = config.finish();
	ASSERT_TRUE(checked) << checked.error().message;

	pg::PoolOptions options;
	options.size = this->poolSize();
	options.connectAttempts = 1;
	auto opened = pg::ConnectionPool::open(settings, options, this->logger);
	ASSERT_TRUE(opened) << opened.error().message;
	this->pool = std::move(*opened);

	// Rapide quand tout est déjà appliqué : une somme de contrôle par fichier.
	pg::MigrationOptions migrations;
	migrations.directory = MATCHA_MIGRATIONS_DIR;
	const auto migrated = pg::migrate(*this->pool, migrations, this->logger);
	ASSERT_TRUE(migrated) << migrated.error().message;
}

SavepointExecutor::SavepointExecutor(pg::Transaction &transaction)
	: _transaction(transaction)
{
}

core::Result<pg::Rows> SavepointExecutor::execute(std::string_view sql, const pg::Params &params)
{
	if (const auto saved = this->_transaction.execute("SAVEPOINT statement"); !saved)
		return std::unexpected(saved.error());
	auto result = this->_transaction.execute(sql, params);
	if (!result)
		(void)this->_transaction.execute("ROLLBACK TO SAVEPOINT statement");
	(void)this->_transaction.execute("RELEASE SAVEPOINT statement");
	return result;
}

void RepositoryTest::SetUp()
{
	DatabaseTest::SetUp();
	if (this->IsSkipped() || this->HasFatalFailure())
		return;
	auto begun = this->pool->begin();
	ASSERT_TRUE(begun) << begun.error().message;
	this->_transaction.emplace(std::move(*begun));
	this->_statements.emplace(*this->_transaction);
}
```

- `MATCHA_MIGRATIONS_DIR` : le chemin **absolu** du dossier `migrations/`, donné par
  CMake à la compilation (plus bas). Pourquoi pas `"migrations"` comme dans `main.cpp` ?
  Un chemin relatif dépend du dossier **courant**, et `ctest` lance les tests depuis
  `build/debug/`, où il n'y a pas de dossier `migrations`.
- Dans `SavepointExecutor::execute`, le `RELEASE` est fait dans les deux cas : après un
  `ROLLBACK TO`, le point existe encore ; on le libère pour ne pas les empiler.

**`backend/tests/integration/AccountRepositoryIntegrationTests.cpp`** : le fichier
garde ses six tests ; seule la plomberie change. Six modifications :

1. Les includes du haut :

   ```cpp
   #include <gtest/gtest.h>

   #include "repositories/AccountRepository.hpp"
   #include "support/DatabaseTest.hpp"
   ```

2. Supprime la fonction `deleteTestAccount` (dans le `namespace` anonyme), et **tous**
   ses appels à la fin des tests : le `ROLLBACK` s'en charge. Juste après la `}` qui
   ferme le `namespace`, déclare la fixture :

   ```cpp
   // Chaque test tourne dans une transaction annulée à la fin (RepositoryTest) :
   // plus besoin de supprimer les comptes créés.
   class AccountRepositoryIntegrationTest : public RepositoryTest
   {
   };
   ```

3. Les six `TEST(AccountRepositoryIntegrationTest, …)` deviennent
   `TEST_F(AccountRepositoryIntegrationTest, …)` : même nom, donc les tests gardent leur
   nom dans `ctest`.

4. Dans chaque test, les deux premières lignes

   ```cpp
   Database database;
   AccountRepository repository(database);
   ```

   deviennent une seule :

   ```cpp
   AccountRepository repository(this->database());
   ```

5. Les requêtes de vérification passent par `this->database()` et lisent **par nom**.
   Il y en a cinq. Dans `CreatesAnUnverifiedAccountAndIncompleteProfile` :

   ```cpp
   	const auto query = this->database().query(
   		"SELECT a.email_verified_at, p.username, p.first_name, p.last_name, "
   		"p.sexual_orientation, p.completed_at "
   		"FROM auth_accounts a "
   		"JOIN matcha_profiles p ON p.account_id = a.id "
   		"WHERE a.id = $1",
   		result.accountId
   	);
   	ASSERT_TRUE(query) << query.error().message;
   	ASSERT_EQ(query->size(), 1u);
   	const pg::Row account = (*query)[0];
   	EXPECT_FALSE(account.get<std::optional<pg::Timestamp>>("email_verified_at"));
   	EXPECT_EQ(account.get<std::string>("username"), identity.username);
   	EXPECT_EQ(account.get<std::string>("first_name"), "Test");
   	EXPECT_EQ(account.get<std::string>("last_name"), "Account");
   	EXPECT_EQ(account.get<std::string>("sexual_orientation"), "bisexual");
   	EXPECT_FALSE(account.get<std::optional<pg::Timestamp>>("completed_at"));

   	const auto token = this->database().query(
   		"SELECT token_hash, expires_at > NOW() AS valid "
   		"FROM email_verification_tokens WHERE account_id = $1",
   		result.accountId
   	);
   	ASSERT_TRUE(token) << token.error().message;
   	ASSERT_EQ(token->size(), 1u);
   	EXPECT_EQ((*token)[0].get<std::string>("token_hash"), tokenHash);
   	EXPECT_TRUE((*token)[0].get<bool>("valid"));
   ```

   `email_verified_at` et `completed_at` sont des `TIMESTAMPTZ` qui peuvent être
   `NULL` : on les lit en `std::optional<pg::Timestamp>`, et `EXPECT_FALSE` vérifie
   qu'ils sont vides. `expires_at > NOW()` reçoit l'alias `valid`.

   Dans `VerifiesEmailTokenOnlyOnce` :

   ```cpp
   	const auto account = this->database().query(
   		"SELECT email_verified_at IS NOT NULL AS verified "
   		"FROM auth_accounts WHERE id = $1",
   		created.accountId
   	);
   	ASSERT_TRUE(account) << account.error().message;
   	ASSERT_EQ(account->size(), 1u);
   	EXPECT_TRUE((*account)[0].get<bool>("verified"));
   ```

   Dans `ResendsVerificationAfterCooldownAndRotatesToken` :

   ```cpp
   	const auto resetCooldown = this->database().query(
   		"UPDATE auth_accounts SET email_verification_last_sent_at = NULL "
   		"WHERE id = $1",
   		created.accountId
   	);
   	ASSERT_TRUE(resetCooldown) << resetCooldown.error().message;
   ```

   Dans `PasswordResetConsumesTokenAndRevokesSessions` :

   ```cpp
   	const auto password = this->database().query(
   		"SELECT password_hash FROM auth_accounts WHERE id = $1",
   		created.accountId
   	);
   	ASSERT_TRUE(password) << password.error().message;
   	ASSERT_EQ(password->size(), 1u);
   	EXPECT_EQ((*password)[0].get<std::string>("password_hash"), "new-password-hash");
   ```

   Dans `RejectsDuplicateEmailOrUsername` :

   ```cpp
   	const auto orphanCheck = this->database().query(
   		"SELECT COUNT(*) AS n FROM auth_accounts WHERE email = $1",
   		second.email
   	);
   	ASSERT_TRUE(orphanCheck) << orphanCheck.error().message;
   	EXPECT_EQ((*orphanCheck)[0].get<std::int64_t>("n"), 0);
   ```

6. Plus aucune mention de `Database` dans le fichier. Vérifie :
   `grep -n "Database\b\|executeParams\|deleteTestAccount" backend/tests/integration/AccountRepositoryIntegrationTests.cpp`
   ne doit rien afficher.

**Nouveau fichier `backend/tests/integration/ProfileRepositoryTests.cpp`** : la
situation 2 de l'étape G, transformée en test.

```cpp
#include "repositories/ProfileRepository.hpp"
#include "support/DatabaseTest.hpp"

#include <string>
#include <vector>

class ProfileRepositoryTest : public RepositoryTest
{
protected:
	// Un compte avec un profil complet (photo principale comprise).
	std::int64_t createCompletedProfile(const std::string &name)
	{
		const auto account = this->database().query(
			"INSERT INTO auth_accounts (email, password_hash) VALUES ($1, 'x') RETURNING id",
			name + "@example.test");
		EXPECT_TRUE(account) << account.error().message;
		const std::int64_t id = (*account)[0].get<std::int64_t>("id");
		EXPECT_TRUE(this->database().query(
			"INSERT INTO matcha_profiles (account_id, username, first_name, last_name, "
			"date_of_birth, gender, biography, city) "
			"VALUES ($1, $2, 'First', 'Last', '1995-01-01', 'woman', 'Hello', 'Paris')",
			id, name));
		EXPECT_TRUE(this->database().query(
			"INSERT INTO matcha_profile_photos (profile_account_id, storage_key, position, "
			"is_primary, file_size, width, height, file_hash) "
			"VALUES ($1, $2, 1, true, 10, 1, 1, repeat('a', 64))",
			id, name + ".webp"));
		return id;
	}

	std::vector<std::int64_t> page(ProfileRepository &repository, std::int64_t viewer,
		std::optional<std::int64_t> after)
	{
		std::vector<std::int64_t> ids;
		for (const ProfileData &profile : repository.listCompletedProfiles(viewer, 2, after))
			ids.push_back(profile.accountId);
		return ids;
	}
};

TEST_F(ProfileRepositoryTest, PagesFollowTheCursor)
{
	ProfileRepository repository(this->database());
	const std::int64_t viewer = this->createCompletedProfile("viewer");
	const std::int64_t a = this->createCompletedProfile("profile_a");
	const std::int64_t b = this->createCompletedProfile("profile_b");
	const std::int64_t c = this->createCompletedProfile("profile_c");

	const auto first = this->page(repository, viewer, std::nullopt);
	ASSERT_EQ(first, (std::vector<std::int64_t>{a, b}));
	EXPECT_EQ(this->page(repository, viewer, first.back()), (std::vector<std::int64_t>{c}));
}

// Avec OFFSET, un profil qui disparaît de la première page décale tout :
// la deuxième page (OFFSET 2) sauterait c. Le curseur, lui, repart de b.
TEST_F(ProfileRepositoryTest, CursorDoesNotSkipWhenAProfileDisappears)
{
	ProfileRepository repository(this->database());
	const std::int64_t viewer = this->createCompletedProfile("viewer");
	const std::int64_t a = this->createCompletedProfile("profile_a");
	const std::int64_t b = this->createCompletedProfile("profile_b");
	const std::int64_t c = this->createCompletedProfile("profile_c");
	const std::int64_t d = this->createCompletedProfile("profile_d");

	const auto first = this->page(repository, viewer, std::nullopt);
	ASSERT_EQ(first, (std::vector<std::int64_t>{a, b}));
	// a supprime sa photo : son profil n'est plus complet.
	ASSERT_TRUE(this->database().query(
		"DELETE FROM matcha_profile_photos WHERE profile_account_id = $1", a));
	EXPECT_EQ(this->page(repository, viewer, first.back()), (std::vector<std::int64_t>{c, d}));
}
```

- `createCompletedProfile` remplit tout ce qu'exige `PROFILE_COMPLETED`, photo
  principale comprise (`position` vaut 1 : le schéma impose `BETWEEN 1 AND 5`).
- `CursorDoesNotSkipWhenAProfileDisappears` rejoue exactement le schéma « A B C D » de
  l'étape G. Avec `OFFSET`, la deuxième page vaudrait `{d}` ; avec le curseur, `{c, d}`.
- `ASSERT_EQ(first, …)` et pas `EXPECT_EQ` : la ligne suivante lit `first.back()`, qui
  serait un comportement indéfini sur un vecteur vide (j'ai eu le crash en écrivant ce
  test, avec `EXPECT_EQ`).

**`backend/CMakeLists.txt`** : remplace tout le bloc
`add_executable(account_repository_integration_tests …)` (avec son
`target_include_directories` et son `target_link_libraries`) par :

```cmake
# Tests des repositories et de MatchService, sur la base postgres-test
# (variables PG_TEST_*), avec le schéma appliqué depuis migrations/.
add_executable(repository_tests
    tests/support/DatabaseTest.cpp
    tests/integration/AccountRepositoryIntegrationTests.cpp
    tests/integration/ProfileRepositoryTests.cpp
    src/repositories/AccountRepository.cpp
    src/repositories/ProfileRepository.cpp
    src/repositories/LikeRepository.cpp
    src/messaging/MessagingRepository.cpp
)

target_include_directories(repository_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_compile_definitions(repository_tests
    PRIVATE MATCHA_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
)

target_link_libraries(repository_tests
    PRIVATE
        cpp-pg::cpp-pg
        cpp-auth::cpp-auth
        GTest::gtest_main
)
```

Et en bas, `gtest_discover_tests(account_repository_integration_tests)` devient :

```cmake
# Tous ces tests partagent la base de test : un seul à la fois, même avec ctest -j.
gtest_discover_tests(repository_tests PROPERTIES RESOURCE_LOCK matcha-test-database)
```

- `tests/` dans les dossiers d'includes : pour `#include "support/DatabaseTest.hpp"`.
- `LikeRepository.cpp` et `MessagingRepository.cpp` sont déjà là : l'étape I les teste,
  et les compiler maintenant vérifie la conversion de l'étape G.
- Plus de `PostgreSQL::PostgreSQL` : les tests passent par cpp-pg, qui apporte libpq
  elle-même (en `LINK_ONLY`, la Q2 du guide 05).

### ✅ Vérifier

```sh
cmake --preset debug
cmake --build --preset debug -j"$(nproc)" --target repository_tests
ctest --preset debug -R '^(AccountRepositoryIntegrationTest|ProfileRepositoryTest)\.' -j8
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 8` (6 + 2). Puis vérifie
qu'ils n'ont rien laissé. `psql` n'est pas dans l'image du backend : depuis l'hôte,
dans `~/matcha` :

```sh
docker compose -f docker-compose.yml -f docker-compose.dev.yml exec postgres-test \
    psql -U matcha_test -d matcha_test -c "SELECT count(*) FROM auth_accounts"
```

✅ **Attendu** : `0`. Les tests ont créé des comptes, des profils, des photos, des
sessions… et le `ROLLBACK` a tout emporté.

### ❓ Point d'arrêt H

**H1.** Dans `CreatesAnUnverifiedAccountAndIncompleteProfile`, le premier
`ASSERT_EQ(result.status, …Success)` échoue. Déroule ce qui se passe jusqu'à la fin du
test, et ce qui reste dans la base de test. Compare avec l'ancien test.

<details><summary>Réponse</summary>

`ASSERT_EQ` échoue : le corps du test s'arrête là (un `return` caché). GoogleTest
détruit ensuite l'objet fixture : `_statements`, puis `_transaction`, dont le
destructeur fait `ROLLBACK`. Le compte, le profil et le jeton créés dans la transaction
disparaissent. **Rien ne reste.** L'ancien test, lui, ne serait jamais arrivé à
`deleteTestAccount` : un compte de plus dans ta base de **dev**.

</details>

**H2.** Pourquoi le `SavepointExecutor` vit-il dans `tests/support/`, et pas dans
cpp-pg à côté de `Transaction` ?

<details><summary>Réponse</summary>

Il n'est utile qu'à **cette** technique de test : simuler, à l'intérieur d'une
transaction, le comportement « une requête = une transaction » du pool. En production,
un repository reçoit soit le pool (chaque requête est indépendante), soit une vraie
transaction de service (où une erreur **doit** tout annuler : c'est voulu, étape I).
Mettre des savepoints partout coûterait trois allers-retours par requête et masquerait
justement cette règle. Ce qui ne sert qu'aux tests reste dans les tests.

</details>

---

## 11. Étape I : le like mutuel, enfin atomique

C'est la **preuve** du plan : la raison d'être de cpp-pg, c'était de pouvoir écrire
ceci correctement.

### 🎬 La situation 1 : deux likes croisés, aucun match

Alice et Bob se sont vus sur la page Découvrir. Ils cliquent sur « J'aime » à 3 ms
d'écart. Deux requêtes HTTP, deux threads de Crow, et `LikeRepository::like` est **une
seule** requête SQL (un `WITH … INSERT … INSERT`), donc une transaction chacune :

```
thread A : Alice like Bob                     thread B : Bob like Alice
─────────────────────────                     ─────────────────────────
INSERT like Alice→Bob   (pas encore validé)
                                              INSERT like Bob→Alice   (pas encore validé)
EXISTS like Bob→Alice ?
  → NON : celui de Bob n'est pas validé,
    invisible pour A (guide 05 §1.5)
                                              EXISTS like Alice→Bob ?
                                                → NON : même raison
COMMIT  (pas de match)                        COMMIT  (pas de match)
```

Les deux likes existent, **aucun match**. Alice et Bob ne pourront jamais se parler, et
rien ne le réparera (audit 4.6).

Je l'ai rejoué : le test de cette étape lance les deux likes **au même instant**
(un `std::latch` fait partir les deux threads ensemble), 20 fois de suite. Sortie
réelle **sans** le verrou qu'on va ajouter :

```
[ RUN      ] MatchServiceTest.SimultaneousLikesCreateExactlyOneMatch
MatchServiceTests.cpp:110: Failure
Expected equality of these values:
  this->matchesBetween(alice, bob)
    Which is: 0
  1
round 0

[  FAILED  ] MatchServiceTest.SimultaneousLikesCreateExactlyOneMatch
```

Dès le **premier** essai (`round 0`), trois lancements sur trois. Ce n'est pas un cas
rare : c'est le comportement normal de deux transactions simultanées.

### 🎬 La situation 2 : le match sans conversation

Aujourd'hui, la route `POST /api/profiles/<id>/like` appelle `likeRepository.like`
(validé tout de suite), **puis** `messagingRepository.ensureDirectConversation`. Si la
seconde échoue (la base redémarre entre les deux, un pic de charge…), le match existe
**sans** conversation. C'est pour ça que `main.cpp` repasse sur **tous** les matchs à
chaque démarrage pour créer les conversations manquantes (audit 4.7). Un démarrage
qui grossit avec le nombre de matchs, pour réparer un bug qu'il suffirait de ne pas
avoir.

### 💡 La notion

**1. Une transaction pour les trois écritures** (situation 2). Like, match et
conversation dans la **même** transaction : `MatchService` l'ouvre, construit
`LikeRepository` et `MessagingRepository` **sur elle** (étape B), et ne valide qu'à la
fin. Une erreur n'importe où : `return` avant le `commit()`, le destructeur fait
`ROLLBACK`, il ne reste **rien**. Plus de match sans conversation, plus besoin de
resynchronisation.

Construire un repository par transaction ne coûte rien : un repository, c'est **une
référence** (`pg::Executor &_database`), rien d'autre.

**2. Un verrou par paire de comptes** (situation 1). La transaction seule ne suffit
pas : elle rend les trois écritures atomiques, mais deux transactions simultanées ne se
voient toujours pas. Il faut que l'une **attende** l'autre. C'est le verrou consultatif
de l'étape D, avec une clé propre à la **paire** :

```sql
SELECT pg_advisory_xact_lock(hashtextextended(
    LEAST($1::BIGINT, $2::BIGINT)::TEXT || ':' || GREATEST($1::BIGINT, $2::BIGINT)::TEXT, 0))
```

- **`LEAST`/`GREATEST`** : « Alice like Bob » et « Bob like Alice » doivent prendre le
  **même** verrou. Avec Alice = 12 et Bob = 7, les deux calculent la clé du texte
  `"7:12"`. Avec `liker || ':' || liked`, l'un verrouillerait `"12:7"`, l'autre `"7:12"` :
  deux verrous différents, aucune attente, le bug est de retour.
- **Une clé par paire**, pas un verrou global : deux likes **sans rapport** (Alice→Bob et
  Chloé→David) ne s'attendent pas.
- **`hashtextextended`**, et pas `pg_advisory_xact_lock(LEAST(a,b), GREATEST(a,b))` (la
  version à deux clés que proposait le plan) : celle-là prend deux `INTEGER` (32 bits),
  et nos identifiants sont des `BIGINT`. Au-delà de 2 milliards de comptes, la
  conversion échouerait. Une collision de hachage (deux paires, même clé) est possible
  mais sans danger : deux likes sans rapport s'attendraient quelques millisecondes.
- **C'est la clé que prend déjà `ensureDirectConversation`** (relis sa requête). Dans la
  même transaction, ce n'est pas un problème : un verrou consultatif se **reprend** sans
  attendre par la transaction qui le tient déjà.

**3. Pourquoi l'attente suffit : la photo par instruction.** Rejouons avec le verrou :

```
thread A : Alice like Bob                     thread B : Bob like Alice
BEGIN                                         BEGIN
verrou "7:12"  ✔                              verrou "7:12"  … attend
INSERT like Alice→Bob
EXISTS like Bob→Alice ? → NON
COMMIT  (verrou rendu)
                                              … obtient le verrou
                                              INSERT like Bob→Alice
                                              EXISTS like Alice→Bob ? → OUI
                                              INSERT match, conversation
                                              COMMIT
```

Le point délicat est le `EXISTS` de B. Il voit le like d'Alice parce qu'en
`READ COMMITTED`, **chaque instruction** prend sa photo de la base **au moment où elle
commence** (guide 05 §1.5). L'instruction de B commence **après** le `COMMIT` de A :
elle le voit.

**Et en `REPEATABLE READ` ?** La photo est prise à la **première** instruction de la
transaction, et gardée jusqu'au bout. La première instruction de B, c'est… la prise du
verrou. Sa photo date d'**avant** le `COMMIT` de A, et B ne verra jamais le like
d'Alice, même après avoir attendu. J'ai essayé (verrou présent, transaction en
`REPEATABLE READ`). Sortie réelle :

```
[ RUN      ] MatchServiceTest.SimultaneousLikesCreateExactlyOneMatch
MatchServiceTests.cpp:110: Failure
Expected equality of these values:
  this->matchesBetween(alice, bob)
    Which is: 0
  1
round 0
```

Le niveau d'isolation « plus strict » casse la solution. Retiens-le : un niveau plus
élevé n'est pas « plus sûr » en général, il donne **d'autres** garanties.
`MatchService` garde le défaut, `READ COMMITTED`.

### ⌨️ Le code

**Nouveau fichier `backend/src/services/MatchService.hpp`** :

```cpp
#pragma once

#include "../repositories/LikeRepository.hpp"

#include <core/Result.hpp>
#include <pg/Pool.hpp>

#include <cstdint>

// Like, match et conversation dans UNE transaction : tout est enregistré, ou
// rien. Un verrou par paire de comptes fait passer l'un après l'autre deux
// likes croisés envoyés au même instant.
class MatchService
{
	public:
		explicit MatchService(pg::ConnectionPool &pool);

		core::Result<LikeResult> like(std::int64_t likerAccountId,
			std::int64_t likedAccountId);
		core::Result<void> unlike(std::int64_t likerAccountId,
			std::int64_t likedAccountId);

	private:
		pg::ConnectionPool &_pool;
};
```

**Nouveau fichier `backend/src/services/MatchService.cpp`** :

```cpp
#include "MatchService.hpp"

#include "../messaging/MessagingRepository.hpp"

namespace
{
	// Verrou consultatif propre à la paire {a, b}, quel que soit l'ordre :
	// LEAST/GREATEST donnent la même clé pour (a, b) et (b, a). Rendu tout seul
	// au COMMIT ou au ROLLBACK. C'est la clé que prend aussi
	// MessagingRepository::ensureDirectConversation : la même transaction peut
	// reprendre un verrou qu'elle tient déjà.
	core::Result<void> lockPair(pg::Transaction &tx, std::int64_t a, std::int64_t b)
	{
		const auto locked = tx.query(
			"SELECT pg_advisory_xact_lock(hashtextextended("
			"LEAST($1::BIGINT, $2::BIGINT)::TEXT || ':' || "
			"GREATEST($1::BIGINT, $2::BIGINT)::TEXT, 0))",
			a, b);
		if (!locked)
			return std::unexpected(locked.error());
		return {};
	}
}

MatchService::MatchService(pg::ConnectionPool &pool):
	_pool(pool)
{
}

core::Result<LikeResult> MatchService::like(std::int64_t likerAccountId,
	std::int64_t likedAccountId)
{
	auto tx = _pool.begin();
	if (!tx)
		return std::unexpected(tx.error());
	if (const auto locked = lockPair(*tx, likerAccountId, likedAccountId); !locked)
		return std::unexpected(locked.error());

	LikeRepository likes(*tx);
	const auto result = likes.like(likerAccountId, likedAccountId);
	if (!result)
		return std::unexpected(result.error());
	if (result->matched)
	{
		MessagingRepository messaging(*tx);
		if (!messaging.ensureDirectConversation(likerAccountId, likedAccountId))
			return core::fail(core::ErrorCode::Internal,
				"cannot create the match conversation");
	}
	if (const auto committed = tx->commit(); !committed)
		return std::unexpected(committed.error());
	return result;
}

core::Result<void> MatchService::unlike(std::int64_t likerAccountId,
	std::int64_t likedAccountId)
{
	auto tx = _pool.begin();
	if (!tx)
		return std::unexpected(tx.error());
	if (const auto locked = lockPair(*tx, likerAccountId, likedAccountId); !locked)
		return std::unexpected(locked.error());

	LikeRepository likes(*tx);
	const auto removed = likes.unlike(likerAccountId, likedAccountId);
	if (!removed)
		return std::unexpected(removed.error());
	if (*removed)
	{
		MessagingRepository messaging(*tx);
		if (!messaging.removeDirectConversation(likerAccountId, likedAccountId))
			return core::fail(core::ErrorCode::Internal,
				"cannot remove the match conversation");
	}
	return tx->commit();
}
```

Déroulons `like` comme on déroule maintenant tout code qui touche à la base :

| Ligne | Connexion | État |
|---|---|---|
| `auto tx = _pool.begin();` | une connexion empruntée, `BEGIN` | transaction ouverte |
| `lockPair(*tx, …)` | la même | verrou de la paire tenu (ou attente) |
| `LikeRepository likes(*tx);` | aucune requête | `likes` référence `*tx` |
| `likes.like(…)` | la même | like (et peut-être match) écrits, invisibles des autres |
| `messaging.ensureDirectConversation(…)` | la même | conversation écrite |
| `tx->commit()` | la même, `COMMIT` | tout devient visible d'un coup, verrou rendu |
| `}` de la fonction | `tx` meurt, sa connexion rentre au pool | |

Et chaque `return` d'erreur entre les deux : `tx` meurt **sans** `commit()`, donc
`ROLLBACK`, verrou rendu, connexion rendue. On n'écrit **aucun** `rollback()` à la main.

`unlike` suit le même modèle, et ne supprime la conversation que si un like a vraiment
été supprimé.

**`backend/src/http/LikeRoutes.hpp`** : la route reçoit le service au lieu du
repository de messagerie. `#include "../services/MatchService.hpp"` remplace
`#include "../messaging/MessagingRepository.hpp"`, et le dernier paramètre devient
`MatchService &matchService`.

**`backend/src/http/LikeRoutes.cpp`** : même changement dans la signature de
`registerLikeRoutes`, puis les deux routes. Le corps de `POST …/like`, à partir du
`try` (compris) :

```cpp
			const auto result = matchService.like(*current, targetId);
			if (!result)
			{
				std::cerr << "[LIKE ERROR] " << result.error().message << std::endl;
				return auth_routes::jsonError(500, "Unable to process like");
			}
			crow::json::wvalue body;
			body["liked"] = result->liked;
			body["matched"] = result->matched;
			if (result->matchId)
				body["match_id"] = *result->matchId;
			return crow::response(200, body);
```

Et celui de `DELETE …/like`, à partir du `try` (compris) :

```cpp
			const auto removed = matchService.unlike(*current, targetId);
			if (!removed)
			{
				std::cerr << "[LIKE ERROR] " << removed.error().message << std::endl;
				return auth_routes::jsonError(500, "Unable to remove like");
			}
			return crow::response(204);
```

Plus de `try`/`catch` : les erreurs arrivent dans le `Result`. (Le `std::cerr` restera
jusqu'à ce que les routes reçoivent un logger, avec cpp-auth-crow au guide 10.)

**Nouveau fichier `backend/tests/integration/MatchServiceTests.cpp`** :

```cpp
#include "services/MatchService.hpp"
#include "support/DatabaseTest.hpp"

#include <latch>
#include <string>
#include <thread>

// Ici, pas de transaction annulée : MatchService ouvre et valide ses propres
// transactions, sur deux threads à la fois. Les comptes créés sont supprimés
// à la fin de chaque test (ON DELETE CASCADE emporte likes, matchs et conversations).
class MatchServiceTest : public DatabaseTest
{
protected:
	std::size_t poolSize() const override { return 4; }

	void TearDown() override
	{
		if (this->pool)
			(void)this->pool->query("DELETE FROM auth_accounts WHERE email LIKE 'match-test-%'");
	}

	std::int64_t createAccount()
	{
		const auto rows = this->pool->query(
			"INSERT INTO auth_accounts (email, password_hash) VALUES ($1, 'x') RETURNING id",
			"match-test-" + std::to_string(++this->_created) + "@example.test");
		EXPECT_TRUE(rows) << rows.error().message;
		return rows ? (*rows)[0].get<std::int64_t>("id") : 0;
	}

	std::int64_t count(const char *sql, std::int64_t a, std::int64_t b)
	{
		const auto rows = this->pool->query(sql, a, b);
		EXPECT_TRUE(rows) << rows.error().message;
		return rows ? (*rows)[0].get<std::int64_t>("n") : -1;
	}

	std::int64_t matchesBetween(std::int64_t a, std::int64_t b)
	{
		return this->count("SELECT count(*) AS n FROM profile_matches "
						   "WHERE first_account_id = LEAST($1::BIGINT, $2::BIGINT) "
						   "AND second_account_id = GREATEST($1::BIGINT, $2::BIGINT)",
						   a, b);
	}

	std::int64_t conversationsBetween(std::int64_t a, std::int64_t b)
	{
		return this->count("SELECT count(*) AS n FROM messaging_conversation_participants p "
						   "JOIN messaging_conversation_participants q "
						   "ON q.conversation_id = p.conversation_id "
						   "WHERE p.account_id = $1 AND q.account_id = $2",
						   a, b);
	}

private:
	int _created = 0;
};

TEST_F(MatchServiceTest, MutualLikeCreatesMatchAndConversation)
{
	MatchService service(*this->pool);
	const std::int64_t alice = this->createAccount();
	const std::int64_t bob = this->createAccount();

	const auto first = service.like(alice, bob);
	ASSERT_TRUE(first) << first.error().message;
	EXPECT_FALSE(first->matched);
	const auto second = service.like(bob, alice);
	ASSERT_TRUE(second) << second.error().message;
	EXPECT_TRUE(second->matched);
	EXPECT_EQ(this->matchesBetween(alice, bob), 1);
	EXPECT_EQ(this->conversationsBetween(alice, bob), 1);
}

TEST_F(MatchServiceTest, UnlikeRemovesMatchAndConversation)
{
	MatchService service(*this->pool);
	const std::int64_t alice = this->createAccount();
	const std::int64_t bob = this->createAccount();
	ASSERT_TRUE(service.like(alice, bob));
	ASSERT_TRUE(service.like(bob, alice));

	const auto removed = service.unlike(alice, bob);
	ASSERT_TRUE(removed) << removed.error().message;
	EXPECT_EQ(this->matchesBetween(alice, bob), 0);
	EXPECT_EQ(this->conversationsBetween(alice, bob), 0);
}

// Le scénario de l'audit (4.6) : A like B et B like A au même instant. Sans
// le verrou de MatchService, chaque transaction ne voit pas le like (pas
// encore validé) de l'autre, et aucun match n'est créé.
TEST_F(MatchServiceTest, SimultaneousLikesCreateExactlyOneMatch)
{
	MatchService service(*this->pool);
	for (int round = 0; round < 20; ++round)
	{
		const std::int64_t alice = this->createAccount();
		const std::int64_t bob = this->createAccount();
		std::latch start(2);
		auto likeAtOnce = [&](std::int64_t liker, std::int64_t liked)
		{
			start.arrive_and_wait(); // les deux threads partent ensemble
			const auto result = service.like(liker, liked);
			EXPECT_TRUE(result) << result.error().message;
		};
		std::thread first(likeAtOnce, alice, bob);
		std::thread second(likeAtOnce, bob, alice);
		first.join();
		second.join();
		ASSERT_EQ(this->matchesBetween(alice, bob), 1) << "round " << round;
		ASSERT_EQ(this->conversationsBetween(alice, bob), 1) << "round " << round;
	}
}
```

- **`DatabaseTest`, pas `RepositoryTest`** : `MatchService` ouvre et **valide** ses
  propres transactions, sur deux connexions à la fois. Une transaction enveloppe ne
  pourrait ni contenir les siennes (pas de transactions imbriquées), ni être partagée
  entre deux threads. C'est exactement la Q8 du guide 05 : on fait le ménage à la main,
  dans `TearDown`, en supprimant les comptes `match-test-%` (le `ON DELETE CASCADE` du
  schéma emporte likes, matchs et conversations).
- **`poolSize()` vaut 4** : deux transactions en même temps, plus les requêtes de
  vérification.
- **`std::latch start(2)`** : chaque thread appelle `arrive_and_wait()` et bloque
  jusqu'à ce que **les deux** soient arrivés. Ils partent alors ensemble : c'est ce qui
  rend la collision quasi certaine (sans le verrou, elle arrive au premier tour).
- **20 tours** : avec le verrou, le test doit passer **à chaque tour** ; un seul tour
  raté suffit à prouver le bug.

**`backend/CMakeLists.txt`** : ajoute `tests/integration/MatchServiceTests.cpp` et
`src/services/MatchService.cpp` à la liste de `add_executable(repository_tests …)`.

### ✅ Vérifier

```sh
cmake --build --preset debug -j"$(nproc)" --target repository_tests
ctest --preset debug -R '^MatchServiceTest\.'
```

✅ **Attendu** : `100% tests passed, 0 tests failed out of 3`.

Puis **casse-le toi-même** : dans `MatchService::like`, remplace la ligne
`if (const auto locked = lockPair(*tx, likerAccountId, likedAccountId); !locked)` par
`if (const core::Result<void> locked{}; !locked)` (un résultat toujours réussi : plus de
verrou), recompile, relance. Tu dois obtenir la sortie de la situation 1. **Remets la
ligne**, et vérifie que c'est de nouveau vert.

### ❓ Point d'arrêt I

**I1.** Dans `MatchService::like`, `ensureDirectConversation` échoue. Déroule la suite
jusqu'à la réponse HTTP. Qu'y a-t-il en base pour Alice et Bob ? Et avec l'ancienne
route ?

<details><summary>Réponse</summary>

`like` renvoie `core::fail(Internal, "cannot create the match conversation")` **sans**
avoir appelé `commit()`. À la `}`, `tx` est détruite : `ROLLBACK`. Le like de Bob et
le match, écrits plus haut dans la même transaction, sont annulés ; le verrou est
rendu ; la connexion rentre au pool. La route répond 500. En base : seulement le like
d'Alice (validé lors de **son** clic). Bob pourra recliquer, et tout sera créé
ensemble. Avec l'ancienne route : like et match validés, pas de conversation, jusqu'au
prochain redémarrage du serveur.

</details>

**I2.** Un collègue « simplifie » la clé du verrou en `$1::TEXT || ':' || $2::TEXT`
(le liker, puis le liké), sans `LEAST`/`GREATEST`. Alice a l'id 12, Bob l'id 7.
Rejoue la situation 1 avec cette version. Et pendant ce temps, Alice like aussi Chloé
(id 20) : ce like-là attend-il celui pour Bob ?

<details><summary>Réponse</summary>

Alice→Bob verrouille `"12:7"`, Bob→Alice verrouille `"7:12"` : **deux verrous
différents**. Personne n'attend personne, les deux `EXISTS` se font avant les deux
`COMMIT` : aucun match, le bug est de retour (et le test le verrait au premier tour).
Avec `LEAST`/`GREATEST`, les deux calculent `"7:12"`.

Alice→Chloé verrouille `"12:20"` (avec ou sans la « simplification ») : une autre clé
que `"7:12"`, donc **pas d'attente**. Le verrou ne sérialise que les likes d'une même
paire.

</details>

---

## 12. Étape J : `main.cpp`, CMake et le front

### 🎬 La situation : l'ordre de destruction

À la fin de `run`, toutes les variables locales sont détruites, **dans l'ordre inverse
de leur déclaration**. Le serveur Crow (`app`) est déclaré **après** les repositories,
qui sont déclarés **après** le pool. Donc : `app` s'arrête d'abord (ses threads ne
traitent plus de requêtes), puis les repositories disparaissent, puis le pool, quand
plus personne ne peut lui emprunter de connexion.

Déclare le pool **après** un repository, et tu obtiens la Q7 du guide 05 : le pool
détruit d'abord, le repository qui garde une référence vers lui, et un thread de Crow
encore en vie qui l'utilise. La règle : **ce qui est utilisé est déclaré avant ce qui
l'utilise**.

### 💡 La notion : la configuration avant tout

Rien de neuf, la règle du guide 04 (Q5) : **toutes** les lectures de config avant
`config.finish()`. `pg::readSettings(config, "DB_")` lit `DB_HOST`, `DB_PORT`,
`DB_NAME`, `DB_USER` et `DB_PASSWORD_FILE` : les mêmes variables que l'ancien
`Database`, qui les lisait lui-même avec `std::getenv`, **après** la validation de la
config, et lançait une exception s'il en manquait une.

### ⌨️ Le code

**`backend/src/main.cpp`** :

1. **Includes** : supprime `#include <libpq-fe.h>`, `"database/Database.hpp"` et
   `"database/DatabaseInitializer.hpp"`. Ajoute `<memory>` et `<string_view>` aux
   includes standard, `"services/MatchService.hpp"` avant
   `"services/ProfilePhotoService.hpp"`, et sous `<core/Logger.hpp>` :

   ```cpp
   #include <pg/Migrations.hpp>
   #include <pg/Pool.hpp>
   ```

2. **`run`** reçoit un second paramètre, et lit la config de la base en premier :

   ```cpp
   int run(core::ILogger &logger, bool migrationsDryRun)
   {
   	const core::SystemEnvironment environment;
   	core::ConfigReader config(environment);

   	pg::ConnectionSettings databaseSettings = pg::readSettings(config, "DB_");
   	databaseSettings.applicationName = "matcha_backend";
   	pg::PoolOptions poolOptions;
   	poolOptions.size = 8; // au-delà, une requête attend (2 s au plus) qu'une connexion se libère
   ```

   `applicationName` apparaît dans `pg_stat_activity` côté PostgreSQL : pratique pour
   savoir quelles connexions sont celles du serveur.

3. **Le bloc `Database` … resynchronisation** (de `Database database;` jusqu'à la `}`
   de la boucle `for` sur `listAllMatches()`) est remplacé par :

   ```cpp
   	auto openedPool = pg::ConnectionPool::open(databaseSettings, poolOptions, logger);
   	if (!openedPool)
   	{
   		logger.error("Database unavailable", {{"reason", openedPool.error().message}});
   		return 1;
   	}
   	// Déclaré avant tout ce qui l'utilise : détruit après eux (guide 05, Q7).
   	std::unique_ptr<pg::ConnectionPool> pool = std::move(*openedPool);

   	pg::MigrationOptions migrationOptions;
   	migrationOptions.dryRun = migrationsDryRun;
   	const auto migrated = pg::migrate(*pool, migrationOptions, logger);
   	if (!migrated)
   	{
   		logger.error("Database migration failed", {{"reason", migrated.error().message}});
   		return 1;
   	}
   	if (migrationsDryRun)
   	{
   		logger.info("Migrations dry run", {{"pending", std::to_string(migrated->applied.size())}});
   		return 0;
   	}

   	AccountRepository accountRepository(*pool);
   	ProfileRepository profileRepository(*pool);
   	LikeRepository likeRepository(*pool);
   	MessagingRepository messagingRepository(*pool);
   	MatchService matchService(*pool);
   ```

   - Base injoignable au démarrage : un message clair et `return 1`, au lieu de
     l'ancien serveur qui démarrait avec `_connection == nullptr` (audit 4.5).
     `ConnectionPool::open` réessaie 12 fois, 5 s d'intervalle (le temps que PostgreSQL
     démarre dans Docker), et l'annonce dans les logs.
   - `pool` est un `std::unique_ptr` : son adresse ne bouge pas (guide 05 §1.4).
   - **La boucle de resynchronisation a disparu**, avec `listAllMatches` : un match ne
     peut plus exister sans sa conversation (étape I).

4. **`registerLikeRoutes`** reçoit `matchService` au lieu de `messagingRepository`
   (dernier argument).

5. **`main`** lit l'option de la ligne de commande :

   ```cpp
   int main(int argc, char **argv)
   {
   	core::StreamLogger logger(std::cerr, core::LogLevel::Info);
   	// matcha_backend --migrations-dry-run : affiche les migrations en attente,
   	// les essaie, annule tout, et s'arrête.
   	const bool migrationsDryRun =
   		argc == 2 && std::string_view(argv[1]) == "--migrations-dry-run";
   	try
   	{
   		return run(logger, migrationsDryRun);
   	}
   ```

   `std::string_view(argv[1]) == "…"` compare le **contenu**. Écrire
   `argv[1] == "--migrations-dry-run"` compilerait, mais comparerait deux **adresses**
   (deux `const char *`), toujours différentes.

**Supprime `src/database/`** :

```sh
git rm -r backend/src/database
```

**`backend/CMakeLists.txt`**, pour l'exécutable du serveur :

1. Supprime `find_package(PostgreSQL REQUIRED)` en haut : plus personne dans le backend
   n'inclut `libpq-fe.h`. libpq arrive par cpp-pg, en `LINK_ONLY` (Q2 du guide 05, en
   vrai).
2. Dans `add_executable(matcha_backend …)`, supprime les trois lignes
   `src/database/…` et ajoute `src/services/MatchService.cpp` après
   `src/services/ProfilePhotoService.cpp`.
3. Dans son `target_link_libraries`, remplace `PRIVATE PostgreSQL::PostgreSQL` par
   `PRIVATE cpp-pg::cpp-pg` (place-la juste après `cpp-core::cpp-core`).

Vérifie qu'il ne reste rien de l'ancien monde :

```sh
grep -rn "database/\|PostgreSQL::\|QueryResult\|executeParams" backend/CMakeLists.txt backend/src backend/tests
```

✅ **Attendu** : aucune ligne.

**Le front** : la page Découvrir passe au curseur. C'est le seul changement côté front,
donne-le tel quel.

`frontend/src/api/auth.ts`, la fonction `getProfiles` :

```ts
// after : le dernier account_id de la page précédente (pagination par curseur)
export async function getProfiles(
  accessToken: string,
  { limit = 24, after }: { limit?: number; after?: number } = {},
): Promise<Profile[]> {
  const cursor = after === undefined ? '' : `&after=${after}`
  const { body } = await request<{ profiles?: Omit<Profile, 'photos'>[] }>(
    `/api/profiles?limit=${limit}${cursor}`,
```

(la suite de la fonction ne change pas.)

`frontend/src/features/discover/DiscoverPage.tsx` : `fetchPage` reçoit le curseur au
lieu de l'offset,

```tsx
  const fetchPage = useCallback(async (after?: number) => {
    const token = tokenRef.current
    if (!token) return
    try {
      const page = await getProfiles(token, { limit: PAGE_SIZE, after })
      setProfiles((current) => after === undefined ? page : [...current, ...page])
```

et, plus bas :

```tsx
    void fetchPage()
  }, [fetchPage])

  function loadMore() {
    setIsLoading(true)
    void fetchPage(profiles.at(-1)?.account_id)
```

`profiles.at(-1)` est le dernier profil chargé ; `?.account_id` donne `undefined` si la
liste est vide (alors `fetchPage` recharge la première page).

### ✅ Vérifier

```sh
cmake --preset debug
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug -N | tail -1
```

✅ **Attendu** : compilation sans warning, puis `Total Tests: 243` : les 238 de
l'étape F (les 6 tests d'`AccountRepository` y sont toujours, déplacés dans
`repository_tests` sous le même nom), plus 2 de `ProfileRepositoryTest` et 3 de
`MatchServiceTest`.

### ❓ Point d'arrêt J

**J1.** Tu lances `matcha_backend --migrations-dry-run` sur ta base de dev, **avant**
d'avoir jamais démarré le nouveau serveur. Quelles lignes de log t'attends-tu à voir
(sans regarder l'étape K) ? La base est-elle modifiée ?

<details><summary>Réponse</summary>

Le pool prêt, puis `Migration checksum recorded` pour chacune des 7 migrations déjà
appliquées (la colonne `checksum` est vide : adoption), puis
`Migration would be applied` pour `009_drop_legacy_users.sql`, puis
`Migrations dry run pending=1`. Et la base n'est **pas** modifiée : la colonne
`checksum`, les sommes adoptées, le `DROP TABLE`, tout était dans la transaction, qui
est annulée. Un second dry-run affiche exactement la même chose (c'est vérifié à
l'étape K).

</details>

---

## 13. Étape K : vérifier tout le parcours

### Les tests

Sur l'hôte, depuis `~/matcha` :

```sh
make test
make sanitize
```

✅ **Attendu**, pour chacun : `100% tests passed, 0 tests failed out of 243`.

Puis en parallèle, trois fois, dans le conteneur (`make shell`, depuis `/app`) :

```sh
for i in 1 2 3; do ctest --preset debug -j8 | tail -3 | head -1; done
```

✅ **Attendu** : trois fois `100% tests passed, 0 tests failed out of 243`. Les deux
`RESOURCE_LOCK` (celui de cpp-pg, celui de `repository_tests`) font passer les tests à
base un par un ; tout le reste tourne en parallèle.

> **Si `LocalImageStorageTest.RemovesOnlyUnreferencedFiles` échoue en `-j`** (je l'ai vu
> une fois, en `sanitize -j8`) : c'est un défaut **antérieur**, sans rapport avec ce
> guide. Ses deux tests utilisent le **même** dossier temporaire
> (`matcha-local-storage-tests`), et l'un le vide pendant que l'autre s'en sert. Le
> correctif tient en une ligne, dans le `SetUp` de `tests/images/LocalImageStorageTests.cpp` :
> ajouter le nom du test au dossier,
> `"matcha-local-storage-tests-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name())`.
> C'est le même principe que `MigrationDirectory` (étape D).

### Le dry-run sur une copie de ta base de dev

On ne touche pas à ta base de dev : on la **copie** dans `postgres-test`. Depuis
l'hôte, dans `~/matcha` :

```sh
DC="docker compose -f docker-compose.yml -f docker-compose.dev.yml"
$DC exec -T postgres-test psql -U matcha_test -d matcha_test \
    -c "DROP DATABASE IF EXISTS matcha_copy" -c "CREATE DATABASE matcha_copy"
$DC exec -T database pg_dump -U matcha --no-owner --no-privileges matcha \
    | $DC exec -T postgres-test psql -q -U matcha_test -d matcha_copy > /dev/null
```

- `pg_dump` écrit toute la base (schéma et données) en SQL ; `psql` le rejoue dans la
  copie. `--no-owner --no-privileges` : les rôles de `matcha` n'existent pas dans
  `postgres-test`.
- `-T` : pas de terminal interactif. Sans lui, `psql` ouvre un *pager* et attend que tu
  appuies sur `q` (je me suis fait avoir).

Puis dans le conteneur (`make shell`, depuis `/app`), le serveur, pointé sur la copie :

```sh
printf %s "$PG_TEST_PASSWORD" > /tmp/pgtestpw
export DB_HOST=postgres-test DB_NAME=matcha_copy DB_USER=matcha_test DB_PASSWORD_FILE=/tmp/pgtestpw
./build/debug/matcha_backend --migrations-dry-run
```

Sortie réelle (sur une copie de ta base faite pendant l'écriture du guide) :

```
level=INFO msg="PostgreSQL pool ready" host=postgres-test database=matcha_adopt size=8
level=INFO msg="Migration checksum recorded" migration=001_create_users.sql
level=INFO msg="Migration checksum recorded" migration=002_create_account_and_profile_schema.sql
level=INFO msg="Migration checksum recorded" migration=003_add_email_verification_resend_cooldown.sql
level=INFO msg="Migration checksum recorded" migration=004_add_password_reset_cooldown.sql
level=INFO msg="Migration checksum recorded" migration=005_add_profile_data_and_photo_metadata.sql
level=INFO msg="Migration checksum recorded" migration=007_create_likes_and_matches.sql
level=INFO msg="Migration checksum recorded" migration=008_create_messaging.sql
level=INFO msg="Migration would be applied" migration=009_drop_legacy_users.sql
level=INFO msg="Migrations dry run" pending=1
```

(Chez toi, `database=matcha_copy`.) **Relance la même commande** : exactement la même
sortie. Le premier dry-run n'a rien gardé, pas même la colonne `checksum` : tout était
dans la transaction annulée. « Checksum recorded » veut dire ici « aurait été
enregistrée ».

**Le fichier modifié.** Pour qu'un fichier puisse être « modifié après application »,
il faut d'abord que les sommes soient **enregistrées**, donc un vrai démarrage sur la
copie. Toujours dans le conteneur :

```sh
timeout 4 ./build/debug/matcha_backend > /tmp/run.log 2>&1; head -10 /tmp/run.log
```

Sans `--migrations-dry-run`, cette fois : les sommes sont enregistrées et `009` est
appliquée **sur la copie**, puis le serveur essaie d'écouter sur le port 18080, déjà
pris par ton serveur de dev, et `timeout` l'arrête au bout de 4 s. Dans le log : les 7
`Migration checksum recorded`, `Applying migration migration=009_drop_legacy_users.sql`,
`Server starting`. Maintenant, modifie une migration déjà appliquée et relance :

```sh
echo "-- petite retouche" >> migrations/002_create_account_and_profile_schema.sql
./build/debug/matcha_backend --migrations-dry-run; echo "[exit $?]"
```

Sortie réelle :

```
level=INFO msg="PostgreSQL pool ready" host=postgres-test database=matcha_adopt size=8
level=ERROR msg="Database migration failed" reason="migration 002_create_account_and_profile_schema.sql was modified after being applied"
[exit 1]
```

Le serveur **refuse de démarrer**, et dit pourquoi. Remets le fichier (sur l'hôte) et
range :

```sh
git checkout -- backend/migrations/002_create_account_and_profile_schema.sql
```

```sh
# dans le conteneur
rm /tmp/pgtestpw /tmp/run.log
unset DB_HOST DB_NAME DB_USER DB_PASSWORD_FILE
```

```sh
# sur l'hôte
$DC exec -T postgres-test psql -U matcha_test -d matcha_test -c "DROP DATABASE matcha_copy"
```

### Le vrai serveur

```sh
make rebuild
make logs
```

✅ **Attendu** dans les logs : les 7 `Migration checksum recorded`, puis
`Applying migration migration=009_drop_legacy_users.sql`, puis `Server starting`. Cette
fois c'est ta base de dev : la table `users` est supprimée, les sommes de contrôle
enregistrées. Au redémarrage suivant, plus aucune ligne de migration.

Puis dans le navigateur :

- **Découvrir** : « Voir plus » charge la suite (requête `/api/profiles?limit=24&after=…`
  dans l'onglet Réseau des outils de développement).
- **Like mutuel** : avec deux comptes (deux navigateurs, ou une fenêtre privée), A like
  B, puis B like A. B voit le match, et la conversation apparaît dans la messagerie des
  deux côtés.

---

## 14. Étape L : commiter

Le commit 1 (la lib) est fait depuis l'étape E. Le second, le backend et le front.
Depuis `~/matcha` :

```sh
git add backend frontend/src/api/auth.ts frontend/src/features/discover/DiscoverPage.tsx
git status
```

✅ **Attendu** : sous `Changes to be committed` :

- **supprimés** : les 6 fichiers de `backend/src/database/`, `UserRepository.hpp/.cpp`,
  et les 3 fichiers de tests de l'étape F ;
- **nouveaux** : `migrations/009_drop_legacy_users.sql`, `migrations/README.md`,
  `MatchService.hpp/.cpp`, `tests/support/DatabaseTest.hpp/.cpp`,
  `ProfileRepositoryTests.cpp`, `MatchServiceTests.cpp` ;
- **modifiés** : `CMakeLists.txt`, `main.cpp`, les 4 repositories (`.hpp` et `.cpp`),
  `LikeRoutes.hpp/.cpp`, `ProfileRoutes.cpp`, `AccountRepositoryIntegrationTests.cpp`,
  et les 2 fichiers du front.

Rien dans `backend/libs/` (commité à l'étape E), et pas de `migrations/002…` (remis à
l'étape K).

```sh
git commit -m "backend : cpp-pg partout, src/database supprimé, like mutuel atomique (audit 4.4 à 4.7, 4.15 à 4.17)"
git log --oneline -3
```

Mets à jour le plan (`docs/guides/00-plan.md`) : guide 06 ✅. Le mercredi est fini, et
avec lui la priorité 1 de l'audit. 🎉

---

## 15. Questions de synthèse

Réponds sous chaque **Ta réponse**, à la place de la ligne `…`, puis préviens-moi. Elles
reprennent toutes une situation du guide : relis l'étape indiquée si tu bloques, mais
**écris avec tes mots**, même si c'est faux ou incomplet. Une réponse recopiée ne me
permet pas de voir ce qui coince.

### S1. Le niveau d'isolation « plus sûr » (étape I)

Un collègue lit que `SERIALIZABLE` est « le niveau le plus sûr » et change
`_pool.begin()` en `_pool.begin(pg::Isolation::Serializable)` dans
`MatchService::like`. Le verrou par paire reste. Déroule la situation 1 (Alice et Bob
cliquent ensemble) avec cette version : que voit le `EXISTS` de la seconde
transaction, et pourquoi ? (Indice : en `SERIALIZABLE`, la photo est prise comme en
`REPEATABLE READ`. Tu n'as pas besoin de connaître le reste de ses règles.)

**Ta réponse :**

…

### S2. Le repository dans la transaction des autres (étapes B, H, I)

`ProfileRepository` est construit **une fois** dans `main.cpp` avec `*pool`, et
**aussi**, dans les tests, avec `this->database()`. Explique, pour chacun des deux cas,
sur quelle(s) connexion(s) partent les requêtes de `findProfile` puis
`refreshCompletion` appelées l'une après l'autre, et quand leurs écritures deviennent
visibles pour les autres. Pourquoi le repository n'a-t-il pas besoin de savoir dans
quel cas il est ?

**Ta réponse :**

…

### S3. Deux façons d'écrire un filtre (étape C)

Deux collègues ajoutent un filtre par ville à `listCompletedProfiles` :

```cpp
// A
builder.where("p.city = '" + *city + "'");
// B
builder.whereIf(city, "p.city = ?");
```

Avec `city = "Paris' OR '1'='1"`, écris le SQL et les paramètres que produit chacune.
Laquelle est une injection, et qu'est-ce qui, dans `QueryBuilder`, ne peut **pas**
empêcher la version A ?

**Ta réponse :**

…

### S4. Renommer une migration (étapes D et F)

Sur une base où 001 à 009 sont appliquées :

1. quelqu'un renomme `003_add_email_verification_resend_cooldown.sql` en
   `003_add_resend_cooldown.sql`, **sans** toucher au contenu ;
2. une autre fois, quelqu'un le renumérote en `010_add_email_verification_resend_cooldown.sql`.

Pour chaque cas, déroule ce que fait `migrate` au démarrage suivant, fichier par
fichier (que vérifie-t-il, avec quoi compare-t-il ?), et dis si le serveur démarre.

**Ta réponse :**

…

---

## 16. Correction

> Remplie après tes réponses.
>
> **Légende** : 🟢 juste et complet · 🟡 juste mais incomplet ou imprécis ·
> 🔴 faux ou à côté · ⏳ pas encore corrigé

| Question | Évaluation |
|---|---|
| S1. Le niveau d'isolation « plus sûr » | ⏳ |
| S2. Le repository dans la transaction des autres | ⏳ |
| S3. Deux façons d'écrire un filtre | ⏳ |
| S4. Renommer une migration | ⏳ |
