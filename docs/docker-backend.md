# Fiche : Docker, build multi-étapes et dev du backend

## TL;DR — les commandes du quotidien

| Je veux…                                      | Commande          |
|-----------------------------------------------|-------------------|
| Tout lancer en dev (1re fois / après un changement de Dockerfile ou `vcpkg.json`) | `make build` |
| Tout lancer en dev (les fois suivantes)       | `make up`         |
| **Recompiler mon backend après avoir modifié le code** | `make rebuild` (dans un 2e terminal) |
| Lancer les tests                              | `make test`       |
| Voir les logs du backend                      | `make logs`       |
| Ouvrir un terminal dans le conteneur backend  | `make shell`      |
| Tout arrêter                                  | `make down`       |
| Lancer comme en production / soutenance       | `make prod` (puis `make prod-down`) |

---

## 1. Rappel : image, conteneur, volume

- **Image** : un « modèle » figé (un système de fichiers + une commande de démarrage),
  construit à partir du `Dockerfile`. On la construit avec `build`.
- **Conteneur** : une image *en cours d'exécution*. On peut en lancer plusieurs à partir
  de la même image. Ce qu'on écrit dedans disparaît quand il est supprimé…
- **Volume** : … sauf ce qui est écrit dans un volume. C'est un dossier qui survit au
  conteneur. Deux sortes :
  - *volume nommé* (`backend_build:/app/build`) : géré par Docker/Podman, stocké chez
    nous dans `/goinfre/glions/containers` (pas dans ta session) ;
  - *bind mount* (`./backend:/app`) : un dossier de ta machine « branché » dans le
    conteneur. Modifier un fichier sur l'hôte le modifie instantanément dans le conteneur.

> Sur cette machine, `docker` est en réalité **Podman** (même commandes) et
> `docker compose` appelle **podman-compose**.

## 2. Le build multi-étapes (`backend/Dockerfile`)

Un `Dockerfile` multi-étapes contient plusieurs `FROM … AS <nom>`. Chaque étape est
une image intermédiaire ; une étape peut repartir d'une autre (`FROM deps AS dev`) ou
lui piocher des fichiers (`COPY --from=build …`). On choisit l'étape finale avec
`target:` dans compose.

```
            ┌──────────────────────────────┐
            │ deps                         │  compilateur, cmake, vcpkg
            │ + dépendances C++ compilées  │  (crow, libpq, libvips…) dans /opt/vcpkg_installed
            └──────┬───────────────┬───────┘
                   │               │
         ┌─────────▼─────┐   ┌─────▼──────────────┐
         │ dev           │   │ build              │  copie le code, compile en Release
         │ + scripts     │   └─────┬──────────────┘
         │   matcha-*    │         │ COPY --from=build  (seulement le binaire)
         └───────────────┘   ┌─────▼──────────────┐
          utilisée par       │ runtime            │  debian-slim + binaire + migrations
          make up / build    │ (~110 Mo)          │  utilisateur non-root « matcha »
                             └────────────────────┘
                              utilisée par make prod
```

Pourquoi c'est bien :

- **Image finale légère et sûre** : pas de compilateur, pas de code source, pas de vcpkg,
  et le serveur ne tourne pas en root. ~110 Mo au lieu de plusieurs Go.
- **Cache** : Docker réutilise une étape tant que ce qu'elle copie n'a pas changé.
  L'étape `deps` ne copie **que `vcpkg.json`** : modifier ton code ne relance pas les
  ~6 minutes de compilation des dépendances. Seul un changement de `vcpkg.json` le fait.
- **Une seule source de vérité** : dev et prod partent exactement des mêmes dépendances.

Détail : vcpkg compile les bibliothèques en **statique**, elles sont donc incluses dans
le binaire. C'est pourquoi l'image `runtime` n'a besoin d'aucune bibliothèque en plus
(vérifiable avec `ldd ./matcha_backend` dans le conteneur).

### Ce qui n'allait pas dans le brouillon

| Problème | Correction |
|---|---|
| `-DCMAKE_TOOLCHAIN_FILE_FILE` (faute de frappe) : cmake ne trouvait pas vcpkg | `-DCMAKE_TOOLCHAIN_FILE` |
| `libsodium 23` : apt cherchait deux paquets, `libsodium` et `23` | inutile : tout est lié en statique |
| `migrations/` absent de l'image finale : le serveur ne pouvait pas initialiser la base | `COPY migrations ./migrations` |
| `COPY . .` avant `vcpkg install` : chaque modif de code recompilait toutes les dépendances | on copie `vcpkg.json` seul d'abord |
| `as` en minuscules | `AS` (simple avertissement) |

