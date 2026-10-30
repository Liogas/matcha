# Migrations

Appliquées au démarrage du serveur par `pg::migrate` (cpp-pg), dans l'ordre de
leur numéro, en une seule transaction.

- **Nom** : `NNN_description.sql`, numéro sur 3 chiffres. Deux fichiers ne
  peuvent pas avoir le même numéro.
- **Ne jamais modifier un fichier déjà appliqué** : sa somme de contrôle
  (SHA-256) est enregistrée dans `schema_migrations`, et le serveur refuse de
  démarrer si le fichier a changé. Pour corriger, ajouter un nouveau fichier.
- **Essayer sans rien appliquer** : `matcha_backend --migrations-dry-run`
  applique les migrations en attente puis annule tout (`ROLLBACK`).
- **Pas de `BEGIN`/`COMMIT` dans un fichier** : le runner ouvre déjà la
  transaction. Donc pas non plus d'instruction interdite en transaction
  (`CREATE INDEX CONCURRENTLY`, `VACUUM`).

## Numéros absents

- **006** : n'a jamais existé dans ce dépôt (le numéro a été sauté). Aucune
  conséquence : seul l'ordre compte. Ne pas réutiliser ce numéro, il serait
  appliqué après 009 sur une base existante… et avant 007 sur une base neuve.