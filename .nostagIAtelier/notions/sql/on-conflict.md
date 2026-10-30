# INSERT ON CONFLICT
> Dit à PostgreSQL quoi faire si une insertion viole une [contrainte](contraintes.md) d'unicité.

## Mise en situation
Un utilisateur enregistre `vegan`, et ce tag existe peut-être. Le réflexe est de tester avant d'insérer :

```text
T1 : SELECT ... vegan ?  -> non
T2 : SELECT ... vegan ?  -> non
T1 : INSERT vegan        -> ok
T2 : INSERT vegan        -> erreur 23505
```

Le motif tester-puis-agir n'est pas atomique, même dans une [transaction](transactions.md) en [`READ COMMITTED`](isolation.md). L'erreur empoisonne en plus toute la transaction de T2.

## La théorie
`ON CONFLICT` rend la décision atomique : l'insertion consulte l'[index](index.md) unique et attend, si nécessaire, la [transaction](transactions.md) concurrente.

### DO NOTHING
```sql
INSERT INTO matcha_interests (name)
VALUES ($1)
ON CONFLICT (LOWER(name)) DO NOTHING;
```

La cible désigne l'[index](index.md) unique : ici l'expression `LOWER(name)`.

### DO UPDATE, ou upsert
```sql
INSERT INTO profile_views (viewer_id, viewed_id, last_viewed_at, view_count)
VALUES ($1, $2, NOW(), 1)
ON CONFLICT (viewer_id, viewed_id)
DO UPDATE SET last_viewed_at = EXCLUDED.last_viewed_at,
              view_count = profile_views.view_count + 1;
```

`EXCLUDED` désigne la ligne qu'on tentait d'insérer. `profile_views.view_count` désigne la ligne existante.

### RETURNING
`DO NOTHING RETURNING id` ne renvoie rien en cas de conflit. Pour obtenir l'id dans tous les cas :

```sql
WITH nouveau AS (
    INSERT INTO matcha_interests (name) VALUES ($1)
    ON CONFLICT (LOWER(name)) DO NOTHING
    RETURNING id
)
SELECT id FROM nouveau
UNION ALL
SELECT id FROM matcha_interests WHERE LOWER(name) = LOWER($1)
LIMIT 1;
```

## Exemple déroulé : dédoublonner les visites
Tu veux une ligne par couple et par heure. Bob visite Alice trois fois en 10 minutes :

```text
Visite 1 : INSERT, nouvelle ligne
Visite 2 : conflit, last_viewed_at mis à jour
Visite 3 : conflit, last_viewed_at mis à jour
```

On garde une ligne par heure en ajoutant une colonne d'heure tronquée à l'[index](index.md) unique. Une expression basée sur `date_trunc` et `timestamptz` n'étant pas immuable, une colonne calculée par l'application est la solution simple.

## Erreurs fréquentes
- Une cible qui ne correspond à aucun [index](index.md) unique : erreur `42P10`.
- `DO UPDATE` avec deux fois la même clé dans un même `INSERT` : erreur, il faut dédoublonner l'entrée.
- S'attendre à des identifiants consécutifs : une séquence avance même en cas de conflit.

## À retenir
Quand une action dépend de « est-ce que ça existe déjà ? », fais-la en une seule instruction.

## Exercice
Pourquoi `ON CONFLICT DO NOTHING` n'empoisonne-t-il pas la [transaction](transactions.md), alors qu'un `INSERT` en conflit le ferait ?

## Corrigé
Parce qu'aucune erreur n'est levée : le conflit est traité comme un cas normal, et l'instruction se termine en succès, avec 0 ligne insérée.

## Voir aussi

- [Les relations N-N](relation-nn.md)
- [Les contraintes d'intégrité](contraintes.md)
- [Tableaux et unnest](tableaux-unnest.md)
