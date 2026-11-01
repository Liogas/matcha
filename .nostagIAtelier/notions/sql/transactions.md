# Les transactions et ACID
> Une transaction regroupe plusieurs requêtes en un tout : soit tout est validé, soit rien n'existe.

## Mise en situation
Alice like Bob, qui l'avait déjà liké. Le serveur doit faire trois écritures :

```text
1. INSERT du like
2. INSERT du match
3. INSERT de la conversation
```

```tableau
titre: Le like mutuel, sans transaction
colonnes: Serveur | PostgreSQL
étape: Alice like Bob. Le like est **validé tout de suite**.
Serveur -> PostgreSQL: INSERT like
+ PostgreSQL: enregistré
étape: Le match aussi : les deux personnes vont voir « vous êtes connectés ».
Serveur -> PostgreSQL: INSERT match
+ PostgreSQL: enregistré
étape: Le serveur plante ici. La troisième écriture n'est jamais envoyée.
! Serveur: plantage
étape: Au redémarrage, la base contient un match **sans conversation**. Rien ne peut l'annuler : les deux premières écritures sont déjà validées.
! = like : oui   match : oui   conversation : non
```

Sans transaction, chaque écriture est validée séparément. Si le serveur plante entre 2 et 3, il reste un match sans conversation, et les deux personnes voient « vous êtes connectés » sans pouvoir se parler. C'est exactement ce que répare la boucle de `main.cpp:107` au démarrage. Ce code soigne un symptôme : la transaction supprime la cause.

### Ce que le mot « atomique » change pour toi
Pense à un virement : débiter un compte, créditer l'autre. Personne n'accepterait qu'une panne laisse l'argent disparu. Ton like est de la même nature, avec trois écritures qui n'ont de sens qu'ensemble. La transaction n'est pas une optimisation : c'est la façon de dire à la base « ces écritures sont une seule opération ».

## La théorie
### Le contrat ACID
- **Atomicité** : tout ou rien. Un échec avant `COMMIT` annule toutes les écritures.
- **Cohérence** : à la fin, toutes les [contraintes](contraintes.md) sont respectées.
- **[Isolation](isolation.md)** : les transactions parallèles ne se gênent pas, à un degré choisi par le niveau d'isolation.
- **Durabilité** : une fois `COMMIT` confirmé, les données survivent à une panne.

### Comment PostgreSQL annule sans effacer
PostgreSQL utilise MVCC, le contrôle de concurrence multiversion. Quand une ligne est modifiée, il n'écrase pas l'ancienne : il crée une nouvelle version. Chaque version porte l'identifiant de la transaction qui l'a créée. Une lecture ne voit que les versions de transactions validées.

Conséquences :

- `ROLLBACK` est rapide : il suffit de considérer la transaction comme annulée ;
- les lecteurs n'attendent pas les écrivains ;
- de vieilles versions restent, et `VACUUM` les nettoie plus tard.

### Comment PostgreSQL garantit la durabilité
Avant de confirmer `COMMIT`, il écrit dans un journal sur disque, le WAL (write-ahead log), puis force l'écriture. Après une panne, il rejoue ce journal. C'est pour cela que `COMMIT` est l'opération coûteuse : faire 1000 petites transactions est bien plus lent que 1000 écritures dans une seule.

### Une transaction appartient à une connexion
C'est le point le plus important pour ton code. PostgreSQL ne connaît ni tes threads ni tes fonctions. `BEGIN` s'applique à la connexion : tout ce qui passe ensuite sur elle fait partie de la transaction.

```tableau
titre: Ancien code : une seule connexion pour tous les threads
colonnes: Thread A | Connexion | Thread B
étape: A traite le like d'Alice. Il ouvre une transaction **sur la connexion partagée**.
Thread A -> Connexion: BEGIN
étape: B traite une tout autre personne. Il ne demande aucune transaction, mais la connexion est déjà dedans.
! Thread B -> Connexion: UPDATE profil
étape: A continue son travail. Il ne sait pas que B est passé.
Thread A -> Connexion: INSERT like
étape: A rencontre une erreur et annule.
Thread A -> Connexion: ROLLBACK
étape: L'`UPDATE` de B disparaît aussi. B a répondu « profil enregistré » à son utilisateur, et c'est faux.
! Connexion: tout est annulé
```

C'est le défaut de l'ancien `Database` relevé par l'audit 4.5. `pg::Transaction` y répond en gardant une connexion pour lui seul, du `BEGIN` au `COMMIT`.

### L'état d'échec
Après une erreur, la transaction est dite aborted. Toute requête suivante échoue avec `25P02`, jusqu'au `ROLLBACK`. La [SAVEPOINT](savepoint.md) permet d'y échapper quand c'est nécessaire.

