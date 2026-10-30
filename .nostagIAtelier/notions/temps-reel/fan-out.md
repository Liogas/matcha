# Fan-out et Pub/Sub
> Un événement est livré à tous ceux qui sont abonnés à son sujet.

## Mise en situation
Un match concerne deux personnes, et chacune peut avoir plusieurs onglets. Le code qui crée le match ne doit pas connaître les connexions. Il dit : « prévenir le compte 12 ». Quelqu'un d'autre sait où est ce compte.

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
