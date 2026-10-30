# Les faux objets de test
> Un remplaçant léger d'une dépendance réelle, pour tester du code sans réseau ni base.

## Mise en situation
Tu veux vérifier qu'une notification arrive avec le bon compteur. Un vrai [WebSocket](../temps-reel/websocket.md) exigerait un serveur, un client, et de l'attente. Le test serait lent, instable, et difficile à comprendre en cas d'échec.

## La théorie
### Les doublures de test
- **Dummy** : un objet passé, jamais utilisé.
- **Stub** : renvoie une réponse prévue.
- **Fake** : une vraie implémentation simplifiée, comme un `InMemoryBroker`.
- **Spy** : enregistre ce qu'il reçoit.
- **Mock** : vérifie les appels attendus. GoogleMock en fournit.

### Ce qui les rend possibles
Une [interface](../cpp/interfaces-injection.md) et l'injection. Le code sous test ne sait pas qu'il parle à un faux.

### Le temps et l'aléa
Ce sont des dépendances cachées. Un test ne doit jamais attendre 30 secondes :

```cpp
class FakeClock : public IClock {
public:
    std::chrono::system_clock::time_point now() const override { return _now; }
    void advance(std::chrono::seconds s) { _now += s; }
private:
    std::chrono::system_clock::time_point _now{};
};
```

`cpp-auth` utilise déjà cette technique, avec des horloges injectables.

## Exemple déroulé
```cpp
TEST(DispatcherTest, PushesUnreadCount) {
    FakeNotificationStore store;
    FakeHub hub;
    AllowAllFilter filter;
    NotificationDispatcher dispatcher(store, hub, filter);

    dispatcher.dispatch(Notification{.recipient = 7, .type = "match"});

    ASSERT_EQ(hub.published.size(), 1);
    EXPECT_EQ(hub.published[0].topic, "account:7");
    EXPECT_NE(hub.published[0].payload.find("unread_count"), std::string::npos);
}
```

Le test s'exécute en quelques microsecondes, et un échec désigne directement le dispatcher.

## Vérifier l'état, pas les appels
Préfère regarder le résultat plutôt que la suite exacte d'appels internes. Un test qui exige « `save` puis `publish` dans cet ordre » casse à chaque refactoring. Sauf quand l'ordre fait partie du contrat, comme publier après le commit.

## Les limites
Un faux peut différer du vrai : un `FakeStore` n'a pas de [contraintes](../sql/contraintes.md) SQL. Garde des [tests d'intégration](pyramide.md) sur la vraie implémentation.

## Erreurs fréquentes
- Un faux trop intelligent, qui contient ses propres bugs.
- Mocker ce qui ne t'appartient pas : une API externe change sans prévenir.
- `sleep` dans un test.

## À retenir
Un faux remplace ce qui est lent ou imprévisible, pour tester la logique seule.

## Exercice
Comment tester une expiration de 30 secondes sans attendre ?

## Corrigé
Le code lit l'heure via `IClock`. Le test crée l'objet, appelle `FakeClock::advance(31s)`, puis vérifie le refus.

## Voir aussi

- [Interfaces et injection de dépendances](../cpp/interfaces-injection.md)
- [La pyramide de tests](pyramide.md)
