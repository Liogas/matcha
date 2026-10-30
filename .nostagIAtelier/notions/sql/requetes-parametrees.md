# Requêtes paramétrées et injection SQL
> La structure de la requête et les valeurs voyagent séparément : une valeur ne peut jamais devenir du SQL.

## Mise en situation
Tu écris la recherche par ville. Le plus naturel est d'assembler la requête comme une phrase :

```cpp
std::string sql = "SELECT account_id FROM matcha_profiles WHERE city = '" + ville + "'";
```

Avec `Lyon`, le serveur reçoit `WHERE city = 'Lyon'`. Tout va bien. Maintenant, un utilisateur tape dans le champ ville :

```text
' OR '1'='1
```

La chaîne assemblée devient :

```sql
SELECT account_id FROM matcha_profiles WHERE city = '' OR '1'='1'
```

La condition est toujours vraie et tous les profils sortent. Le client a réécrit ta requête, parce que son texte et le tien ont été mélangés. Avec une requête plus sensible, il pourrait lire la table des comptes ou contourner une connexion. Le sujet le dit clairement : une faille de sécurité vaut 0.

## La théorie
### Pourquoi l'injection existe
Un serveur SQL reçoit un texte et l'analyse pour comprendre sa structure : quelles tables, quelles conditions, quelles valeurs. Si des données de l'utilisateur sont dans ce texte, l'analyseur ne peut pas les distinguer de ce que tu as écrit. Le `'` qui ferme la chaîne a le même sens, qu'il vienne de toi ou de l'attaquant.

C'est le même mécanisme que la [XSS](../web/xss-echappement.md) : du code et des données partagent le même canal. La vraie solution consiste à séparer les canaux.

### Le protocole en deux temps
Avec une requête paramétrée, le client envoie deux choses distinctes :

1. le texte de la requête, avec des trous : `WHERE city = $1` ;
2. la liste des valeurs : `["Lyon"]`.

Le serveur analyse le texte d'abord, donc la structure est figée avant même de voir les valeurs. Quand `' OR '1'='1` arrive, c'est simplement une ville au nom étrange. Le serveur la compare telle quelle et ne trouve aucun profil. Rien n'a été échappé : il n'y a rien à échapper, car la valeur n'est jamais interprétée.

### Ce qu'un paramètre peut remplacer
Un `$n` remplace uniquement une valeur. Il ne remplace jamais un élément de structure :

- un nom de table ou de colonne ;
- un mot-clé comme `ASC` ou `DESC` ;
- une liste de colonnes dans `SELECT`.

C'est logique, puisque la structure doit être connue avant les valeurs. `ORDER BY $1` ne provoque pas d'erreur, mais il trie par une constante, donc il ne trie rien.

### La liste blanche pour la structure
Pour un tri choisi par le client, on n'insère jamais son texte. On lui fait choisir parmi des fragments que nous avons écrits :

```cpp
const pg::SortWhitelist kSorts = {
    {"age", "date_of_birth DESC, account_id"},
    {"fame", "fame_rating DESC, account_id"},
};
```

`?sort=fame` sert de clé de recherche. La chaîne du client n'atteint jamais le SQL. Une clé inconnue donne une erreur `Invalid`, puis un `400`.

## Exemple complet
Suivons `?city=' OR '1'='1` à travers le code :

```cpp
auto rows = db.query(
    "SELECT account_id FROM matcha_profiles WHERE city = $1", ville);
```

1. `query` appelle `pg::params(ville)` pour construire les paramètres.
2. `execute` appelle `PQexecParams(conn, sql, 1, nullptr, values, ...)`.
3. Le serveur analyse le texte, sans connaître la valeur.
4. Il exécute le plan avec une comparaison à la chaîne `' OR '1'='1`.
5. Il renvoie 0 ligne.

## Dans matcha
Toutes les requêtes passent par `pg::Executor::query` ou `execute`, qui utilisent [libpq](libpq.md). Le [QueryBuilder](query-builder.md) applique la même règle aux filtres optionnels. Le seul endroit où du texte est assemblé, c'est le nom de table des [migrations](migrations.md), qui vient d'une option du programme et qui est contrôlé par le motif `[a-z_][a-z0-9_]*`.

## Erreurs fréquentes
- **Concaténer un entier « parce que c'est sûr »** : c'est vrai aujourd'hui, mais l'habitude finit par s'étendre à une chaîne.
- **Croire que l'[échappement](../web/xss-echappement.md) manuel suffit** : les règles dépendent de l'encodage, et une seule exception suffit.
- **Oublier les jokers de LIKE** : dans `name LIKE $1`, la valeur est sûre, mais `%` et `_` restent des jokers. Un utilisateur peut envoyer `%` et tout lister, ou provoquer une recherche coûteuse.
- **Valider au lieu de paramétrer** : la validation rend les données propres, le paramètre rend la requête sûre. Il faut les deux.

## À retenir
Une requête paramétrée ne nettoie pas la valeur : elle garantit qu'elle ne sera jamais lue comme du SQL. Pour la structure, on utilise une [liste blanche](../web/validation-entrees.md).

## Exercice
On te propose ce code pour trier les profils :

```cpp
db.query("SELECT account_id FROM matcha_profiles ORDER BY " + tri + " LIMIT $1", 20);
```

Quel est le risque, et comment corriger ?

## Corrigé
`tri` vient de l'URL et entre dans la structure de la requête. Un client peut y placer une sous-requête ou un `CASE` qui révèle des données selon l'ordre obtenu. Il ne peut pas passer en `$n`, car un paramètre ne remplace pas une colonne. La bonne correction est une `SortWhitelist` : on cherche `tri` dans la table, puis on utilise le fragment écrit par nous, ou on répond `400`.

## Voir aussi

- [Un QueryBuilder sûr](query-builder.md)
- [libpq, l'API C de PostgreSQL](libpq.md)
- [Valider les entrées](../web/validation-entrees.md)
- [XSS et échappement](../web/xss-echappement.md)
