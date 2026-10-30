# Heartbeat : détecter une connexion morte
> Un ping régulier révèle les connexions disparues sans prévenir.

## Mise en situation
Un téléphone entre dans un tunnel. Aucune fermeture n'arrive au serveur. L'utilisateur reste affiché en ligne, et les messages partent dans le vide. Sa session reste en mémoire dans le hub.

## La théorie
### Pourquoi TCP ne suffit pas
TCP ne détecte une panne que s'il essaie d'envoyer. Si rien n'est envoyé, une connexion dont l'autre extrémité a disparu semble ouverte, parfois pendant des heures. Un proxy ou un routeur peut aussi fermer silencieusement une connexion inactive.

### Ping et pong
Le protocole [WebSocket](websocket.md) prévoit des trames de contrôle. Le serveur envoie un `ping`, et le navigateur répond automatiquement `pong`, sans code JavaScript.

```text
t=0   ping ->
t=0.1 <- pong        vivant
t=30  ping ->
t=40  (rien)         mort : fermeture
```

Crow fournit `send_ping`, et un délai d'inactivité sur les routes WebSocket.

### Choisir les délais
- Trop court : gaspillage, et déconnexion des réseaux lents.
- Trop long : [présence](presence.md) fausse.

Un ping toutes les 30 secondes et un délai de 10 secondes rendent une disparition visible en moins d'une minute.

Le heartbeat doit aussi être plus fréquent que `proxy_read_timeout` de [Nginx](../infra/nginx.md), 60 secondes par défaut, sinon Nginx coupe toute connexion silencieuse.

### Le debounce
Écrire `last_seen_at` à chaque activité produirait des milliers d'`UPDATE`. On garde en mémoire la dernière écriture, et on ne réécrit que si elle date de plus d'une minute. Une écriture finale a lieu à la fermeture.

## Exemple déroulé
```text
09:00:00 ouverture, online = true, écriture last_seen
09:00:30 pong, moins d'une minute : pas d'écriture
09:01:00 pong, plus d'une minute : écriture
09:01:30 aucune réponse
09:01:40 délai dépassé : fermeture, compteur à 0, écriture finale
```

## Reconnexion côté client
Le front doit se reconnecter avec un délai croissant : 1 s, 2 s, 4 s, avec un peu d'aléa. Si le serveur redémarre, cela évite que tous les clients reviennent à la même milliseconde.

## Erreurs fréquentes
- Un délai du proxy plus court que le ping.
- Compter sur l'événement `onclose` seul.
- Écrire en base à chaque ping.

## À retenir
Le silence ne prouve rien. Pour savoir qu'une connexion vit, il faut lui demander.

## Exercice
Pourquoi ajouter de l'aléa au délai de reconnexion ?

## Corrigé
Après un redémarrage, tous les clients ont perdu la connexion au même moment. Avec un délai identique, ils reviendraient tous ensemble, et surchargeraient le serveur. L'aléa étale les retours.

## Voir aussi

- [WebSocket](websocket.md)
- [Présence et dernière connexion](presence.md)
- [Nginx en proxy inverse](../infra/nginx.md)
