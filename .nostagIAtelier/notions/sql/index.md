# Les index
> Une structure triée à part, qui permet de trouver des lignes sans lire toute la table.

## Mise en situation
Avec 10 profils, tout va vite. Avec 500, puis des milliers de likes et de notifications, `GET /api/notifications` doit trouver les lignes d'un utilisateur parmi toutes les autres. Sans index, PostgreSQL parcourt toute la table, à chaque requête.

Imagine un livre sans index à la fin : pour trouver un mot, tu lis tout.

### Compter les pages lues
Une table de 1 000 000 de notifications. PostgreSQL range ses lignes dans des pages de 8 Ko. Cherche celles du compte 42.

```tableau
titre: Avec et sans index
colonnes: Requête | Index | Table
étape: Sans index, il faut lire toute la table, quelle que soit la taille du résultat.
! Requête -> Table: lit 1 000 000 de lignes
étape: Avec un index, on descend l'arbre : quelques pages suffisent, même sur un million de lignes.
+ Requête -> Index: 4 niveaux
étape: Les feuilles donnent les emplacements. On ne lit que les lignes concernées.
+ Index -> Table: 20 lignes précises
étape: La différence grandit avec la table : doubler la table ajoute à peine une lecture à l'index.
+ = la recherche passe de N à log N
```

## La théorie
### Le B-tree
Un index B-tree est un arbre équilibré et trié. Chaque niveau réduit fortement la recherche :

```text
             [ 500 ]
          /           \
    [ 100 300 ]    [ 700 900 ]
    /    |    \      ...
 feuilles triées, reliées entre elles
```

Pour un million de lignes, quelques niveaux suffisent. Comme les feuilles sont reliées et triées, l'index sert aussi aux plages et au tri : `WHERE age BETWEEN 20 AND 30`, `ORDER BY`.

### L'index composite
Un index sur `(a, b)` est trié par `a`, puis par `b` à `a` égal, comme un annuaire par nom puis prénom.

- `WHERE a = 1` : oui.
- `WHERE a = 1 AND b = 2` : oui.
- `WHERE a = 1 ORDER BY b` : oui, sans tri.
- `WHERE b = 2` : très peu utile.

Règle : les colonnes d'égalité d'abord, puis celle de la plage ou du tri.

### Exemple des notifications
```sql
CREATE INDEX ON notifications (recipient_id, read_at, id DESC);
```

Il répond à : « les non lues de ce destinataire, les plus récentes d'abord ». Il filtre, il trie, et il évite un tri coûteux.

### Les autres formes
- **Index partiel** : `WHERE read_at IS NULL`. Plus petit, il ne contient que ce qu'on cherche.
- **Index d'expression** : `LOWER(email)`.
- **Index couvrant** : `INCLUDE (colonne)` permet de répondre sans lire la table.
- **GIN** : pour tableaux, [JSONB](jsonb.md), trigrammes.

### Le coût
Un index ralentit chaque écriture, car il faut le tenir à jour, et il occupe de la place. On en crée pour de vraies requêtes, vérifiées avec [EXPLAIN ANALYZE](explain-analyze.md).

## Exemple déroulé
```sql
SELECT id FROM profile_likes WHERE liked_account_id = 7
ORDER BY created_at DESC LIMIT 20;
```

La [migration](migrations.md) 007 a créé `(liked_account_id, created_at DESC)`. PostgreSQL :

1. descend dans l'arbre jusqu'à l'entrée 7 ;
2. lit les 20 premières entrées, déjà dans le bon ordre ;
3. s'arrête.

Sans cet index, il lirait tous les likes, filtrerait, trierait, puis garderait 20 lignes.

## Quand l'index n'est pas utilisé
- La table est petite : tout lire est plus rapide.
- Le filtre garde une grande partie de la table.
- La colonne est transformée : `WHERE LOWER(email) = $1` n'utilise pas un index sur `email`.
- `LIKE '%abc'` : le début n'est pas connu, voir [pg_trgm](recherche-floue.md).

## Exemple pas à pas : l'ordre des colonnes
Un index `(recipient_id, read_at, id DESC)` est trié comme un annuaire. Tout dépend de ce qu'on peut chercher dans cet ordre.

```tableau
titre: Ce que peut faire un index composite
étape: On fixe le destinataire : tout est regroupé, l'index s'applique directement.
+ > WHERE recipient_id = 42
étape: On fixe aussi `read_at` : on descend d'un niveau dans le tri.
+ > WHERE recipient_id = 42 AND read_at IS NULL
étape: Le tri sur `id` est déjà celui de l'index : aucun tri à faire.
+ > ... ORDER BY id DESC LIMIT 20
étape: Sans le destinataire, c'est comme chercher un prénom dans un annuaire trié par nom.
! > WHERE read_at IS NULL
```

## Erreurs fréquentes
- Un index par colonne au lieu d'un index composite adapté.
- Oublier d'indexer une [clé étrangère](contraintes.md).
- Conclure sur 10 lignes.

## À retenir
Un index, c'est de l'ordre préparé à l'avance. Choisis l'ordre des colonnes selon la requête.

## Exercice
Quel index choisir pour `SELECT ... FROM messaging_messages WHERE conversation_id = $1 ORDER BY id DESC LIMIT 50` ?

## Corrigé
`(conversation_id, id DESC)`. L'égalité d'abord, puis le tri. PostgreSQL trouve la conversation et lit 50 entrées dans l'ordre, sans tri.

## Voir aussi

- [EXPLAIN ANALYZE](explain-analyze.md)
- [Pagination keyset ou OFFSET](pagination-keyset.md)
- [Autocomplétion et pg_trgm](recherche-floue.md)

<!-- enrichie -->
