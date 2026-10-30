# Les CTE (WITH)
> Une sous-requête nommée, pour découper une grosse requête en étapes lisibles.

## Mise en situation
Le score des suggestions combine compatibilité, tags communs et distance. Écrit d'un seul bloc avec des sous-requêtes imbriquées, c'est illisible. Un CTE donne un nom à chaque étape.

## La théorie
### La syntaxe
```sql
WITH compatibles AS (
    SELECT account_id FROM matcha_profiles WHERE completed_at IS NOT NULL
), communs AS (
    SELECT other.profile_account_id AS account_id, COUNT(*) AS n
    FROM matcha_profile_interests me
    JOIN matcha_profile_interests other USING (interest_id)
    WHERE me.profile_account_id = $1
    GROUP BY other.profile_account_id
)
SELECT c.account_id, COALESCE(x.n, 0) AS tags_communs
FROM compatibles c
LEFT JOIN communs x USING (account_id)
ORDER BY tags_communs DESC;
```

Chaque CTE est comme une table [temporaire](../cpp/duree-de-vie-objets.md), visible par les suivants. `LEFT JOIN` garde les profils sans tag commun, et `COALESCE` remplace `NULL` par 0.

### Exécution
Depuis PostgreSQL 12, un CTE simple est fusionné dans la requête principale, et l'optimiseur peut y pousser les filtres. Pour forcer un calcul unique : `AS MATERIALIZED`.

### CTE d'écriture
```sql
WITH nouveau AS (
    INSERT INTO profile_likes (liker_account_id, liked_account_id)
    VALUES ($1, $2) RETURNING id
)
SELECT id FROM nouveau;
```

## Le point clé : une seule photo
Toutes les parties d'une instruction voient le même état de la base, pris au début de l'instruction. Un CTE ne voit donc pas ce qu'un CTE voisin vient d'insérer. Il voit l'état d'avant.

C'est important pour le like mutuel. L'ancienne requête ressemblait à :

```sql
WITH inserted AS (INSERT INTO profile_likes ...),
     reciprocal AS (SELECT ... FROM profile_likes WHERE ...)
INSERT INTO profile_matches ...
```

Si A et B exécutent cela en même temps, chaque instruction a sa photo, sans le like de l'autre. Aucun match n'est créé. Voir l'[isolation](isolation.md).

## Exemple déroulé
```sql
WITH a AS (INSERT INTO t (v) VALUES (1) RETURNING v)
SELECT (SELECT COUNT(*) FROM t) AS dans_t, (SELECT COUNT(*) FROM a) AS dans_a;
```

Si `t` est vide, le résultat est `dans_t = 0`, `dans_a = 1` : la ligne n'est visible que par `RETURNING`.

## Erreurs fréquentes
- Compter sur l'ordre d'exécution des CTE d'écriture.
- Croire qu'un CTE est toujours calculé une fois.
- Faire un CTE là où une simple jointure suffit.

## À retenir
Un CTE rend une requête lisible. Il ne crée pas d'étape dans le temps : tout se passe dans la même photo.

## Exercice
Dans une même instruction, un CTE fait `UPDATE t SET n = 2`, et le `SELECT` principal lit `n`. Que lit-il ?

## Corrigé
L'ancienne valeur, 1, car il lit la photo du début d'instruction. Pour voir la nouvelle valeur, on utilise `RETURNING n` dans le CTE, et on lit ce résultat.

## Voir aussi

- [Les transactions et ACID](transactions.md)
- [Les niveaux d'isolation](isolation.md)
- [EXPLAIN ANALYZE](explain-analyze.md)
