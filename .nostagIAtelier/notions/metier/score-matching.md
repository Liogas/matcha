# Le score de matching
> Une note combine distance, tags communs et popularité pour classer les suggestions.

## Mise en situation
Le sujet veut des correspondances intelligentes, selon la proximité, les tags partagés et la popularité, avec une priorité à la même zone. En soutenance, on te demandera : pourquoi ce profil apparaît-il en premier ? Il faut une réponse chiffrée.

## La théorie
### Un score pondéré
```text
score = wd × proximité + wt × tags + wf × popularité
```

### Normaliser
Les grandeurs n'ont pas la même échelle. La popularité va de 0 à 100, les tags de 0 à 5. Sans [normalisation](../web/validation-entrees.md), la popularité écrase tout. On ramène chaque composante entre 0 et 1 :

```text
proximité = 1 - min(distance / 100, 1)
tags      = min(communs / 5, 1)
popularité = fame / 100
```

### Les poids
`wd = 0.5`, `wt = 0.3`, `wf = 0.2` donnent la priorité à la distance. Ils sont en configuration, ce qui évite de recompiler pour les ajuster.

### La priorité géographique
Une pondération ne garantit pas que les proches passent d'abord. Deux options :

- un bonus fort sous 25 km ;
- des tranches : tous les proches d'abord, et le score à l'intérieur de chaque tranche.

```sql
ORDER BY (distance_km <= 25) DESC, score DESC, account_id DESC
```

## Exemple déroulé
Deux candidats pour Alice :

```text
Bob   : 5 km,  1 tag,  fame 80
Chloé : 40 km, 4 tags, fame 30

Bob   : 0,5×0,95 + 0,3×0,2 + 0,2×0,8 = 0,475 + 0,06 + 0,16 = 0,695
Chloé : 0,5×0,60 + 0,3×0,8 + 0,2×0,3 = 0,30 + 0,24 + 0,06 = 0,60
```

Bob passe devant. C'est une explication qu'on peut donner en soutenance.

### Le calcul en SQL
1. [Bounding box](bounding-box.md) et filtres fixes.
2. Un [CTE](../sql/cte.md) pour les tags communs.
3. Le score dans la requête.
4. `ORDER BY` et `LIMIT`.

Tout calculer en C++ exigerait de charger tous les profils.

### Les exclusions
Soi-même, profils incomplets, incompatibles, bloqués, et éventuellement déjà likés.

### La pagination
Le score n'est pas une colonne : pour la [pagination](../sql/pagination-keyset.md) keyset, il doit faire partie du curseur.

## Erreurs fréquentes
- Additionner des échelles différentes.
- Des poids en dur.
- Un score qui donne toujours les mêmes profils : la popularité auto-entretenue enferme les autres.

## À retenir
Un bon score s'explique à la main : chaque composante est bornée, et chaque poids a une raison.

## Exercice
Que devient le classement si la popularité n'est pas normalisée ?

## Corrigé
Avec `0,2 × 80 = 16` contre un maximum de 0,5 pour la proximité, la popularité décide de tout, et les proches perdent leur priorité. La [normalisation](../web/validation-entrees.md) donne un sens aux poids.

## Voir aussi

- [La note de popularité](fame-rating.md)
- [Compatibilité d'orientation](orientation.md)
- [Bounding box et antiméridien](bounding-box.md)
- [Les CTE (WITH)](../sql/cte.md)
