# Pagination keyset ou OFFSET
> Au lieu de compter les lignes à sauter, on repart de la dernière ligne déjà vue.

## Mise en situation
Dans les suggestions, tu lis les 20 premiers profils. Page suivante, avec la méthode habituelle :

```sql
SELECT account_id FROM matcha_profiles ORDER BY fame_rating DESC LIMIT 20 OFFSET 20;
```

Pendant ta lecture, un nouveau profil monte dans le classement et se place en tête. Tout le reste descend d'un rang :

```text
Avant           Après une arrivée en tête
1 Alice         1 Zoé    (nouvelle)
...             2 Alice
20 Théo         ...
21 Maya         21 Théo
22 Noé          22 Maya
```

`OFFSET 20` commence maintenant au rang 21 : Théo, que tu as déjà vu, s'affiche une seconde fois. Dans l'autre sens, si quelqu'un sort du classement, un profil est sauté sans que personne ne s'en rende compte.

## La théorie
### Le coût réel d'OFFSET
`OFFSET` ne saute pas : il lit puis jette. `OFFSET 10000 LIMIT 20` construit 10 020 lignes pour en garder 20. La page 500 coûte donc beaucoup plus que la page 1. Le temps croît avec la profondeur.

### L'idée du keyset
On ne demande plus « la page 3 » mais « ce qui vient après cette ligne » :

```sql
SELECT account_id, fame_rating FROM matcha_profiles
WHERE (fame_rating, account_id) < ($1, $2)
ORDER BY fame_rating DESC, account_id DESC
LIMIT 20;
```

`$1` et `$2` sont les valeurs de la dernière ligne reçue. Avec un [index composite](index.md) `(fame_rating DESC, account_id DESC)`, PostgreSQL va directement au bon endroit de l'arbre, puis lit 20 entrées. Le coût ne dépend plus de la profondeur.

### La comparaison de lignes
`(a, b) < (x, y)` compare comme un dictionnaire :

```text
a < x   OU   (a = x ET b < y)
```

SQL sait le faire nativement, et il sait utiliser l'[index](index.md) pour cela.

### Pourquoi une colonne unique est obligatoire
Plusieurs profils peuvent avoir la note 50. Si on ne trie que par note, et que la page s'arrête au milieu de ces profils, on ne sait pas où reprendre : `fame < 50` en perd, `fame <= 50` en répète. La clé `account_id` rend l'ordre total, chaque ligne a une place unique, et la reprise est exacte.

### Le curseur
Le client ne doit pas manipuler ces valeurs. On les encode dans un [jeton opaque](../auth/tokens-opaques.md), souvent du Base64 d'un petit JSON :

```json
{"items": [...], "next_cursor": "eyJmIjo0MiwiaSI6MTIzfQ"}
```

Le client renvoie `?cursor=...`. Le serveur le décode et le valide comme toute entrée : un curseur peut être modifié à la main, donc un curseur invalide donne `400`.

## Exemple complet
Dernière ligne reçue : note 42, compte 123.

```sql
WHERE (fame_rating, account_id) < (42, 123)
```

- Une ligne `(50, 10)` est exclue : 50 > 42, elle est déjà passée.
- `(42, 200)` est exclue : même note, compte plus grand, donc déjà passée dans cet ordre descendant.
- `(42, 100)` est gardée.
- `(30, 999)` est gardée.

Si Zoé arrive en tête, elle ne change rien : la reprise dépend d'une valeur, pas d'un rang.

## Les limites à connaître
- On ne peut pas sauter directement à la page 37. Pour un fil de profils, ce n'est pas un besoin.
- Le curseur dépend du tri. Changer de tri revient à repartir du début.
- Pour un tri par score calculé, le score doit faire partie du curseur.
- Les sens mélangés (`age ASC, id DESC`) ne fonctionnent pas avec la comparaison de lignes simple. Il faut écrire la condition développée.
- `NULL` casse la comparaison : prévois `NULLS LAST` et un filtre, ou une colonne `NOT NULL`.

## Dans matcha
Le guide 06 utilise la pagination keyset sur `listCompletedProfiles`. Les notifications, les visites et les messages l'utilisent aussi avec `id DESC`.

## À retenir
OFFSET compte des lignes, le keyset se souvient d'une ligne. Le premier est simple mais instable et lent, le second est stable et rapide, à condition d'avoir une clé unique.

## Exercice
Les messages d'une conversation s'affichent du plus récent au plus ancien, triés par `id DESC`. Écris la requête de la page suivante. Pourquoi une seule colonne suffit-elle ici ?

## Corrigé
```sql
SELECT id, body FROM messaging_messages
WHERE conversation_id = $1 AND id < $2
ORDER BY id DESC LIMIT 50;
```

`id` est unique, donc l'ordre est déjà total : aucune colonne de départage n'est nécessaire. L'[index](index.md) utile est `(conversation_id, id DESC)`.

## Voir aussi

- [Les index](index.md)
- [Un QueryBuilder sûr](query-builder.md)
- [EXPLAIN ANALYZE](explain-analyze.md)
- [Un seul moteur pour suggestions et recherche](../metier/recherche-unifiee.md)
