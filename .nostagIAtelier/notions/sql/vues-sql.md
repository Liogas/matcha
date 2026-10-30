# Les vues SQL
> Une requête nommée, utilisable comme une table, pour écrire une règle une seule fois.

## Mise en situation
Le blocage doit s'appliquer aux suggestions, à la recherche, à la consultation, aux likes, aux notifications et aux messages. Si chaque requête recopie son `NOT EXISTS`, un oubli suffit pour qu'un utilisateur bloqué réapparaisse. De même, la règle de complétude du profil est aujourd'hui copiée dans quatre requêtes.

## La théorie
### Une vue
```sql
CREATE VIEW blocked_pairs AS
    SELECT blocker_id AS a, blocked_id AS b FROM user_blocks
    UNION ALL
    SELECT blocked_id, blocker_id FROM user_blocks;
```

Une vue ne stocke rien. C'est un nom pour une requête : au moment de l'exécution, son texte est intégré à la requête appelante, et le planificateur optimise le tout. Les [index](index.md) des tables d'origine sont donc utilisés.

Le `UNION ALL` exprime la symétrie : si A bloque B, A ne voit plus B, et B ne voit plus A.

### Avec un paramètre : la fonction
Une vue n'a pas de paramètre. Une fonction SQL peut en avoir :

```sql
CREATE FUNCTION is_blocked(viewer BIGINT, other BIGINT)
RETURNS boolean LANGUAGE sql STABLE AS $$
    SELECT EXISTS (
        SELECT 1 FROM user_blocks
        WHERE (blocker_id = viewer AND blocked_id = other)
           OR (blocker_id = other AND blocked_id = viewer))
$$;
```

`STABLE` promet qu'elle ne modifie rien, et qu'elle renvoie le même résultat dans une instruction.

### La vue matérialisée
`CREATE MATERIALIZED VIEW` stocke réellement le résultat. Lecture rapide, mais il faut la rafraîchir avec `REFRESH`. Inutile ici : un blocage doit agir tout de suite.

## Exemple déroulé
```sql
SELECT p.account_id
FROM matcha_profiles p
WHERE p.completed_at IS NOT NULL
  AND NOT EXISTS (
      SELECT 1 FROM blocked_pairs bp
      WHERE bp.a = $1 AND bp.b = p.account_id);
```

PostgreSQL transforme ce `NOT EXISTS` en anti-jointure : il garde les profils sans correspondance. Si un [index](index.md) existe sur `(blocker_id, blocked_id)` et `(blocked_id, blocker_id)`, le coût reste faible.

### NOT EXISTS ou NOT IN
Préfère `NOT EXISTS`. `NOT IN (...)` ne renvoie aucune ligne dès que la sous-requête contient un `NULL`, à cause de la logique à trois valeurs.

## Les limites
Une vue centralise un fragment SQL, pas l'obligation de l'utiliser. Un développeur peut toujours l'oublier. D'où les tests par point d'entrée, voir la règle centralisée.

## Erreurs fréquentes
- Empiler des vues : le plan devient difficile à comprendre.
- Modifier une vue sans [migration](migrations.md).
- Utiliser `NOT IN`.

## À retenir
Une vue donne un nom à une règle. Le jour où la règle change, elle change à un seul endroit.

## Exercice
Pourquoi le blocage doit-il être symétrique dans la vue, alors que la table ne contient qu'un sens ?

## Corrigé
La table enregistre qui a décidé, utile pour permettre le déblocage. L'effet, lui, va dans les deux sens : aucun des deux ne doit voir l'autre. Stocker une seule ligne et la lire dans les deux sens évite de maintenir deux lignes cohérentes.

## Voir aussi

- [Une règle transversale centralisée](../metier/policy-centralisee.md)
- [Dénormalisation et triggers](denormalisation.md)
- [Les index](index.md)
