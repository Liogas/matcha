# Un QueryBuilder sûr
> Une petite classe qui assemble une requête aux filtres optionnels, sans jamais écrire une valeur dans le texte SQL.

## Mise en situation
La recherche de matcha accepte quatre filtres optionnels : âge, popularité, ville et tags. Chacun peut être présent ou absent, ce qui fait 2 × 2 × 2 × 2 = 16 requêtes possibles. Impossible de les écrire à la main.

Le premier réflexe ressemble à ceci :

```cpp
std::string sql = "SELECT account_id FROM matcha_profiles WHERE 1=1";
int n = 1;
if (minAge) sql += " AND age >= $" + std::to_string(n++);
if (city)   sql += " AND city = $" + std::to_string(n++);
```

Ça fonctionne, mais tout est fragile. Il faut compter les `$n`, garder les valeurs dans le même ordre que le texte, et ne jamais glisser une valeur dans le texte. Une erreur de décalage donne une erreur SQL ou, pire, compare le mauvais champ sans rien dire.

## La théorie
### Le rôle exact du builder
Un builder ne remplace pas ton SQL : il assemble des morceaux que tu écris. Il fait trois choses :

- il transforme chaque `?` en numéro suivant : `$1`, `$2`... ;
- il range les valeurs dans le même ordre, dans `Params` ;
- il impose une [liste blanche](../web/validation-entrees.md) pour le tri.

Ce n'est pas un ORM. Un ORM décrit les tables par des classes et écrit le SQL à ta place, ce que le sujet interdit. Ici, chaque condition reste écrite par toi. Le sujet autorise d'ailleurs explicitement une bibliothèque pour simplifier la gestion des requêtes.

### Le modèle mental
Le builder accumule, puis produit un résultat immuable :

```text
QueryBuilder  ->  _conditions + _params + _orderBy + _limitParam
build()       ->  Query { sql, params }  ou  Error Invalid
```

### Chaque méthode renvoie *this
`where` renvoie une référence sur l'objet lui-même, ce qui permet d'enchaîner les appels :

```cpp
query.where("p.age >= ?", 18).whereIf(city, "p.city = ?").limit(20);
```

### Trois catégories d'erreurs
- **Mauvais nombre de `?`** : c'est un bug de ton code. `addCondition` lève `std::logic_error`, parce qu'aucun utilisateur ne peut le provoquer.
- **Clé de tri inconnue** : elle vient d'une URL. Ce n'est pas un bug, donc `build()` renvoie une erreur `Invalid`, voir Result.
- **Limite** : elle passe en paramètre, comme toute valeur.

Cette distinction revient partout : une exception pour ce qui ne doit jamais arriver, un `Result` pour ce qui peut arriver.

## Exemple complet
```cpp
const pg::SortWhitelist kSorts = {
    {"fame", "p.fame_rating DESC, p.account_id DESC"},
    {"age", "p.date_of_birth DESC, p.account_id DESC"},
};

core::Result<pg::Rows> rechercher(pg::Executor &db, const Filtres &f) {
    pg::QueryBuilder query("SELECT p.account_id FROM matcha_profiles p");
    query.where("p.completed_at IS NOT NULL")
         .whereIf(f.minFame, "p.fame_rating >= ?")
         .whereIf(f.city, "p.city = ?")
         .orderBy(f.sort, kSorts)
         .limit(20);
    auto built = query.build();
    if (!built)
        return std::unexpected(built.error());
    return db.execute(built->sql, built->params);
}
```

Avec `minFame = 50`, `city` vide et `sort = "fame"`, le déroulé est le suivant :

1. `where` sans argument ajoute la condition telle quelle.
2. `whereIf(minFame, ...)` voit une valeur : `first` vaut 1, la condition devient `p.fame_rating >= $1`, et 50 entre dans `_params`.
3. `whereIf(city, ...)` ne fait rien.
4. `orderBy` trouve `fame` dans la [liste blanche](../web/validation-entrees.md).
5. `limit(20)` ajoute 20 comme paramètre `$2`.

Résultat :

```sql
SELECT p.account_id FROM matcha_profiles p
WHERE p.completed_at IS NOT NULL AND p.fame_rating >= $1
ORDER BY p.fame_rating DESC, p.account_id DESC LIMIT $2
```

### Pourquoi whereIf prend l'optional lui-même
Une version naïve serait `whereIf(city.has_value(), "...", *city)`. Le problème est que les arguments sont évalués avant l'appel : `*city` serait lu même si l'`optional` est vide, ce qui est un comportement indéfini. En prenant `const std::optional<T> &`, la fonction décide elle-même et ne déréférence que si la valeur existe.

## Dans matcha
`pg::QueryBuilder` vit dans `libs/cpp-pg`. La fonction `build()` renvoie un `pg::Query`, qu'on passe à `execute`. Le futur moteur de recherche s'appuiera dessus.

## Pourquoi les tests sont purs
Le builder ne touche pas à la base : un test compare le texte et les paramètres produits. Il s'exécute en quelques microsecondes, sans PostgreSQL :

```cpp
TEST(QueryBuilderTest, SkipsEmptyOptional) {
    pg::QueryBuilder query("SELECT id FROM t");
    std::optional<int> absent;
    query.where("a = ?", 1).whereIf(absent, "b = ?");
    EXPECT_EQ(query.build()->sql, "SELECT id FROM t WHERE a = $1");
}
```

## Erreurs fréquentes
- **Un `?` dans une chaîne littérale** : `where("name <> '?'")` serait compté comme un paramètre. Écris cette valeur en paramètre.
- **Oublier une clé secondaire de tri** : voir la [pagination](pagination-keyset.md) keyset.
- **Garder une `Query` après la mort de ses valeurs** : `Params` possède ses textes, mais `string_view` ne possède rien, voir la [durée de vie](../cpp/duree-de-vie-objets.md).

## À retenir
Le builder ne rend pas le SQL plus intelligent : il rend les erreurs de numérotation impossibles, et il concentre la règle de sécurité à un seul endroit.

## Exercice
Écris la requête produite par ce code, et la liste des paramètres :

```cpp
pg::QueryBuilder query("SELECT id FROM t");
query.limit(10).where("age >= ? AND age <= ?", 18, 30);
```

## Corrigé
`limit` est appelé en premier, donc 10 devient `$1`. `where` commence ensuite à 2.

```sql
SELECT id FROM t WHERE age >= $2 AND age <= $3 LIMIT $1
```

Paramètres : `[10, 18, 30]`. C'est le rôle de `_limitParam` : retenir le numéro, quel que soit l'ordre des appels.

## Voir aussi

- [Requêtes paramétrées et injection SQL](requetes-parametrees.md)
- [Pagination keyset ou OFFSET](pagination-keyset.md)
- [Un seul moteur pour suggestions et recherche](../metier/recherche-unifiee.md)
- [Templates variadiques et fold expressions](../cpp/templates-variadiques.md)
