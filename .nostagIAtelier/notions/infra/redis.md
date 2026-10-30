# Redis
> Une base en mémoire, très rapide, idéale pour les données [temporaires](../cpp/duree-de-vie-objets.md) et les compteurs.

## Mise en situation
Le [limiteur](../auth/rate-limiting.md) de connexion doit compter des essais, partagés entre threads, avec une expiration automatique. PostgreSQL le pourrait, mais chaque essai coûterait une écriture sur disque, pour une donnée sans importance après 5 minutes.

## La théorie
### Le modèle
Redis stocke des clés et des valeurs en mémoire : chaînes, compteurs, listes, ensembles, tables.

### Un seul thread
Les commandes s'exécutent l'une après l'autre. Chaque commande est donc atomique. Conséquence : une commande lente, comme `KEYS *`, bloque tout le monde.

### Les commandes utiles
- `INCR clé` : incrément atomique.
- `EXPIRE clé 300` : suppression automatique.
- `TTL clé` : temps restant.
- `SET clé valeur EX 30 NX` : poser si absent, avec expiration.
- `GETDEL clé` : lire et supprimer.
- `EVAL script` : Lua exécuté d'un bloc.
- `PUBLISH` et `SUBSCRIBE` : diffusion sans stockage.

### La persistance
Redis peut écrire sur disque (RDB, AOF), mais il faut considérer ses données comme perdables. PostgreSQL reste la source de vérité.

## Exemple déroulé : le limiteur
```cpp
const auto result = _redis.eval<std::vector<long long>>(
    ATTEMPT_SCRIPT, {key}, {window, maxAttempts});
```

Le script Lua fait `INCR`, puis `EXPIRE` si c'est le premier essai. Comme il s'exécute d'un seul bloc, aucun autre client ne peut s'intercaler. Voir la [limitation de débit](../auth/rate-limiting.md).

### La présence
`SET presence:7 1 EX 60`, rafraîchi par le [heartbeat](../temps-reel/heartbeat.md). Si le serveur plante, la clé expire seule.

### Sécurité
Redis ne doit pas être exposé. Le compose garde le port interne, et un mot de passe en fichier protège l'accès.

## Quand ne pas l'utiliser
Pour une donnée dont la perte est grave, ou qui demande des relations. Un cache ajoute aussi une source d'incohérence : ne l'ajoute pas sans besoin mesuré.

## Erreurs fréquentes
- `KEYS *` en production : utilise `SCAN`.
- Une clé sans expiration pour une donnée [temporaire](../cpp/duree-de-vie-objets.md) : la mémoire se remplit.
- Traiter Redis comme une base durable.

## À retenir
Redis sert aux données rapides, [temporaires](../cpp/duree-de-vie-objets.md) et reconstruisibles. Le reste va dans PostgreSQL.

## Exercice
Pourquoi un TTL convient-il mieux à la [présence](../temps-reel/presence.md) qu'un état mis à jour à la déconnexion ?

## Corrigé
Si le serveur plante, ou si un client disparaît, personne n'écrit la déconnexion, et l'utilisateur reste en ligne pour toujours. Avec un TTL, l'absence de rafraîchissement suffit : la clé disparaît seule.

## Voir aussi

- [Limitation de débit et Redis](../auth/rate-limiting.md)
- [Fan-out et Pub/Sub](../temps-reel/fan-out.md)
- [Présence et dernière connexion](../temps-reel/presence.md)
