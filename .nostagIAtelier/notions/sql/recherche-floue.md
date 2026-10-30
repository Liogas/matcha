# Autocomplétion et pg_trgm
> Trouver les tags qui commencent par, ou ressemblent à, ce que l'utilisateur tape.

## Mise en situation
`GET /api/interests?q=veg` doit proposer `vegan` et `vegetarien` à chaque frappe, en quelques millisecondes. Et une personne qui tape `vgean` aimerait peut-être trouver `vegan`.

## La théorie
### La recherche par préfixe
```sql
SELECT name FROM matcha_interests
WHERE name LIKE $1 || '%'
ORDER BY name LIMIT 10;
```

Un [B-tree](index.md) trie les textes : un préfixe est une plage, facile à trouver. Mais cela dépend de la collation. Avec une collation autre que `C`, il faut :

```sql
CREATE INDEX ON matcha_interests (name text_pattern_ops);
```

`LIKE '%veg%'` ne profite pas du B-tree : le début est inconnu, comme chercher un mot dont on connaît seulement le milieu dans un dictionnaire.

### Les trigrammes
`pg_trgm` découpe un texte en groupes de 3 caractères :

```text
vegan -> "  v", " ve", "veg", "ega", "gan", "an "
```

Deux textes qui partagent beaucoup de trigrammes se ressemblent. Un [index](index.md) GIN inverse la logique : pour chaque trigramme, il connaît les lignes qui le contiennent.

```sql
CREATE EXTENSION IF NOT EXISTS pg_trgm;
CREATE INDEX ON matcha_interests USING gin (name gin_trgm_ops);

SELECT name, similarity(name, $1) AS s
FROM matcha_interests
WHERE name % $1
ORDER BY s DESC LIMIT 10;
```

`%` signifie « assez similaire ». Cet index accélère aussi `LIKE '%veg%'`.

## Exemple déroulé
L'utilisateur tape `#Veg` :

1. Le serveur retire `#` et passe en minuscules : c'est la [normalisation](../web/validation-entrees.md).
2. Il échappe `%` et `_`.
3. Il cherche `veg%`.
4. Il renvoie 10 noms au plus.

Côté front, on attend 200 ms après la dernière frappe avant d'appeler. Sinon chaque lettre déclenche une requête.

## Quel choix pour matcha
Le préfixe suffit : le vocabulaire est petit, et le besoin est de réutiliser des tags. `pg_trgm` est une amélioration éventuelle.

## Erreurs fréquentes
- Oublier d'échapper `%`.
- Pas de `LIMIT`.
- Un [index](index.md) sur `name` alors que la requête utilise `LOWER(name)`.

## À retenir
Un préfixe se résout avec un [B-tree](index.md), une sous-chaîne ou une ressemblance demande des trigrammes.

## Exercice
Pourquoi faut-il échapper `%` dans `q`, alors que la valeur passe par un paramètre ?

## Corrigé
Le paramètre empêche l'[injection SQL](requetes-parametrees.md), mais `LIKE` interprète ensuite `%` comme un joker dans la valeur. Un `q` égal à `%` listerait tout. La sécurité SQL et le sens du motif sont deux sujets différents.

## Voir aussi

- [Les index](index.md)
- [Requêtes paramétrées et injection SQL](requetes-parametrees.md)
