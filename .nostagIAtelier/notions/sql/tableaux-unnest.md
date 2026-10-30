# Tableaux et unnest
> On passe une liste entière dans un seul paramètre, puis on la déplie en lignes.

## Mise en situation
L'utilisateur enregistre 8 tags. Tu pourrais construire `VALUES ($1), ($2), ... ($8)`, mais le nombre de paramètres varie, et la requête change à chaque fois. PostgreSQL sait recevoir un tableau comme une seule valeur.

## La théorie
### Le type tableau
Tout type a sa version tableau : `text[]`, `bigint[]`. En format texte, un tableau s'écrit `{vegan,geek}`.

Une chaîne contenant une virgule ou un guillemet doit être entre guillemets : `{"a,b","c\"d"}`. Cette écriture est la responsabilité de `Params`, jamais de l'appelant.

### unnest
```sql
SELECT unnest('{vegan,geek}'::text[]);
```

renvoie deux lignes. Le tableau devient une table.

### Plusieurs colonnes
```sql
SELECT * FROM unnest($1::bigint[], $2::text[]) AS t(id, nom);
```

Les tableaux sont dépliés en parallèle : une technique simple pour une insertion groupée.

### ANY
```sql
SELECT * FROM matcha_profiles WHERE account_id = ANY($1::bigint[]);
```

C'est l'équivalent de `IN (...)`, avec un seul paramètre, quelle que soit la taille de la liste.

### Le cast
Le serveur doit connaître le type du paramètre. Sans `::text[]`, tu obtiens `could not determine data type`, ou un type inattendu.

## Exemple déroulé
```cpp
std::vector<std::string> tags = {"vegan", "geek"};
db.query("INSERT INTO matcha_interests (name) "
         "SELECT unnest($1::text[]) ON CONFLICT (LOWER(name)) DO NOTHING", tags);
```

1. `Params` transforme le vecteur en `{"vegan","geek"}`.
2. [`PQexecParams`](libpq.md) l'envoie comme une seule valeur.
3. `unnest` produit deux lignes.
4. Chaque ligne suit le chemin normal, y compris [ON CONFLICT](on-conflict.md).

La sécurité est la même que pour un paramètre simple : aucune valeur n'est interprétée comme du SQL.

## Erreurs fréquentes
- Écrire le tableau à la main dans le code.
- Oublier les `NULL` : dans un tableau, le mot `NULL` sans guillemets signifie NULL, et `"NULL"` avec guillemets est une chaîne.
- Doublon dans le tableau avec un `DO UPDATE` : erreur.
- `name <> ALL($1)` avec un tableau vide est vrai partout : réfléchis au cas vide.

## À retenir
Un tableau en paramètre remplace une requête de taille variable par une requête fixe.

## Exercice
Que renvoie `WHERE id = ANY($1)` si `$1` est un tableau vide ?

## Corrigé
Aucune ligne : aucune valeur ne peut être égale à un élément d'une liste vide. Il n'y a donc pas d'erreur de syntaxe, contrairement à `IN ()`.

## Voir aussi

- [Requêtes paramétrées et injection SQL](requetes-parametrees.md)
- [Les relations N-N](relation-nn.md)
- [INSERT ON CONFLICT](on-conflict.md)
