# Fan-out et Pub/Sub
> Un événement est livré à tous ceux qui sont abonnés à son sujet.

## Mise en situation
Un match concerne deux personnes, et chacune peut avoir plusieurs onglets. Le code qui crée le match ne doit pas connaître les connexions. Il dit : « prévenir le compte 12 ». Quelqu'un d'autre sait où est ce compte.

### Un match, trois onglets
Alice et Bob viennent de matcher. Alice a deux onglets ouverts, Bob un seul, sur son téléphone. Le service ne s'occupe de rien de tout ça.

```tableau
titre: Une publication, plusieurs livraisons
colonnes: Service | Hub | Onglet 1 | Onglet 2 | Téléphone
étape: Le service dit seulement à qui s'adresse l'événement.
Service -> Hub: publish(account:7, match)
étape: Le hub copie la liste des abonnés sous verrou, puis relâche le verrou avant d'envoyer.
Hub: 2 sessions
étape: Chaque onglet d'Alice reçoit sa copie.
Hub -> Onglet 1: match
Hub -> Onglet 2: match
étape: Même chose pour Bob, sur son sujet.
Service -> Hub: publish(account:12, match)
Hub -> Téléphone: match
étape: Le service n'a jamais su combien de connexions existaient. Ajouter un onglet ne change rien à son code.
+ = l'éditeur ne connaît pas les abonnés
```

## La théorie
### Publier et s'abonner
- **Sujet (topic)** : un nom, comme `account:12`.
- **Abonné** : une session.
- **Éditeur** : le code qui publie.

L'éditeur ne connaît pas les abonnés. Ce découplage permet d'ajouter un onglet ou une fonctionnalité sans toucher à l'éditeur.

```cpp
hub.subscribe("account:12", session);
hub.publish("account:12", Envelope{"notification", data});
```

### Le fan-out
Une publication donne N livraisons : on recopie le message vers chaque abonné. Le coût croît avec le nombre d'abonnés, d'où l'envoi hors verrou, voir la [contention](../cpp/mutex-contention.md).

### Le client lent
Un client sur un mauvais réseau ne lit pas vite. Ses messages s'accumulent en mémoire. Sans limite, il peut consommer toute la RAM du serveur. La règle : une file bornée par session, et si elle déborde, on coupe cette session. Le client se reconnectera et rechargera son état.

### Plusieurs instances
Avec deux backends, Alice peut être sur le premier, et Bob sur le second.

```text
Backend 1 : publish(account:12) -> Redis Pub/Sub -> Backend 2 -> session de Bob
```

`IBroker` isole ce choix : `InMemoryBroker` aujourd'hui, `RedisBroker` plus tard, sans toucher au `Hub`.

### Pub/Sub ne stocke rien
[Redis](../infra/redis.md) Pub/Sub livre aux abonnés présents, et oublie. Un absent perd le message. C'est acceptable, car la vérité est dans PostgreSQL.

## Exemple déroulé : le test du client lent
```cpp
TEST(HubTest, SlowSessionDoesNotBlockOthers) {
    auto lente = std::make_shared<FakeSession>(/*bloquante=*/true);
    auto rapide = std::make_shared<FakeSession>();
    hub.subscribe("a", lente);
    hub.subscribe("a", rapide);
    hub.publish("a", message);
    EXPECT_EQ(rapide->received().size(), 1);
}
```

Ce test est possible car `Hub` ne connaît pas Crow, voir la séparation cœur et [adaptateur](../archi/lib-coeur-adaptateur.md).

## Exemple pas à pas : le client lent
Un client sur un mauvais réseau ne doit pas pouvoir faire tomber le serveur ou retarder les autres. Regarde la file de sa session, bornée à 3 messages pour l'exemple.

```tableau
titre: Une file bornée à 3 messages
colonnes: Hub | File de Bob | Réseau de Bob
étape: Le réseau de Bob ne suit plus. Les messages restent dans la file.
Hub -> File de Bob: message 1
Hub -> File de Bob: message 2
Hub -> File de Bob: message 3
étape: Le message suivant déborde. Attendre bloquerait tout le monde, et garder grossirait la mémoire sans limite.
! Hub -> File de Bob: message 4
étape: La session est fermée. Seul Bob est touché.
! File de Bob: session coupée
étape: Son navigateur se reconnecte et recharge l'état depuis PostgreSQL : rien n'est perdu, car la vérité est en base.
+ Réseau de Bob -> Hub: reconnexion
```

## Erreurs fréquentes
- Publier avant le commit.
- Traiter le push comme une livraison garantie.
- Un topic trop large : un sujet global réveille tout le monde.

## À retenir
L'éditeur annonce, le hub distribue, et la base garde. Aucun des trois ne dépend des deux autres.

## Exercice
Pourquoi [Redis](../infra/redis.md) Pub/Sub ne peut-il pas remplacer la table `notifications` ?

## Corrigé
Il ne stocke rien : un utilisateur hors ligne perdrait la notification. La table garde l'historique et permet le compteur de non-lus.

## Voir aussi

- [WebSocket](websocket.md)
- [Mutex et contention de verrou](../cpp/mutex-contention.md)
- [Redis](../infra/redis.md)
- [L'outbox et publier après le commit](outbox.md)
