# La couche services
> La logique d'un cas d'usage vit dans un service testable, pas dans le handler HTTP.

## Mise en situation
`LikeRoutes.cpp` décide aujourd'hui de créer la conversation. Demain, il devra vérifier la photo, le blocage, écrire la notification, recalculer la popularité. Le handler deviendra un long bloc, impossible à tester sans HTTP, et la même logique sera recopiée pour le [WebSocket](../temps-reel/websocket.md).

## La théorie
### Les couches
```text
HTTP       : lit la requête, écrit la réponse
Service    : applique les règles, gère la transaction
Repository : écrit le SQL
Domaine    : règles pures, sans entrée-sortie
```

Chaque couche ne parle qu'à celle du dessous, et n'a qu'une raison de changer. Une évolution de l'API touche le handler, une évolution du schéma touche le repository.

### Le repository
Il traduit des opérations métier en SQL. Il reçoit un `pg::Executor &`, et il ne décide pas des [transactions](../sql/transactions.md) : un repository ne sait pas à quoi il participe.

### Le service
Il orchestre : c'est lui qui sait ce qui doit réussir ensemble. Il ouvre la [transaction](../sql/transactions.md), construit les repositories sur elle, et décide de publier après le commit.

### Pourquoi le service ne parle pas HTTP
Il renvoie un Result, jamais un statut. Le même service peut ainsi servir une route, un message [WebSocket](../temps-reel/websocket.md), un test ou un générateur de profils.

## Exemple déroulé
Avant :

```cpp
CROW_ROUTE(app, "/api/profiles/<int>/like")([&](const crow::request &request, int target) {
    // authentification, validation, SQL, conversation, JSON : tout ici
});
```

Après :

```cpp
// Handler
[&](const crow::request &request, int target) {
    auto &ctx = app.get_context<RequireAuth>(request);
    auto result = matchService.like(ctx.accountId, target);
    return result ? toJson(*result) : httpError(result.error());
};

// Service
core::Result<LikeResult> MatchService::like(std::int64_t actor, std::int64_t target) {
    auto tx = _pool.begin();
    if (!tx) return std::unexpected(tx.error());
    ProfileRepository profiles(*tx);
    if (!profiles.hasPrimaryPhoto(actor))
        return core::fail(core::ErrorCode::Forbidden, "profile photo required");
    // verrou, like, match, conversation
    if (auto done = tx->commit(); !done)
        return std::unexpected(done.error());
    // publication après le commit
}
```

Le handler fait trois lignes, et la règle de la photo se teste sans serveur.

## Jusqu'où aller
Une couche qui ne fait que transmettre est du bruit. Pour une lecture simple, le handler peut appeler le repository. Commence par les likes, là où la logique existe vraiment.

## Erreurs fréquentes
- Inclure `crow.h` dans un service.
- Ouvrir une [transaction](../sql/transactions.md) dans un repository.
- Mettre du SQL dans le service.

## À retenir
Le handler traduit, le service décide, le repository exécute.

## Exercice
Pourquoi `MatchService` ne doit-il pas renvoyer un statut HTTP ?

## Corrigé
Il serait lié au transport. Le même cas d'usage peut venir d'un [WebSocket](../temps-reel/websocket.md) ou d'un test. Il renvoie une erreur métier, et l'[adaptateur](lib-coeur-adaptateur.md) HTTP la traduit à un seul endroit.

## Voir aussi

- [Result et std::expected](../cpp/result-expected.md)
- [Interfaces et injection de dépendances](../cpp/interfaces-injection.md)
- [Les événements de domaine](evenements.md)
