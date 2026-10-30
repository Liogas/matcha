# Docker Compose
> Un fichier décrit tous les services, leurs réseaux et leur ordre de démarrage.

## Mise en situation
matcha, c'est PostgreSQL, [Redis](redis.md), le backend, le frontend, [Nginx](nginx.md) et Mailpit. Les lancer un par un avec les bonnes options est pénible. `docker compose up` doit suffire.

## La théorie
### Services et réseau
Chaque service tourne dans son conteneur, sur un réseau privé commun. Un service est joignable par son nom, grâce à un DNS interne :

```text
http://backend:18080
```

### ports et exposition
- `ports` publie un port sur l'hôte.
- Sans `ports`, le service reste interne au réseau du compose.

```yaml
ports:
  - "127.0.0.1:5432:5432"
```

Le préfixe `127.0.0.1` limite l'accès à la machine. Sans lui, le port est ouvert sur toutes les [interfaces](../cpp/interfaces-injection.md), parfois même malgré le pare-feu.

### Volumes
- Volume nommé : `postgres_data` survit à `docker compose down`, pas à `down -v`.
- Bind mount : un dossier de l'hôte, utile pour le développement.
- `tmpfs` : en mémoire. `postgres-test` l'utilise pour des tests rapides et jetables.

### Démarrage et santé
```yaml
healthcheck:
  test: ["CMD-SHELL", "pg_isready -U \"$$POSTGRES_USER\""]
  interval: 5s
```

```yaml
depends_on:
  database:
    condition: service_healthy
```

Sans `condition`, `depends_on` attend seulement que le conteneur démarre, pas que le service soit prêt.

### Plusieurs fichiers
`docker-compose.dev.yml` ajoute ce qui est propre au développement, comme `postgres-test`.

## Exemple déroulé
```text
docker compose up
1. database démarre, healthcheck
2. redis démarre
3. Quand database est sain, backend démarre
4. Le backend applique les migrations
5. nginx et frontend démarrent
```

Le [pool](../sql/pool-connexions.md) garde tout de même ses tentatives de connexion : un service peut redémarrer à tout moment.

## Erreurs fréquentes
- Publier inutilement la base ou le backend.
- Oublier `down -v` et croire la base vide.
- Croire que `depends_on` attend l'état prêt.

## À retenir
Compose est la description exécutable de l'infrastructure. Ce qui n'est pas publié n'est pas exposé.

## Exercice
Pourquoi le port 5432 ne doit-il pas être publié sur toutes les [interfaces](../cpp/interfaces-injection.md) ?

## Corrigé
La base serait joignable depuis le réseau, et seul son mot de passe la protégerait. Sur `127.0.0.1`, seule la machine y accède. En production, on ne le publierait pas du tout.

## Voir aussi

- [Secrets et fichiers .env](secrets-fichiers.md)
- [Docker : build multi-étapes](docker-multi-etapes.md)
- [Healthcheck et arrêt propre](sante-arret-propre.md)
