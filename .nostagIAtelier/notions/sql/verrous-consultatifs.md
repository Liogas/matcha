# Les verrous consultatifs
> Un verrou posé sur un nombre que tu choisis, sans rapport avec une table ou une ligne.

## Mise en situation
Pour éviter le like croisé décrit dans la fiche sur l'[isolation](isolation.md), une [transaction](transactions.md) doit attendre l'autre. Le réflexe est `SELECT ... FOR UPDATE`, qui verrouille une ligne. Mais quelle ligne ? Le like d'Alice n'existe pas encore, celui de Bob non plus. On ne peut pas verrouiller ce qui n'existe pas.

Il nous faut un verrou sur une idée : « la relation entre Alice et Bob ».

## La théorie
### Le principe
PostgreSQL tient une table de verrous nommés par des entiers de 64 bits. Tu choisis le nombre, et PostgreSQL ne lui donne aucun sens. Il garantit seulement que deux [transactions](transactions.md) ne tiennent pas le même nombre en même temps. Le mot consultatif vient de là : seul le code qui demande le verrou est concerné. Une requête qui ne le demande pas passe sans attendre.

### Les deux variantes
- `pg_advisory_xact_lock(clé)` : libéré automatiquement à la fin de la [transaction](transactions.md), par `COMMIT` ou `ROLLBACK`.
- `pg_advisory_lock(clé)` : tenu jusqu'au déverrouillage explicite ou à la fermeture de la connexion.

Avec un [pool](pool-connexions.md), la seconde est dangereuse : la connexion retourne au pool en gardant le verrou, et elle ne se ferme jamais. On utilise donc toujours la version transactionnelle.

### Fabriquer la clé
Tes identifiants sont des `BIGINT`, et une paire ne tient pas dans un seul nombre. La forme à deux clés prend deux `INTEGER` de 32 bits, trop petits. On hache donc un texte :

```sql
SELECT pg_advisory_xact_lock(hashtextextended(
    LEAST($1::BIGINT, $2::BIGINT)::TEXT || ':' ||
    GREATEST($1::BIGINT, $2::BIGINT)::TEXT, 0));
```

`hashtextextended(texte, graine)` renvoie un `BIGINT`. `LEAST` et `GREATEST` mettent la paire dans un ordre fixe. Le texte est donc le même dans les deux sens.

Une collision reste possible. Elle ne casse rien : deux paires sans rapport s'attendraient quelques millisecondes.

### Un verrou ré-entrant
Dans une même [transaction](transactions.md), redemander un verrou déjà tenu réussit sans attendre. `ensureDirectConversation` prend la même clé : dans `MatchService`, ce n'est pas un problème.

## Exemple complet
Alice = 12, Bob = 7. Les deux calculent la clé du texte `"7:12"` :

```text
Thread A : Alice like Bob           Thread B : Bob like Alice
BEGIN                               BEGIN
verrou "7:12" obtenu                verrou "7:12" ... attend
INSERT like Alice->Bob
like Bob->Alice existe ? NON
COMMIT (verrou libéré)
                                    ... verrou obtenu
                                    INSERT like Bob->Alice
                                    like Alice->Bob existe ? OUI
                                    INSERT match et conversation
                                    COMMIT
```

Résultat : exactement un match. Une paire sans rapport, Chloé et David, prend une autre clé et ne s'arrête pas. Si on avait écrit `liker || ':' || liked`, A verrouillerait `"12:7"` et B `"7:12"` : aucune attente, et le bug reviendrait.

Cette solution fonctionne parce que B reprend une nouvelle photo après l'attente, en [`READ COMMITTED`](isolation.md). Voir l'exemple de la fiche isolation.

## Autre usage : les migrations
Deux backends démarrent en même temps et voient la même [migration](migrations.md) à appliquer. Avec `hashtextextended('schema_migrations', 0)`, le second attend le premier, puis constate qu'il n'y a plus rien à faire. Voir les migrations.

## Erreurs fréquentes
- **Clé dépendant du sens** : plus aucune protection.
- **Oublier une [transaction](transactions.md) autour** : hors transaction, le verrou est libéré dès la fin de l'instruction, donc il ne protège rien.
- **Prendre deux clés dans un ordre variable** : c'est le chemin vers un [deadlock](deadlock.md).
- **Verrou global** : une clé fixe sérialiserait tous les likes du site.

## À retenir
Un verrou consultatif protège une idée métier plutôt qu'une ligne. Il est utile justement quand la ligne à protéger n'existe pas encore.

## Exercice
Tu veux empêcher qu'un utilisateur envoie deux demandes de changement d'email en même temps. Quelle clé choisir ? Faut-il `LEAST`/`GREATEST` ?

## Corrigé
Une clé basée sur le compte, avec un préfixe pour ne pas croiser d'autres usages : `hashtextextended('email_change:' || $1::TEXT, 0)`. `LEAST`/`GREATEST` ne servent pas, car il n'y a qu'un identifiant et pas de sens à normaliser.

## Voir aussi

- [Les niveaux d'isolation](isolation.md)
- [Le deadlock](deadlock.md)
- [Les migrations](migrations.md)
- [Les transactions et ACID](transactions.md)