## Exemple complet
```cpp
core::Result<void> MatchService::like(std::int64_t liker, std::int64_t liked) {
    auto tx = _pool.begin();
    if (!tx)
        return std::unexpected(tx.error());

    LikeRepository likes(*tx);
    MessagingRepository messaging(*tx);

    auto result = likes.like(liker, liked);
    if (!result)
        return std::unexpected(result.error()); // ROLLBACK par le destructeur
    if (result->matched)
        if (auto c = messaging.ensureDirectConversation(liker, liked); !c)
            return std::unexpected(c.error());  // ROLLBACK aussi

    return tx->commit();
}
```

```tableau
titre: Le même like, avec une transaction
colonnes: MatchService | pg::Transaction | PostgreSQL
étape: `_pool.begin()` réserve une connexion pour ce like, et pour lui seul.
MatchService -> pg::Transaction: _pool.begin()
pg::Transaction -> PostgreSQL: BEGIN
étape: Les écritures sont faites, mais **invisibles pour les autres** tant que rien n'est validé.
pg::Transaction -> PostgreSQL: INSERT like, match
étape: La création de la conversation échoue. La fonction fait `return` avec une erreur.
! MatchService: return erreur
étape: `tx` sort de portée sans `commit()` : son destructeur annule. Aucun oubli possible.
pg::Transaction -> PostgreSQL: ROLLBACK
étape: La base est exactement comme avant le like. Pas de match orphelin.
+ = like : non   match : non   conversation : non
```

À chaque `return` prématuré, `tx` est détruit et le [RAII](../cpp/raii.md) annule tout. Il n'y a plus de chemin qui laisse une écriture à moitié faite. Les [repositories](../archi/couche-services.md) sont construits sur la transaction, car ils reçoivent un Executor.

## Ce qu'une transaction ne protège pas
Une transaction rend tes écritures atomiques entre elles. Elle n'empêche pas deux transactions simultanées de prendre de mauvaises décisions. Le like croisé en est l'exemple : voir l'[isolation](isolation.md) et les [verrous consultatifs](verrous-consultatifs.md).

Elle ne couvre pas non plus le monde extérieur. Un email ou un message [WebSocket](../temps-reel/websocket.md) envoyé avant `COMMIT` ne peut pas être annulé : d'où la règle de publier après le commit.

## Exemple pas à pas : ce que voient deux connexions
Ouvre deux terminaux `psql` et reproduis ceci. C'est la meilleure façon de sentir ce qu'est une écriture non validée.

```tableau
titre: Deux terminaux psql
colonnes: Terminal 1 | Terminal 2
étape: Le terminal 1 ouvre une transaction et insère.
Terminal 1: BEGIN
Terminal 1: INSERT tag 'rando'
étape: Il voit sa propre ligne : il est dans sa transaction.
Terminal 1: SELECT : 1 ligne
étape: Le terminal 2 ne voit rien. La ligne n'existe pas encore pour le reste du monde.
Terminal 2: SELECT : 0 ligne
étape: Après le `COMMIT`, elle apparaît pour tout le monde, d'un seul coup.
Terminal 1: COMMIT
+ Terminal 2: SELECT : 1 ligne
```

## Erreurs fréquentes
- **Garder la transaction ouverte pendant un appel réseau** : la connexion du [pool](pool-connexions.md) est bloquée, et les verrous restent posés.
- **Ignorer le résultat de `commit()`** : il peut échouer.
- **Oublier une requête hors transaction** : si un [repository](../archi/couche-services.md) utilise le pool au lieu de la transaction, il prend une autre connexion, et ne voit pas les écritures non validées.
- **Mélanger le like et un calcul long** : plus une transaction dure, plus elle gêne les autres.

## À retenir
Une transaction est une unité de travail attachée à une connexion. Le service définit ce qui doit réussir ensemble, et le `ROLLBACK` automatique du destructeur s'occupe du reste.

## Exercice
Dans `MatchService::like`, un collègue déplace `hub.publish("match")` juste avant `tx->commit()`. Décris deux scénarios où c'est un problème.

## Corrigé
Premier scénario : le `COMMIT` échoue. L'utilisateur reçoit une notification de match pour un match qui n'existe pas. Second scénario : le client reçoit le message et interroge aussitôt `/api/matches`. La transaction n'étant pas encore validée, le match n'apparaît pas. Il faut publier après un `commit()` réussi.

## Voir aussi

- [Les niveaux d'isolation](isolation.md)
- [Le pool de connexions](pool-connexions.md)
- [SAVEPOINT](savepoint.md)
- [RAII](../cpp/raii.md)
- [L'outbox et publier après le commit](../temps-reel/outbox.md)
