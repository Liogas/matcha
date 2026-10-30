# Nginx en proxy inverse
> Un seul point d'entrée reçoit tout le trafic, puis le distribue.

## Mise en situation
Le navigateur doit charger le front et appeler l'API, sur la même origine, ce qui évite les problèmes de CORS. Il faut aussi limiter le débit, borner les uploads et gérer les [WebSockets](../temps-reel/websocket.md), sans mettre tout cela dans le C++.

## La théorie
### Proxy inverse
Un proxy classique représente le client. Un proxy inverse représente le serveur : le client ne parle qu'à lui.

```text
Navigateur -> Nginx -> backend:18080
                    -> frontend
```

Il apporte un point unique pour le TLS, les limites et les en-têtes.

### Les locations
```nginx
location = /api/auth/login { ... }  # correspondance exacte
location /api/ { ... }              # préfixe
location / { ... }                  # le reste
```

Nginx choisit la correspondance la plus précise.

### limit_req
```nginx
limit_req_zone $login_limit_key zone=login_per_ip:10m rate=5r/m;
limit_req zone=login_per_ip burst=5 nodelay;
```

- La zone garde les compteurs en mémoire, 10 Mo ici.
- `rate=5r/m` : 5 requêtes par minute.
- `burst=5` : une rafale de 5 est tolérée.
- `nodelay` : les requêtes de la rafale passent sans attendre.
- `limit_req_status 429` : le code renvoyé.

La clé `$login_limit_key` vaut vide pour autre chose qu'un POST, et Nginx ne compte pas une clé vide.

### Les en-têtes transmis
```nginx
proxy_set_header X-Real-IP $remote_addr;
```

Ce remplacement écrase ce que le client a envoyé : c'est ce qui rend l'en-tête utilisable, voir les [proxies de confiance](../web/proxies-confiance.md).

### Le WebSocket
```nginx
map $http_upgrade $connection_upgrade {
    default upgrade;
    '' close;
}
proxy_http_version 1.1;
proxy_set_header Upgrade $http_upgrade;
proxy_set_header Connection $connection_upgrade;
```

Sans cela, `Upgrade` n'est pas transmis, et le [WebSocket](../temps-reel/websocket.md) échoue.

## Exemple déroulé
```text
1. POST /api/auth/login
2. Location exacte, limite vérifiée
3. X-Real-IP réécrit
4. Transmission au backend
5. Réponse renvoyée au client
```

La 12e tentative en une minute reçoit `429` sans atteindre le C++, donc sans dépenser d'[Argon2](../auth/argon2id.md).

## Erreurs fréquentes
- Oublier une location, et ne plus avoir de limite.
- `proxy_read_timeout` trop court pour les [WebSockets](../temps-reel/websocket.md).
- Un `client_max_body_size` trop bas pour les photos.

## À retenir
Ce qui se décide sans la base doit se décider au plus tôt : à l'entrée, dans Nginx.

## Exercice
Pourquoi garder `limit_req` alors qu'il existe un [limiteur](../auth/rate-limiting.md) [Redis](redis.md) ?

## Corrigé
Ils protègent des choses différentes. Nginx bloque un volume par IP, sans coût pour le backend. [Redis](redis.md) compte les échecs par compte et IP, avec la logique métier. Ils se complètent, et une attaque sur 1000 comptes depuis une IP n'est vue que par Nginx.

## Voir aussi

- [Proxies de confiance](../web/proxies-confiance.md)
- [Limitation de débit et Redis](../auth/rate-limiting.md)
- [Heartbeat : détecter une connexion morte](../temps-reel/heartbeat.md)
