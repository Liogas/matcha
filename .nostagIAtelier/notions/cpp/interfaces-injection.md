# Interfaces et injection de dépendances
> On programme contre un contrat abstrait, et l'objet concret est fourni de l'extérieur.

## Mise en situation
Tu veux vérifier qu'un utilisateur bloqué ne reçoit aucune notification. Si [`NotificationDispatcher`](../temps-reel/notifications-stockees.md) fabrique lui-même sa connexion et son [WebSocket](../temps-reel/websocket.md), ton test a besoin d'une base, d'un serveur et d'un vrai client. Il sera lent, fragile et difficile à écrire. Le code est soudé à ses dépendances.

Le même problème apparaît pour les [repositories](../archi/couche-services.md) : si `LikeRepository` possède sa connexion, il ne peut pas participer à la [transaction](../sql/transactions.md) du service.

## La théorie
### Une interface en C++
C++ n'a pas de mot-clé `interface`. On utilise une classe dont les méthodes sont virtuelles pures (`= 0`) :

```cpp
class INotificationStore {
public:
    virtual ~INotificationStore() = default;
    virtual core::Result<void> save(const Notification &n) = 0;
};
```

Elle ne peut pas être instanciée. Elle définit un contrat : tout ce qui en hérite sait faire `save`.

### Le polymorphisme
```cpp
class PgNotificationStore : public INotificationStore { ... };
class FakeNotificationStore : public INotificationStore { ... };

void envoyer(INotificationStore &store) { store.save(n); }
```

`envoyer` ne connaît pas le type réel. Chaque objet porte un pointeur vers sa vtable, une table de pointeurs de fonctions. L'appel lit l'adresse dans cette table. Le coût est d'une indirection, négligeable face à une requête SQL.

### L'injection de dépendances
Au lieu de créer ce dont il a besoin, un objet le reçoit :

```cpp
// Soudé
NotificationDispatcher() : _store(new PgNotificationStore(...)) {}

// Injecté
explicit NotificationDispatcher(INotificationStore &store) : _store(store) {}
```

Le code n'a pas changé de logique, mais la décision de quelle implémentation utiliser est sortie de la classe. C'est l'inversion de dépendance : le code métier dépend d'une abstraction, et non du contraire.

### Le composition root
Il faut bien créer les objets concrets quelque part. Ce sera en un seul endroit, le plus haut possible : `main.cpp`. On y lit la configuration, crée le [pool](../sql/pool-connexions.md) et assemble les services. L'audit le relève déjà comme un point fort.

### Le destructeur
Détruire un objet à travers un pointeur de base exige un destructeur virtuel, sinon le destructeur de la fille n'est pas appelé. `pg::Executor` fait un autre choix : son destructeur est `protected` et non virtuel. Détruire via `Executor *` devient une erreur de compilation, ce qui évite la vtable du destructeur.

## Exemple déroulé : pg::Executor
```cpp
class LikeRepository {
public:
    explicit LikeRepository(pg::Executor &database) : _database(database) {}
    core::Result<LikeResult> like(std::int64_t a, std::int64_t b);
private:
    pg::Executor &_database;
};
```

Trois usages, sans changer une ligne du [repository](../archi/couche-services.md) :

```cpp
LikeRepository lecture(*pool);    // chaque requête emprunte une connexion
LikeRepository dansTx(*tx);       // tout passe dans la transaction
LikeRepository enTest(fixtureTx); // annulé à la fin du test
```

Un repository n'est qu'une référence : en construire un par [transaction](../sql/transactions.md) ne coûte rien.

## Dans matcha
- `IImageStorage` et `LocalImageStorage` ;
- `IPasswordHasher` et `PasswordHasher` ;
- `JwksProvider` avec `HttpJwksProvider`, et bientôt [`StaticJwksProvider`](../auth/jwks.md) ;
- `IClock`, pour contrôler le temps dans les tests ;
- `ILogger`, injecté dans le [pool](../sql/pool-connexions.md).

## Quand ne pas créer d'interface
Une interface a un coût : un fichier de plus, et une navigation moins directe. Elle se justifie s'il existe deux implémentations réelles, ou un besoin de [faux objet](../tests/faux-objets.md) en test. Une fonction pure comme `haversineKm` n'en a pas besoin.

Garde-les petites : un faux avec 15 méthodes à écrire est un signe que l'interface en fait trop.

## Erreurs fréquentes
- Injecter par référence un objet qui meurt avant son utilisateur, voir la [durée de vie](duree-de-vie-objets.md).
- Oublier le destructeur virtuel.
- Faire dépendre une interface de Crow : elle n'est plus générique.

## À retenir
Le code métier dit ce dont il a besoin, et c'est `main.cpp` ou le test qui décide avec quoi le brancher.

## Exercice
Comment tester, sans attendre 30 secondes, qu'un [ticket WebSocket](../temps-reel/ticket-websocket.md) expire ?

## Corrigé
Le code ne doit pas appeler l'horloge système directement, mais un `IClock &` injecté. En test, on fournit une `FakeClock` dont la valeur est réglée à la main : on crée le ticket, on avance l'horloge de 31 secondes, et on vérifie le refus. Le test dure quelques microsecondes.

## Voir aussi

- [Les faux objets de test](../tests/faux-objets.md)
- [Templates variadiques et fold expressions](templates-variadiques.md)
- [La couche services](../archi/couche-services.md)
