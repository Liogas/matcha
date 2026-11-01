# L'outbox et publier après le commit
> Ne jamais annoncer une action que la base peut encore annuler.

## Mise en situation
```cpp
hub.publish("account:7", "Vous avez un match !");
auto c = ensureConversation();   // échoue
return std::unexpected(c.error()); // ROLLBACK
```

L'utilisateur voit un match qui n'existe pas. Inversons :

```cpp
tx->commit();
hub.publish(...);   // le serveur meurt ici
```

Le match existe, et la notification est perdue. Il y a deux systèmes, la base et le [WebSocket](websocket.md), et aucune [transaction](../sql/transactions.md) ne couvre les deux.

### Trois ordres possibles, trois pannes
Il n'existe aucun ordre sans risque entre deux systèmes. On choisit donc la panne la moins grave.

```tableau
titre: Publier avant ou après le commit
colonnes: Service | PostgreSQL | WebSocket
étape: Avant le commit : la notification part tout de suite.
Service -> WebSocket: match !
étape: Puis la transaction échoue. L'utilisateur a été prévenu d'un match qui n'existe pas.
! Service -> PostgreSQL: ROLLBACK
! = notification fausse : inacceptable
étape: Après le commit : la base est sûre d'abord.
Service -> PostgreSQL: COMMIT
étape: Si le serveur meurt ici, la notification en direct est perdue, mais elle est stockée en base.
Service: panne éventuelle
+ = au pire, visible au prochain chargement
```

## La théorie
### Le problème de la double écriture
Dès qu'une action touche deux systèmes, l'un peut réussir et l'autre échouer. On ne peut pas les rendre atomiques, sauf avec un protocole lourd comme le two-phase commit. Il faut donc choisir un ordre, et un moyen de rattraper.

### Publier après le commit
La règle de base : on valide, puis on publie. Le pire cas est une notification perdue, jamais fausse. Si la notification est stockée dans la même [transaction](../sql/transactions.md) que le like, elle n'est même pas perdue : le push n'est qu'un signal, et l'utilisateur la verra au prochain chargement.

Pour matcha, cette solution suffit.

### Le pattern outbox
Pour une garantie plus forte, l'événement est écrit dans la même [transaction](../sql/transactions.md) :

```sql
BEGIN;
INSERT INTO profile_matches ...;
INSERT INTO outbox (type, payload) VALUES ('match', '{...}');
COMMIT;
```

Un worker lit ensuite la table :

```sql
SELECT id, type, payload FROM outbox
WHERE processed_at IS NULL
ORDER BY id LIMIT 100
FOR UPDATE SKIP LOCKED;
```

Il publie, puis marque les lignes. L'événement existe si et seulement si l'action existe.

`SKIP LOCKED` permet à plusieurs workers de travailler sans se gêner : chacun saute les lignes prises par un autre.

### Au moins une fois
Si le worker meurt entre la publication et le marquage, il republie après redémarrage. La livraison est au moins une fois, jamais exactement une fois. Le consommateur doit tolérer un doublon, par exemple avec un identifiant d'événement.

## Exemple déroulé : les emails
Aujourd'hui, l'inscription crée le compte, puis envoie l'email. Si le SMTP échoue, la route répond `503`, alors que le compte existe. Avec une `email_outbox` :

```text
1. Transaction : compte + ligne email_outbox
2. Réponse immédiate au client
3. Le worker envoie, réessaie avec un délai croissant
```

La requête ne dépend plus du SMTP, ce qui règle aussi l'[énumération](../auth/enumeration-temps-constant.md) par le temps.

## Exemple pas à pas : le worker d'outbox
Si tu mets en place l'outbox, suis une ligne de sa création à son traitement, avec une panne au pire moment.

```tableau
titre: Une ligne d'outbox
colonnes: Service | Table outbox | Worker | Hub
étape: L'événement est écrit dans la transaction du match : il existe si et seulement si le match existe.
Service -> Table outbox: INSERT, COMMIT
étape: Le worker réserve des lignes. `SKIP LOCKED` évite que deux workers prennent les mêmes.
Worker -> Table outbox: SELECT ... SKIP LOCKED
étape: Il publie.
Worker -> Hub: publish
étape: Il meurt avant de marquer la ligne. Au redémarrage, elle est toujours à traiter.
! Worker: panne
Worker -> Hub: publish une deuxième fois
étape: Le message arrive deux fois. L'identifiant de l'événement permet au client d'ignorer le doublon.
+ = au moins une fois, doublons tolérés
```

## Erreurs fréquentes
- Publier dans la [transaction](../sql/transactions.md).
- Oublier qu'un worker peut rejouer.
- Laisser grandir l'outbox sans purge.

## À retenir
Quand deux systèmes sont concernés, écris l'intention dans la base, en même temps que l'action. Le reste peut être rejoué.

## Exercice
Pourquoi un push perdu est-il acceptable pour matcha ?

## Corrigé
La notification est stockée dans la base dans la même [transaction](../sql/transactions.md) que l'action. Le [WebSocket](websocket.md) n'est qu'un moyen de la montrer plus vite. Si le push est perdu, elle apparaît au prochain chargement, et le compteur reste juste.

## Voir aussi

- [Les transactions et ACID](../sql/transactions.md)
- [Notifications stockées et compteurs](notifications-stockees.md)
- [Les événements de domaine](../archi/evenements.md)
