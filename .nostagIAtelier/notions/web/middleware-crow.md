# Middleware Crow et contexte de requête
> Du code exécuté autour de chaque requête, qui peut la refuser ou préparer des données.

## Mise en situation
Chaque route recommence :

```cpp
const auto current = auth_routes::authenticatedAccountId(request, authenticator);
if (!current)
    return unauthorized();
```

Oublier ces lignes sur une nouvelle route ouvre une faille. Et le mauvais code d'erreur (audit 4.8) est copié partout.

## La théorie
### Le principe
Un middleware s'insère dans le traitement de la requête :

```text
Requête -> before_handle -> handler -> after_handle -> Réponse
```

C'est un endroit unique pour ce qui concerne toutes les routes : authentification, journalisation, en-têtes.

### Dans Crow
```cpp
struct RequireAuth {
    struct context { std::int64_t accountId = 0; };
    void before_handle(crow::request &req, crow::response &res, context &ctx);
    void after_handle(crow::request &req, crow::response &res, context &ctx);
};
```

`context` est créé pour chaque requête : deux requêtes simultanées ne partagent rien. Si `before_handle` termine la réponse avec `res.end()`, le handler n'est pas appelé.

### Global ou local
Crow permet de déclarer un middleware local, puis de l'activer route par route :

```cpp
CROW_ROUTE(app, "/api/profile")
    .CROW_MIDDLEWARES(app, RequireAuth)
    ([&](const crow::request &req) {
        auto &ctx = app.get_context<RequireAuth>(req);
        return handle(ctx.accountId);
    });
```

## Exemple déroulé
```cpp
void RequireAuth::before_handle(crow::request &req, crow::response &res, context &ctx) {
    const auto token = bearerToken(req);
    const auto result = token ? authenticator.authenticate(*token) : invalid();
    if (result.status == Status::VerificationUnavailable) {
        res.code = 503; res.end(); return;
    }
    if (result.status != Status::Valid) {
        res.code = 401; res.end(); return;
    }
    ctx.accountId = parse(result.claims->subject);
}
```

Le handler n'a plus qu'à lire `ctx.accountId`. Les cas `401` et `503` sont traités une seule fois. Le [`request_id`](../infra/logs-structures.md) des logs naît au même endroit.

## Le cas du WebSocket
Le [WebSocket](../temps-reel/websocket.md) ne passe pas par cette chaîne : il a ses propres callbacks `onaccept`, voir le [ticket WebSocket](../temps-reel/ticket-websocket.md).

## Erreurs fréquentes
- Une route oubliée reste publique : prévois un test qui parcourt les routes.
- Un `before_handle` qui fait des requêtes coûteuses pour tout le monde.
- Stocker des données par requête dans une variable globale.

## À retenir
Ce qui doit s'appliquer à toutes les routes ne doit pas dépendre de la mémoire de celui qui écrit la route.

## Exercice
Pourquoi le contexte doit-il être créé par requête ?

## Corrigé
Crow traite des requêtes en parallèle. Un champ partagé serait écrasé par une autre requête : l'utilisateur A agirait au nom de B, en plus du [data race](../cpp/data-race.md).

## Voir aussi

- [HTTP : méthodes et codes de statut](http-bases.md)
- [JWT : les jetons d'accès](../auth/jwt.md)
- [La couche services](../archi/couche-services.md)
