# Les contraintes d'intégrité
> Les règles déclarées dans le schéma sont vérifiées par la base, pour toutes les requêtes et tous les chemins de code.

## Mise en situation
Le code vérifie qu'on ne se like pas soi-même. Mais un script de seed, une nouvelle route ou un bug peuvent l'oublier. Une règle qui n'existe que dans le C++ est une règle qu'un seul chemin respecte. La table `profile_likes` le garantit en elle-même :

```sql
CONSTRAINT profile_likes_no_self_like
    CHECK (liker_account_id <> liked_account_id),
CONSTRAINT profile_likes_unique_pair
    UNIQUE (liker_account_id, liked_account_id)
```

## La théorie
### Les types
- **`PRIMARY KEY`** : identifie chaque ligne, unique et non nulle.
- **`FOREIGN KEY`** : la valeur doit exister dans la table référencée.
- **`UNIQUE`** : pas de doublon, grâce à un [index](index.md).
- **`NOT NULL`** : valeur obligatoire.
- **`CHECK`** : une condition sur la ligne.

### Unicité sur une expression
```sql
CREATE UNIQUE INDEX matcha_interests_name_lower_unique
    ON matcha_interests (LOWER(name));
```

`Vegan` et `vegan` ne peuvent pas coexister. Ce cas n'est pas exprimable en `UNIQUE` simple.

### Unicité partielle
```sql
CREATE UNIQUE INDEX matcha_profile_photos_one_primary
    ON matcha_profile_photos (profile_account_id)
    WHERE is_primary;
```

L'unicité ne concerne que les lignes avec `is_primary`. Une seule photo principale par profil : la règle est garantie en une ligne.

### Le comportement à la suppression
- `ON DELETE CASCADE` : supprime les lignes liées.
- `ON DELETE RESTRICT` : refuse la suppression, comme pour un tag utilisé.
- `ON DELETE SET NULL` : met la colonne à `NULL`.

Le choix a des effets métier. `unlike` supprime aujourd'hui le match, et la cascade supprime la conversation et ses messages, alors que le sujet parle de chat désactivé. C'est la carte #46.

### NULL et les contraintes
`NULL` signifie inconnu. Une condition `CHECK` qui vaut inconnu passe. Ajoute `NOT NULL` si la valeur doit exister. Pour `UNIQUE`, deux `NULL` sont considérés comme différents.

## Exemple déroulé : la paire canonique
Un match entre 7 et 12 peut s'écrire `(7,12)` ou `(12,7)`. Sans règle, deux lignes pourraient coexister. `profile_matches` impose :

```sql
CHECK (first_account_id < second_account_id)
UNIQUE (first_account_id, second_account_id)
```

Une seule écriture est valide, `(7,12)`, et l'unicité empêche les doublons. Le code doit trier la paire avant l'insertion, avec `LEAST` et `GREATEST`. En cas d'erreur, la base refuse.

## Double validation
Le C++ valide pour produire un message clair, en validation. La base valide pour garantir la vérité, même en cas de bug. Sur le genre et l'orientation, il manque aujourd'hui la seconde barrière (audit 4.13).

## Erreurs fréquentes
- Ajouter un `CHECK` sans vérifier les données existantes : la [migration](migrations.md) échoue.
- Tester puis insérer, au lieu de laisser l'unicité trancher.
- Oublier l'[index](index.md) sur une clé étrangère : `ON DELETE CASCADE` doit alors parcourir toute la table.

## À retenir
Le schéma est ta dernière ligne de défense : ce qui y est écrit reste vrai, quel que soit le code qui s'exécute.

## Exercice
Écris la contrainte qui garantit qu'une personne ne peut pas signaler deux fois le même compte, et celle qui empêche de se signaler soi-même.

## Corrigé
```sql
CONSTRAINT user_reports_unique_pair UNIQUE (reporter_id, reported_id),
CONSTRAINT user_reports_no_self CHECK (reporter_id <> reported_id)
```

## Voir aussi

- [Les relations N-N](relation-nn.md)
- [Les codes SQLSTATE](sqlstate.md)
- [Les migrations](migrations.md)
- [Valider les entrées](../web/validation-entrees.md)
