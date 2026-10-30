# Les événements de domaine
> Une action annonce ce qui s'est passé, et d'autres parties du code réagissent, sans se connaître.

## Mise en situation
Après un like, il faut notifier, recalculer la popularité, peut-être enregistrer une statistique. Si `MatchService` appelle tout cela, il dépend de tout. Ajouter une réaction oblige à modifier un service qui fonctionne.

## La théorie
### L'événement
Un événement est un fait passé, immuable, nommé au passé :

```cpp
struct LikeCreated { AccountId actor, target; };
struct MatchCreated { AccountId first, second; ConversationId conversation; };
struct ProfileViewed { AccountId viewer, viewed; };
```

### Publier et réagir
```text
MatchService -> publie MatchCreated
                  |-> FameUpdater
                  |-> NotificationHandler
```

Le service ne sait pas qui écoute. Ajouter un effet, c'est ajouter un abonné.

### Synchrone ou non
- **En mémoire** : les abonnés sont appelés dans la foulée, c'est simple.
- **Avec une [outbox](../temps-reel/outbox.md)** : écrit dans la [transaction](../sql/transactions.md), traité plus tard, c'est plus robuste.

### Quand publier
Toujours après le commit, ou via une [outbox](../temps-reel/outbox.md).

## Exemple déroulé
```cpp
class EventBus {
public:
    template <class E>
    void subscribe(std::function<void(const E &)> handler);
    template <class E>
    void publish(const E &event);
};
```

```cpp
bus.subscribe<MatchCreated>([&](const MatchCreated &e) {
    notifier.notifyMatch(e.first, e.second);
});
```

Pour ajouter un super-like : un cas d'usage, un événement et un abonné, sans toucher au reste. C'est un bon argument en entretien sur la maintenabilité.

## Le prix à payer
Le déroulé n'est plus visible en lisant le service. Avec un ou deux effets, un appel direct est plus simple. Ne crée un bus que lorsque les réactions se multiplient.

## Erreurs fréquentes
- Un abonné qui échoue et fait échouer l'action déjà validée.
- Une chaîne d'événements impossible à suivre.
- Un événement nommé comme un ordre, comme `SendNotification`.

## À retenir
Un événement sépare ce qui s'est produit de ce qu'on en fait.

## Exercice
Quelle différence entre `LikeCreated` et `SendNotification` ?

## Corrigé
Le premier décrit un fait, et chacun décide d'y réagir. Le second est un ordre adressé à quelqu'un : l'émetteur reste couplé à ce qu'il attend.

## Voir aussi

- [La couche services](couche-services.md)
- [L'outbox et publier après le commit](../temps-reel/outbox.md)
