# WebSocket
> Une connexion gardée ouverte, où le serveur peut parler sans attendre de requête.

## Mise en situation
Alice écrit à Bob. Le sujet exige que Bob le voie en moins de 10 secondes, sur n'importe quelle page. En HTTP pur, le serveur ne peut pas parler le premier : le client doit redemander toutes les quelques secondes, ce qui gaspille des requêtes et ajoute du délai.

## La théorie
### Les alternatives
- **Polling** : le client demande régulièrement. Simple, mais du trafic pour rien, et du retard.
- **Long polling** : le serveur garde la requête ouverte jusqu'à avoir une réponse.
- **Server-Sent Events** : un flux du serveur vers le client, à sens unique.
- **WebSocket** : une connexion à double sens, permanente.

### La mise à niveau
Tout commence par une requête HTTP :

```text
GET /api/ws HTTP/1.1
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: ...

HTTP/1.1 101 Switching Protocols
```

Après le `101`, la même connexion TCP ne parle plus HTTP : elle transporte des trames dans les deux sens, avec très peu d'en-têtes. [Nginx](../infra/nginx.md) doit transmettre `Upgrade`, ce que fait `map $http_upgrade $connection_upgrade`.

### Les limites du navigateur
L'API JavaScript `new WebSocket(url)` ne permet pas d'ajouter `Authorization`. D'où le jeton dans l'URL actuel, à remplacer par un ticket.

### Un seul WebSocket, plusieurs types
Ouvrir un WebSocket par fonctionnalité multiplierait les connexions et les authentifications. Une enveloppe permet d'en partager un seul :

```json
{"type":"message","data":{"conversation_id":4,"body":"Salut"}}
{"type":"notification","data":{"kind":"match","unread_count":3}}
{"type":"presence","data":{"account_id":7,"online":true}}
```

## Exemple déroulé
```text
1. Bob ouvre le site, le front ouvre /api/ws
2. Le hub range sa session sous account:12
3. Alice envoie un message
4. Le serveur le stocke en base, en transaction
5. Après le commit, hub.publish("account:12", message)
6. Le navigateur de Bob reçoit la trame, et l'interface se met à jour
```

Si Bob a deux onglets, le [fan-out](fan-out.md) atteint les deux. S'il est hors ligne, le message reste en base.

## L'autorisation dure toute la connexion
L'authentification se fait à l'ouverture, mais une connexion peut vivre des heures. Si Alice bloque Bob après l'ouverture, un contrôle à l'ouverture ne suffit plus. Chaque message reçu passe donc par les règles, comme une requête HTTP.

Il faut aussi renvoyer une erreur au client : aujourd'hui, un message refusé est ignoré en silence.

## Erreurs fréquentes
- Ne pas détecter une connexion morte : voir le [heartbeat](heartbeat.md).
- Faire confiance à ce que le client annonce, comme son id : l'identité vient du ticket.
- Écouter sans limite de débit.

## À retenir
Le WebSocket donne un canal permanent au serveur. La base reste la source de vérité, et le canal ne fait que prévenir.

## Exercice
Pourquoi vérifier le blocage à chaque message, et pas seulement à l'ouverture ?

## Corrigé
Le blocage peut arriver à tout moment. Un contrôle à l'ouverture laisserait une connexion existante continuer à envoyer. L'état du droit doit être évalué au moment de l'action.

## Voir aussi

- [Heartbeat : détecter une connexion morte](heartbeat.md)
- [Le ticket WebSocket](ticket-websocket.md)
- [Fan-out et Pub/Sub](fan-out.md)
- [unique_ptr, shared_ptr, weak_ptr](../cpp/pointeurs-intelligents.md)
