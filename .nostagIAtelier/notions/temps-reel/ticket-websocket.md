# Le ticket WebSocket
> Un jeton court, à usage unique, remplace le [JWT](../auth/jwt.md) dans l'URL.

## Mise en situation
Le navigateur ne peut pas ajouter d'en-tête à un [WebSocket](websocket.md). Le front écrit donc :

```text
wss://matcha.local/api/messaging/ws?access_token=eyJhbGci...
```

Cette URL est écrite dans les logs d'accès de [Nginx](../infra/nginx.md). Pendant 15 minutes, quiconque lit ces logs peut se faire passer pour l'utilisateur. C'est le point 4.12 de l'audit, et `MessagingWebSocket.cpp:32` commente d'ailleurs ce compromis.

## La théorie
### L'idée
Séparer l'authentification de l'ouverture. On obtient le droit par une requête HTTP normale, puis on l'utilise une seule fois dans l'URL.

### Le flux
```text
1. POST /api/ws-ticket  (Authorization: Bearer ...)
2. Le serveur crée un ticket : 32 octets aléatoires, compte 42, expire dans 30 s
3. { "ticket": "ab12..." }
4. new WebSocket("wss://.../api/ws?ticket=ab12...")
5. onaccept : le serveur consomme le ticket, et obtient le compte
```

Le ticket reste un [token opaque](../auth/tokens-opaques.md). Même lu dans un log, il a expiré ou a déjà servi.

### Le stockage
Avec une seule instance, une table en mémoire suffit, protégée par un [mutex](../cpp/mutex-contention.md), avec nettoyage des expirés. Avec plusieurs instances, [Redis](../infra/redis.md) et sa commande `GETDEL` permettent de consommer atomiquement.

### L'alternative
Ouvrir le [WebSocket](websocket.md) sans authentification, puis envoyer le [JWT](../auth/jwt.md) dans le premier message. Ça fonctionne, mais le serveur doit gérer des connexions anonymes, et les couper si elles ne s'identifient pas.

## Exemple déroulé
```cpp
std::optional<AccountId> TicketStore::consume(const std::string &ticket) {
    std::lock_guard lock(_mutex);
    const auto it = _tickets.find(ticket);
    if (it == _tickets.end() || it->second.expiresAt < _clock.now())
        return std::nullopt;
    const AccountId account = it->second.account;
    _tickets.erase(it);
    return account;
}
```

Lecture et suppression se font sous le même verrou : deux connexions avec le même ticket ne peuvent pas réussir toutes les deux. `_clock` est une horloge injectée, ce qui permet de tester l'expiration.

## Erreurs fréquentes
- Durée trop longue.
- Ticket réutilisable.
- Oublier de nettoyer les tickets jamais utilisés : fuite mémoire lente.
- Laisser `/api/messaging/ws?access_token=` actif après la [migration](../sql/migrations.md).

## À retenir
Un [secret](../infra/secrets-fichiers.md) dans une URL finit toujours quelque part. S'il doit y passer, il doit être à usage unique et très court.

## Exercice
Pourquoi 30 secondes, et pas 1 seconde ni 10 minutes ?

## Corrigé
Le ticket doit survivre au délai entre sa réception et l'ouverture du [WebSocket](websocket.md), y compris sur un réseau lent : une seconde serait trop courte. Il n'y a aucune raison de lui laisser plusieurs minutes : plus il dure, plus un ticket volé mais non consommé serait exploitable.

## Voir aussi

- [WebSocket](websocket.md)
- [Les tokens opaques à usage unique](../auth/tokens-opaques.md)
- [JWT : les jetons d'accès](../auth/jwt.md)
