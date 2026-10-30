# Une bibliothèque : cœur et adaptateur
> Le cœur générique ignore le framework, et un petit adaptateur fait le lien avec Crow.

## Mise en situation
Le hub de messagerie utilise directement `crow::websocket::connection`. On ne peut le tester qu'avec un vrai serveur, et il ne pourra pas servir ailleurs. Dès qu'une bibliothèque inclut `crow.h`, elle hérite de tout Crow.

## La théorie
### L'architecture hexagonale, en simple
Le cœur contient la logique et définit ce dont il a besoin par des [interfaces](../cpp/interfaces-injection.md). Les adaptateurs branchent le monde extérieur sur ces interfaces.

```text
cpp-realtime          : Hub, ISession, PresenceTracker   (aucun Crow)
cpp-realtime-crow     : CrowSession implémente ISession
```

### Le sens des dépendances
```text
cpp-core  <-  cpp-pg  <-  cpp-notify  <-  backend
                                ^
              cpp-realtime  ----+
```

Les flèches vont vers le plus stable. `cpp-core` ne dépend de rien. Le backend dépend des libs, jamais l'inverse. Aucun cycle.

### La règle de découpage
Sans le mot Matcha ni like, le code va dans une lib. Avec, il reste dans `src/`. La messagerie est générique, la règle « il faut un match pour discuter » ne l'est pas : elle entre par une `IConversationPolicy`.

### La définition de terminé
Une lib est finie quand elle passe [`-Werror`](../build/warnings.md), `sanitize`, le [package-consumer](../build/package-cmake.md), a un README et un [CHANGELOG](../build/semver.md), et remplace du vrai code dans le backend.

## Exemple déroulé
```cpp
// cpp-realtime
class ISession {
public:
    virtual ~ISession() = default;
    virtual void send(const std::string &payload) = 0;
};

// cpp-realtime-crow
class CrowSession : public ISession {
public:
    explicit CrowSession(crow::websocket::connection &c) : _connection(c) {}
    void send(const std::string &payload) override { _connection.send_text(payload); }
private:
    crow::websocket::connection &_connection;
};

// tests
class FakeSession : public ISession {
public:
    void send(const std::string &payload) override { received.push_back(payload); }
    std::vector<std::string> received;
};
```

Le `Hub` fonctionne avec les deux, sans le savoir.

## Le risque de sur-ingénierie
L'audit le rappelle : mieux vaut 5 bibliothèques solides que 12 squelettes. N'extrais une lib que si elle sert vraiment.

## Erreurs fréquentes
- Une lib générique qui inclut un type de Crow dans un en-tête public.
- Une dépendance circulaire entre libs.
- Extraire trop tôt.

## À retenir
Le cœur ne connaît pas le framework : c'est ce qui le rend testable, et réutilisable.

## Exercice
Comment tester qu'un client lent ne bloque pas les autres, sans serveur ?

## Corrigé
On crée deux [`FakeSession`](../tests/faux-objets.md), dont une dont `send` bloque, on les abonne au `Hub`, et on publie. Le test vérifie que la session rapide a reçu son message. C'est possible car le `Hub` ne dépend que d'`ISession`.

## Voir aussi

- [Interfaces et injection de dépendances](../cpp/interfaces-injection.md)
- [Rendre une lib installable](../build/package-cmake.md)
- [Les faux objets de test](../tests/faux-objets.md)
