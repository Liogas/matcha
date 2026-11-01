# Healthcheck et arrêt propre
> Le serveur dit s'il est vivant et prêt, et il termine son travail avant de s'arrêter.

## Mise en situation
Docker lance le backend, mais comment sait-il qu'il répond vraiment ? Et lors d'un `docker stop`, une [transaction](../sql/transactions.md) en cours est coupée net si le processus meurt sans prévenir. La connexion disparaît, et PostgreSQL annule : c'est sûr, mais brutal pour l'utilisateur.

### Ce que `docker stop` déclenche
Imagine un utilisateur qui envoie un message à l'instant où tu redéploies. Selon que l'arrêt est propre ou non, il voit soit une réponse normale, soit une erreur réseau.

```tableau
titre: Un arrêt propre
colonnes: Docker | Backend | PostgreSQL
étape: Docker demande poliment au processus de s'arrêter.
Docker -> Backend: SIGTERM
étape: Crow n'accepte plus de nouvelles requêtes, mais termine celles en cours.
Backend: finit la requête
Backend -> PostgreSQL: COMMIT
étape: `app.run()` rend la main. `main` continue, et les objets sont détruits dans l'ordre inverse.
Backend: serveur détruit
Backend -> PostgreSQL: pool fermé
étape: Le processus sort de lui-même, bien avant les 10 secondes.
+ Backend -> Docker: exit 0
```

```tableau
titre: Le même arrêt, avec un script d'entrée sans exec
colonnes: Docker | sh | Backend
étape: Le PID 1 est le shell, pas le serveur. C'est lui qui reçoit le signal.
Docker -> sh: SIGTERM
étape: Le shell ne le transmet pas à son enfant. Le serveur continue comme si de rien n'était.
! sh: ignore le signal
étape: 10 secondes plus tard, Docker perd patience.
! Docker -> Backend: SIGKILL
étape: Les requêtes en cours sont coupées net, et chaque redéploiement prend 10 secondes.
! = coupure brutale
```

## La théorie
### Vivant ou prêt
- **Liveness** (`/health`) : le processus répond. Sinon, il faut le redémarrer.
- **Readiness** (`/ready`) : il peut servir, base et [Redis](redis.md) sont joignables. Sinon, on ne lui envoie pas de trafic, mais le redémarrer ne servirait à rien si c'est la base qui est tombée.

```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:18080/health"]
```

### Les signaux
`docker stop` envoie `SIGTERM`, attend 10 secondes, puis envoie `SIGKILL`, qu'on ne peut pas intercepter.

Crow gère les signaux :

```cpp
app.signal_clear();
app.signal_add(SIGINT);
app.signal_add(SIGTERM);
```

`app.run()` se termine alors proprement, et `main` poursuit.

### L'ordre de destruction
Après `run()`, les objets de `main` meurent dans l'ordre inverse. Le [pool](../sql/pool-connexions.md) est déclaré avant le serveur, donc il meurt après lui. C'est la [durée de vie](../cpp/duree-de-vie-objets.md) appliquée à un programme entier.

### PID 1
Dans un conteneur, le processus principal a le PID 1, et le noyau ne lui applique pas le comportement par défaut des signaux. Si un script shell lance le binaire sans `exec`, le signal n'arrive pas au binaire. Le [`Dockerfile`](docker-multi-etapes.md) utilise la forme exec, `CMD ["./matcha_backend"]`, qui lance directement le binaire. Dans un script d'entrée, il faudrait écrire :

```sh
exec ./matcha_backend
```

## Exemple déroulé
```text
1. docker stop
2. SIGTERM reçu : Crow arrête d'accepter
3. Les requêtes en cours se terminent
4. Les objets sont détruits, le pool ferme ses connexions
5. Exit 0, en 1 seconde
```

Sans gestion, on attend 10 secondes, puis `SIGKILL`.

### /ready
```cpp
auto rows = pool->query("SELECT 1");
return rows ? crow::response(200) : crow::response(503);
```

## Erreurs fréquentes
- Un `/health` qui interroge la base : une panne de base ferait redémarrer le backend en boucle.
- Oublier `exec`.
- Un arrêt qui ne ferme pas les [WebSockets](../temps-reel/websocket.md).

## À retenir
Un service doit pouvoir dire comment il va, et mourir proprement.

## Exercice
Pourquoi `/ready` ne doit-il pas servir à redémarrer un conteneur ?

## Corrigé
Il échoue quand une dépendance tombe. Redémarrer le backend ne répare pas PostgreSQL, et provoque des redémarrages en boucle, qui aggravent l'incident.

## Voir aussi

- [Docker Compose](docker-compose.md)
- [RAII](../cpp/raii.md)
