# Les relations N-N
> Un profil a plusieurs tags, un tag a plusieurs profils : il faut une table de liaison.

## Mise en situation
Le sujet demande des tags réutilisables. Une colonne `tags TEXT` contenant `"vegan,geek"` semble simple. Mais comment trouver les profils `geek` ? Comment compter les tags communs ? Comment proposer les tags existants ? Comment renommer un tag ? Une liste dans une case est invisible pour la base.

## La théorie
### Normaliser
Une relation N-N se modélise par trois tables :

```sql
matcha_profiles(account_id, ...)
matcha_interests(id, name, created_at)
matcha_profile_interests(profile_account_id, interest_id, created_at)
```

Chaque ligne de la table de liaison est un fait : « ce profil a ce tag ». La clé primaire est le couple, ce qui interdit de l'avoir deux fois.

### Les relations possibles
- **1-1** : un compte, un profil. Séparer `auth_accounts` et `matcha_profiles` isole l'authentification.
- **1-N** : un profil, plusieurs photos. La [clé étrangère](contraintes.md) est côté N.
- **N-N** : table de liaison.

### Les avantages
- Un tag n'est stocké qu'une fois.
- Les [contraintes](contraintes.md) s'appliquent : `ON DELETE RESTRICT` protège un tag utilisé.
- Un [index](index.md) rend les recherches rapides.

## Exemple déroulé : les tags communs
```sql
SELECT other.profile_account_id, COUNT(*) AS common
FROM matcha_profile_interests me
JOIN matcha_profile_interests other ON other.interest_id = me.interest_id
WHERE me.profile_account_id = $1 AND other.profile_account_id <> $1
GROUP BY other.profile_account_id
ORDER BY common DESC;
```

C'est une auto-jointure : la table sert deux fois, sous deux noms.

Exemple avec Alice (`vegan`, `geek`), Bob (`geek`) et Chloé (`vegan`, `geek`) :

```text
me (Alice)      other
vegan           Chloé
geek            Bob
geek            Chloé
```

Après `GROUP BY` : Chloé 2, Bob 1.

## Remplacer les tags d'un profil
`PUT /api/profile/interests` est une opération en [transaction](transactions.md) :

```sql
INSERT INTO matcha_interests (name)
SELECT unnest($1::text[]) ON CONFLICT (LOWER(name)) DO NOTHING;

DELETE FROM matcha_profile_interests WHERE profile_account_id = $2;

INSERT INTO matcha_profile_interests (profile_account_id, interest_id)
SELECT $2, id FROM matcha_interests WHERE LOWER(name) = ANY($3::text[]);
```

Sans transaction, un échec après le `DELETE` laisserait le profil sans tags.

## Erreurs fréquentes
- Ne pas normaliser les noms : `Vegan`, `vegan` et `#vegan` feraient trois tags.
- Oublier un [index](index.md) sur `interest_id` : la clé primaire ne sert que dans l'ordre `(profile_account_id, interest_id)`.
- Ne pas limiter le nombre de tags par profil.

## À retenir
Une liste dans une colonne cache l'information. Une ligne par lien la rend interrogeable.

## Exercice
Écris la requête qui liste les profils ayant au moins un des tags `vegan` ou `geek`.

## Corrigé
```sql
SELECT DISTINCT pi.profile_account_id
FROM matcha_profile_interests pi
JOIN matcha_interests i ON i.id = pi.interest_id
WHERE LOWER(i.name) = ANY($1::text[]);
```

`DISTINCT` évite de compter deux fois un profil qui a les deux tags. Pour tous les tags, on regroupe et on compare `COUNT(DISTINCT i.id)` au nombre demandé.

## Voir aussi

- [INSERT ON CONFLICT](on-conflict.md)
- [Tableaux et unnest](tableaux-unnest.md)
- [Les contraintes d'intégrité](contraintes.md)
