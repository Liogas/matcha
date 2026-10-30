# EXPLAIN ANALYZE
> Montre comment PostgreSQL exécute réellement une requête, et où part le temps.

## Mise en situation
Les suggestions mettent 800 ms. Plutôt que d'ajouter des [index](index.md) au hasard, tu demandes à PostgreSQL ce qu'il fait.

```sql
EXPLAIN ANALYZE
SELECT account_id FROM matcha_profiles
WHERE fame_rating > 50 ORDER BY fame_rating DESC LIMIT 20;
```

## La théorie
### Le planificateur
PostgreSQL ne suit pas l'ordre de ta requête. Il estime le coût de plusieurs stratégies à partir de statistiques sur les données, puis choisit la moins chère.

`EXPLAIN` affiche le plan estimé. `EXPLAIN ANALYZE` exécute aussi la requête et ajoute les valeurs réelles. Ajouter `BUFFERS` montre les lectures de pages.

### Lire un plan
```text
Limit  (cost=0.28..1.20 rows=20 width=8) (actual time=0.02..0.05 rows=20 loops=1)
  ->  Index Scan using fame_idx on matcha_profiles
        (cost=0.28..45.10 rows=480 width=8) (actual time=0.02..0.04 rows=20 loops=1)
        Index Cond: (fame_rating > 50)
Planning Time: 0.1 ms
Execution Time: 0.08 ms
```

C'est un arbre : les nœuds les plus à droite s'exécutent d'abord, et leurs lignes remontent.

- `cost=début..fin` : estimation en unités arbitraires.
- `rows` : lignes estimées, puis réelles.
- `loops` : nombre d'exécutions. Le temps affiché est par boucle.

### Les nœuds courants
- **Seq Scan** : lit toute la table.
- **[Index](index.md) Scan** : suit l'index, puis lit la table.
- **Index Only Scan** : l'index suffit.
- **Bitmap Heap Scan** : prépare une carte, puis lit les pages. Efficace pour beaucoup de lignes.
- **Nested Loop** : pour chaque ligne à gauche, cherche à droite. Parfait si la gauche est petite.
- **Hash Join** : construit une table de hachage du petit côté.
- **Merge Join** : fusionne deux flux triés.
- **Sort** : regarde `Sort Method`. `external merge` signifie un tri sur disque.

## Exemple déroulé : un index qui change tout
Sans [index](index.md) :

```text
Limit (actual time=9.8..9.8 rows=20)
  -> Sort
       -> Seq Scan on matcha_profiles (rows=480)
            Filter: (fame_rating > 50)
            Rows Removed by Filter: 520
```

On lit 1000 lignes, on en jette 520, puis on trie. Après `CREATE INDEX ON matcha_profiles (fame_rating DESC)` :

```text
Limit (actual time=0.03..0.05 rows=20)
  -> Index Scan using ...
```

Plus de tri, et 20 lignes lues.

## La méthode
1. Trouve le nœud avec le plus grand temps réel.
2. Compare les lignes estimées et réelles. Un fort écart indique des statistiques périmées : lance `ANALYZE`.
3. Regarde `Rows Removed by Filter` : un grand nombre suggère un [index](index.md).
4. Vérifie le résultat après ta modification.

## Attention aux écritures
`EXPLAIN ANALYZE` exécute vraiment la requête. Pour un `DELETE` :

```sql
BEGIN;
EXPLAIN ANALYZE DELETE FROM profile_likes WHERE ...;
ROLLBACK;
```

## Erreurs fréquentes
- Tester sur 10 lignes : le plan n'a rien à voir.
- Croire qu'un Seq Scan est toujours mauvais.
- Regarder `cost` au lieu du temps réel.

## À retenir
On ne devine pas une performance : on la mesure, on change une chose, on remesure.

## Exercice
Un `Nested Loop` affiche `actual time=0.01..0.02 rows=1 loops=500`. Quel temps total représente cette étape, et qu'en penses-tu ?

## Corrigé
Environ 500 × 0,02 ms = 10 ms. Le chiffre affiché est par boucle. Le nœud s'exécute 500 fois : c'est le signe d'une requête exécutée pour chaque ligne, et un [index](index.md) ou une autre jointure pourrait aider.

## Voir aussi

- [Les index](index.md)
- [Pagination keyset ou OFFSET](pagination-keyset.md)
