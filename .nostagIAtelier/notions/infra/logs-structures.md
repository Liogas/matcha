# Des logs utiles : niveaux et request_id
> Un log doit permettre de reconstituer une requête, sans jamais contenir de [secret](secrets-fichiers.md).

## Mise en situation
Un utilisateur signale une erreur à 14 h 03. Les logs sont des `std::cerr` sans niveau, mêlés entre dix requêtes simultanées. Impossible de savoir quelles lignes vont ensemble.

## La théorie
### Les niveaux
- **debug** : détail pour le développement.
- **info** : événement normal.
- **warn** : anormal, mais géré.
- **error** : une opération a échoué.

En production, on garde `info` et au-dessus.

### La corrélation
Un `request_id` naît dans le [middleware](../web/middleware-crow.md), accompagne chaque ligne, et revient dans un en-tête de réponse :

```text
[info]  req=a81f like actor=12 target=7
[error] req=a81f pg error 40P01 deadlock
```

`grep a81f` donne toute l'histoire de cette requête.

### Le logger injecté
`core::ILogger` est une [interface](../cpp/interfaces-injection.md). Les tests peuvent injecter un faux qui capture les messages. Une lib ne doit jamais écrire directement dans `std::cerr`.

### Le coût et le verrou
Plusieurs threads écrivent. `StreamLogger` formate hors verrou, puis écrit sous verrou, pour que les lignes ne se mélangent pas.

### Ce qu'il ne faut pas journaliser
Mots de passe, tokens, [JWT](../auth/jwt.md), [cookies](../web/cookies.md), coordonnées, messages privés, orientation. Une ligne de log est copiée, archivée et lue par d'autres. Les logs fuient plus facilement qu'une base.

## Exemple déroulé
Mauvais :

```cpp
logger.info("login " + email + " " + password);
```

Bon :

```cpp
logger.info("login failed account=" + std::to_string(id));
```

Une erreur SQL est journalisée avec son détail côté serveur, et le client reçoit un message générique. Voir Result.

## Les logs structurés
Un format JSON par ligne facilite la recherche :

```json
{"level":"info","req":"a81f","msg":"like","actor":12}
```

## Erreurs fréquentes
- Journaliser dans une boucle chaude.
- Des URL avec jetons, comme le [WebSocket](../temps-reel/websocket.md) actuel.
- Un message sans contexte, comme `error`.

## À retenir
Écris tes logs pour la personne qui les lira pendant une panne, et qui n'a pas le droit de voir les [secrets](secrets-fichiers.md).

## Exercice
Pourquoi le [JWT](../auth/jwt.md) dans l'URL du [WebSocket](../temps-reel/websocket.md) est-il un problème de logs ?

## Corrigé
[Nginx](nginx.md) écrit toutes les URL dans ses logs d'accès. Chaque [JWT](../auth/jwt.md) y reste en clair et reste valable 15 minutes. Toute personne ayant accès à ces fichiers peut l'utiliser. Le ticket règle le problème.

## Voir aussi

- [Middleware Crow et contexte de requête](../web/middleware-crow.md)
- [Mutex et contention de verrou](../cpp/mutex-contention.md)
