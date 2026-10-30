# Les migrations
> Le schéma évolue par une suite de fichiers numérotés, appliqués une seule fois, dans l'ordre, sur chaque base.

## Mise en situation
Tu ajoutes la latitude aux profils. Sur ta machine, tu tapes `ALTER TABLE` dans `psql`, et tout marche. Le correcteur clone le dépôt et lance `docker compose up` : la colonne n'existe pas, l'application plante. Le schéma doit vivre dans le dépôt, au même titre que le code.

## La théorie
### Le principe
Chaque changement est un fichier SQL numéroté dans `backend/migrations/`. La base retient ce qui a déjà été appliqué dans une table, `schema_migrations`. Au démarrage, le runner compare les deux listes et applique ce qui manque.

```text
fichiers : 001 002 003 004 005 007 008 009
en base  : 001 002 003 004 005 007 008
à faire  : 009
```

Une base vide et une base ancienne arrivent ainsi au même schéma.

### Le déroulé dans cpp-pg
1. Ouvrir une [transaction](transactions.md).
2. Prendre le verrou `pg_advisory_xact_lock(hashtextextended('schema_migrations', 0))`.
3. Lire les versions et sommes de contrôle appliquées.
4. Vérifier les sommes des fichiers déjà appliqués.
5. Appliquer les manquants, dans l'ordre, et les enregistrer.
6. `COMMIT`.

Tout se fait en une seule transaction, grâce au DDL transactionnel de PostgreSQL : même `CREATE TABLE` peut être annulé. Si la migration 010 échoue, 009 est annulée avec elle, et la base reste dans son état de départ.

### Pourquoi un verrou
Deux instances du backend qui démarrent en même temps liraient la même liste et appliqueraient la même migration. Le [verrou consultatif](verrous-consultatifs.md) fait attendre la seconde. Quand elle obtient le verrou, son `SELECT` voit les migrations déjà appliquées, car c'est une nouvelle instruction en [`READ COMMITTED`](isolation.md).

### La somme de contrôle
Une migration appliquée ne doit plus changer. Sinon deux bases peuvent porter le même numéro sans avoir le même schéma, et personne ne le saura.

Le runner calcule un SHA-256 de chaque fichier et le stocke :

```sql
SELECT encode(sha256(convert_to($1, 'UTF8')), 'hex') AS checksum;
```

Si le fichier change plus tard, l'empreinte diffère, et le démarrage s'arrête avec un message clair. Une erreur au démarrage vaut mieux qu'une divergence silencieuse.

### L'historique ne s'écrit qu'en ajoutant
Une migration est comme un commit publié. Pour corriger une erreur, on ajoute une nouvelle migration. Ainsi la table `users` reste créée par 001, et une migration ultérieure la supprime. Le numéro 006 manquant n'a pas d'effet : l'ordre compte, pas la continuité. On le documente simplement.

### Le mode dry-run
`--dry-run` annonce ce qui serait fait, sans rien appliquer. C'est utile avant de lancer sur une base qu'on ne veut pas abîmer, ou sur une copie.

## Exemple complet
Ajouter la géolocalisation :

```sql
-- 009_add_profile_location.sql
ALTER TABLE matcha_profiles
    ADD COLUMN latitude DOUBLE PRECISION,
    ADD COLUMN longitude DOUBLE PRECISION,
    ADD CONSTRAINT matcha_profiles_latitude_range
        CHECK (latitude BETWEEN -90 AND 90),
    ADD CONSTRAINT matcha_profiles_longitude_range
        CHECK (longitude BETWEEN -180 AND 180);

CREATE INDEX matcha_profiles_location_idx
    ON matcha_profiles (latitude, longitude);
```

Au prochain démarrage, partout, le fichier s'applique une fois. Les colonnes sont nullables, car les comptes existants n'ont pas de position.

## Pourquoi `executeScript`
Un fichier contient plusieurs instructions, or [`PQexecParams`](libpq.md) n'en accepte qu'une. `Transaction::executeScript` utilise `PQexec`, qui sait en enchaîner plusieurs. Cette fonction ne prend aucun paramètre, et ne reçoit que du SQL écrit par toi, donc elle ne présente pas de risque d'[injection](../cpp/interfaces-injection.md).

## Erreurs fréquentes
- **Modifier un fichier déjà appliqué** : le runner refuse de démarrer. Écris une nouvelle migration.
- **Ajouter une colonne `NOT NULL` sans défaut** à une table remplie : échec immédiat.
- **Ajouter un [`CHECK`](contraintes.md) sans regarder les données** : la migration échoue si une ligne le viole.
- **`CREATE INDEX CONCURRENTLY`** : interdit dans une [transaction](transactions.md).

## À retenir
Une migration est une étape irréversible de l'histoire du schéma : numérotée, vérifiée par empreinte, appliquée en [transaction](transactions.md) sous verrou.

## Exercice
Tu découvres une faute dans le nom d'une colonne de `005_...sql`, déjà appliquée partout. Que fais-tu ? Que se passerait-il si tu modifiais directement le fichier ?

## Corrigé
On crée une nouvelle migration avec `ALTER TABLE ... RENAME COLUMN`. Si tu modifiais `005`, sa somme de contrôle changerait, et chaque backend refuserait de démarrer. Sur une base neuve, le fichier donnerait le bon nom, et sur les bases existantes il resterait l'ancien : c'est la divergence que ce mécanisme empêche.

## Voir aussi

- [Les verrous consultatifs](verrous-consultatifs.md)
- [Les transactions et ACID](transactions.md)
- [Les contraintes d'intégrité](contraintes.md)