## 3. Le mode dev : recompiler sans relancer le conteneur

En dev, compose combine **deux fichiers** (c'est ce que fait le Makefile) :

```
docker compose -f docker-compose.yml -f docker-compose.dev.yml up
```

`docker-compose.dev.yml` *surcharge* le service backend :

- `target: dev` : on utilise l'image avec compilateur et dépendances ;
- `./backend:/app` : ton code est monté dans le conteneur, rien à copier ;
- `backend_build:/app/build` : les fichiers compilés (`.o`) sont gardés dans un volume.
  Une recompilation ne refait que les fichiers modifiés, et rien n'arrive dans ta session.

Dans le conteneur, le processus principal est `matcha-dev-server` (`backend/dev/`) :

```
démarrage du conteneur
   └─ pas encore de binaire ? → matcha-build
   └─ boucle :  lance ./build/matcha_backend
                attend qu'il s'arrête
                le relance 2 s plus tard
```

Et `make rebuild` exécute `matcha-rebuild` **dans le conteneur déjà lancé** :

1. `matcha-build` : `cmake` + compilation (Debug) de `matcha_backend` ;
2. si ça compile → il arrête le serveur, que la boucle relance aussitôt avec le nouveau binaire ;
3. si ça ne compile pas → l'erreur s'affiche et **l'ancien serveur continue de tourner**.

Workflow type :

```sh
make up            # terminal 1 : tout tourne, logs visibles
# ... tu modifies backend/src/... dans VS Code ...
make rebuild       # terminal 2 : recompile + redémarre le serveur (~quelques secondes)
make test          # compile aussi les tests puis lance ctest
```

Quand faut-il quand même relancer avec `make build` ?
- tu as modifié `backend/Dockerfile`, `vcpkg.json` ou un script de `backend/dev/` ;
- tu as modifié `docker-compose*.yml` ou `.env`.

## 4. Le mode prod (`make prod`)

`docker compose up --build` avec seulement `docker-compose.yml` : le backend utilise
l'étape `runtime`, le code est *copié et compilé dans l'image*, aucun dossier de l'hôte
n'est monté. C'est ce qu'obtient quelqu'un qui clone le dépôt et lance `docker compose up`.

`userns_mode: "keep-id:uid=10001,gid=10001"` (spécifique Podman) fait correspondre ton
utilisateur à l'utilisateur `matcha` du conteneur, pour qu'il puisse lire les secrets
(fichiers en `600` qui t'appartiennent). Même principe que pour PostgreSQL (uid 999).

## 5. Les secrets (rappel)

`make build`, `make up` et `make prod` lancent d'abord `make secrets`
(`scripts/init-secrets.sh`), qui crée `.env` et génère dans `secrets/` ce qui manque
(mot de passe PostgreSQL, mot de passe Redis, clé JWT). Compose les monte dans
`/run/secrets/` ; aucun mot de passe n'est écrit dans les fichiers compose ni dans l'environnement.

## 6. Dépannage

| Symptôme | Piste |
|---|---|
| `executable file /app/./matcha_backend not found` au `make up` | le conteneur de dev a démarré sur l'image de prod. Vérifie que `docker-compose.yml` (`image: matcha_backend:prod`) et `docker-compose.dev.yml` (`image: matcha_backend:dev`) ont bien des noms d'image différents, puis `make build` |
| `make rebuild` : `no container with name … running` | le dev n'est pas lancé : `make up` |
| Le serveur redémarre en boucle (`[dev] Serveur arrêté (code 1)`) | il plante au démarrage, lis le message juste au-dessus dans `make logs` |
| Erreurs cmake bizarres après un changement de Dockerfile | supprimer le cache de build : `make down && docker volume rm matcha_backend_build && make build` |
| `password authentication failed` (PostgreSQL) | `secrets/db-password` a été régénéré après la création de la base, voir README › Démarrage |
| Plus de place dans la session | images et volumes sont dans `/goinfre/glions/containers` ; nettoyer avec `docker system prune` (supprime images et conteneurs inutilisés) |
| Ajout d'un fichier `.cpp` | l'ajouter dans `backend/CMakeLists.txt`, puis `make rebuild` (cmake est relancé automatiquement) |
