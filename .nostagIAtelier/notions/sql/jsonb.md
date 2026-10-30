# JSONB
> Un type PostgreSQL qui stocke du JSON dans un format binaire, indexable et interrogeable.

## Mise en situation
La table `notifications` contient plusieurs types : like, visite, match, message, unlike. Chacun a ses propres détails. Ajouter une colonne par détail laisserait la plupart vides, et surtout [`cpp-notify`](../temps-reel/notifications-stockees.md) ne doit pas connaître les types de matcha. Il lui faut une place pour une donnée opaque.

## La théorie
### JSON ou JSONB
- `JSON` garde le texte exact, et le réanalyse à chaque lecture.
- `JSONB` le décompose à l'écriture, sans doublons de clés ni espaces. L'écriture coûte un peu plus, mais la lecture est rapide et l'indexation possible.

On choisit `JSONB`, sauf besoin de garder le texte exact.

### Les opérateurs
```sql
payload -> 'actor'            -- renvoie du JSON
payload ->> 'conversation_id' -- renvoie du texte
payload @> '{"kind":"match"}' -- contient ce fragment
payload ? 'actor'             -- possède cette clé
```

### L'indexation
```sql
CREATE INDEX ON notifications USING gin (payload);
```

Un [index](index.md) GIN accélère `@>`. Pour un seul champ souvent filtré, un index d'expression est plus léger.

### La règle de conception
Une donnée qui sert à filtrer, joindre ou contraindre mérite une vraie colonne. Le reste peut aller dans le JSON.

## Exemple déroulé
```sql
CREATE TABLE notifications (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    recipient_id BIGINT NOT NULL REFERENCES auth_accounts(id) ON DELETE CASCADE,
    actor_id BIGINT REFERENCES auth_accounts(id) ON DELETE CASCADE,
    type TEXT NOT NULL,
    payload JSONB NOT NULL DEFAULT '{}',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    read_at TIMESTAMPTZ
);
```

`recipient_id` et `read_at` servent aux requêtes : ce sont des colonnes. `actor_id` a besoin d'une [clé étrangère](contraintes.md). `payload` contient `{"conversation_id": 42}` pour un message.

## Depuis cpp-pg
On passe le JSON en texte, avec un cast :

```cpp
db.query("INSERT INTO notifications (recipient_id, type, payload) "
         "VALUES ($1, $2, $3::jsonb)", id, "match", payload.dump());
```

`nlohmann::json::dump()` produit le texte. Le cast garantit une analyse par PostgreSQL : un JSON invalide donne une erreur `22P02`.

## Erreurs fréquentes
- Tout mettre dans le JSON : plus de [contraintes](contraintes.md), plus de types.
- Oublier que `->>` renvoie du texte : `'10' < '9'`. Il faut caster.
- Afficher le contenu sans [échappement](../web/xss-echappement.md) : voir la XSS.

## À retenir
JSONB sert aux données variables et secondaires. Les données structurantes restent en colonnes.

## Exercice
Pourquoi `read_at` n'est-il pas dans `payload` ?

## Corrigé
Il sert à filtrer les non lues et à compter, à chaque chargement de page. En colonne, il est typé et indexable par l'[index](index.md) `(recipient_id, read_at, id DESC)`. Dans le JSON, il faudrait un index spécial et une conversion.

## Voir aussi

- [Notifications stockées et compteurs](../temps-reel/notifications-stockees.md)
- [Les index](index.md)
