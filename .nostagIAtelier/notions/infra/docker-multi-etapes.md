# Docker : build multi-étapes
> On compile dans une image complète, puis on ne garde que le résultat dans une image légère.

## Mise en situation
Un correcteur lance `docker compose up` et doit obtenir une API qui fonctionne. Une image avec le compilateur, [vcpkg](../build/vcpkg.md) et les sources pèse des gigaoctets, et elle offre à un attaquant tous les outils dont il rêve.

## La théorie
### Image et conteneur
Une image est un modèle immuable, formé de couches. Un conteneur est une instance en cours d'exécution, isolée par le noyau. Ce n'est pas une machine virtuelle : il partage le noyau de l'hôte.

### Les couches et le cache
Chaque instruction crée une couche. Docker réutilise une couche si l'instruction et tout ce qui précède n'ont pas changé. On place donc ce qui change rarement en premier.

### Le multi-étapes
```dockerfile
FROM debian:bookworm AS deps
# compilateur, vcpkg, dépendances

FROM deps AS build
COPY . .
RUN cmake --preset release && cmake --build --preset release

FROM debian:bookworm-slim AS runtime
WORKDIR /app
COPY --from=build /app/build/release/matcha_backend ./matcha_backend
COPY migrations ./migrations
USER matcha
CMD ["./matcha_backend"]
```

Chaque `FROM` commence une étape. `COPY --from` prend seulement ce qu'il faut. L'image finale ne contient ni compilateur, ni sources.

### Les quatre étapes de matcha
- `deps` : compilateur, [vcpkg](../build/vcpkg.md), dépendances. Long, mis en cache.
- `dev` : `deps` plus scripts, code monté depuis l'hôte.
- `build` : compilation Release.
- `runtime` : binaire, [migrations](../sql/migrations.md) et bibliothèques système.

Dans Compose, `target: runtime` choisit l'étape.

## Exemple déroulé
```text
Tu modifies LikeRoutes.cpp
deps    : cache, rien à refaire
build   : COPY . . change, recompilation
runtime : recopie le binaire
```

Si `vcpkg.json` change, `deps` est reconstruite. C'est le cache décrit dans [vcpkg](../build/vcpkg.md).

### Les fichiers oubliés
L'étape `runtime` ne contient que ce qu'on y copie : [migrations](../sql/migrations.md), et plus tard la liste de mots de passe et les villes. Oublier un fichier casse uniquement en production.

### Lancer en non-root
`USER matcha` lance le programme avec un utilisateur dédié, créé avec `useradd --system`. En cas de compromission, l'attaquant n'est pas root dans le conteneur.

### .dockerignore
Il évite d'envoyer `build/`, `.git` ou `secrets/` dans le contexte.

## Erreurs fréquentes
- Copier tout le code avant d'installer les dépendances.
- Un [secret](secrets-fichiers.md) passé en `ARG` : il reste dans l'historique de l'image.
- Oublier une bibliothèque partagée dans `runtime`, ce qui donne `error while loading shared libraries` au démarrage.

## À retenir
L'image finale contient le strict nécessaire pour s'exécuter, rien de plus.

## Exercice
Pourquoi l'image `runtime` installe-t-elle `ca-certificates`, alors qu'elle ne contient presque rien d'autre ?

## Corrigé
Ce paquet fournit les autorités de certification. Sans lui, toute connexion TLS sortante, comme un appel HTTPS ou un envoi d'email chiffré, échouerait à vérifier le certificat du serveur. L'image finale doit contenir ce dont le programme a besoin à l'exécution, pas à la compilation.

## Voir aussi

- [vcpkg : gérer les dépendances](../build/vcpkg.md)
- [Docker Compose](docker-compose.md)
