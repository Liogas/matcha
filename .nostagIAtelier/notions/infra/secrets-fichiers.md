# Secrets et fichiers .env
> Un secret ne doit jamais être dans Git, ni visible dans la configuration d'un conteneur.

## Mise en situation
Le sujet prévient : le stockage public d'identifiants peut entraîner l'échec du projet. L'audit avait trouvé `admin/admin` dans `docker-compose.yml`. C'est corrigé, et il faut comprendre pourquoi cela compte.

## La théorie
### Git n'oublie rien
Un secret commité reste dans l'historique, même supprimé ensuite. Des robots scannent GitHub en continu. Une fois publié, un secret est compromis : il faut le changer, pas seulement le retirer.

### Trois niveaux
- **Dans le code** : à proscrire.
- **Variable d'environnement** : mieux, mais visible avec `docker inspect` et dans `/proc`.
- **Fichier monté** : le conteneur lit un fichier dans `/run/secrets/`, invisible dans sa configuration.

### Dans matcha
```yaml
environment:
  POSTGRES_PASSWORD_FILE: /run/secrets/db_password
secrets:
  - db_password
```

- `.env` et `/secrets/` sont dans `.gitignore`.
- `.env.example` documente les variables, sans vraies valeurs.
- `scripts/init-secrets.sh` les génère.
- `ConfigReader` applique la convention `*_FILE`.

## Exemple déroulé
Un nouveau développeur clone le dépôt :

```text
1. cp .env.example .env
2. ./scripts/init-secrets.sh
3. docker compose up
```

Aucun secret ne transite par Git. La clé [RS256](../auth/signature-rs256.md) est créée localement.

### Vérifier
```sh
git log --all -p -- '*.pem' | head
git grep -i password
```

Vérifie aussi que `.env.example` ne contient que des valeurs fictives : ce fichier, lui, est versionné.

### Permissions
`chmod 600` sur les fichiers de secrets.

## Dans les logs
Ne journalise jamais un secret. Si `ConfigReader` affiche la configuration au démarrage, il doit masquer ces valeurs.

## Erreurs fréquentes
- Un secret en argument de ligne de commande : visible par `ps`.
- `ARG` dans un [Dockerfile](docker-multi-etapes.md).
- Un `.env` copié dans l'image : `.dockerignore`.

## À retenir
Un secret se génère localement, se monte en fichier et ne quitte jamais la machine.

## Exercice
Tu as committé une clé par erreur, puis tu l'as supprimée au commit suivant. Que dois-tu faire ?

## Corrigé
La considérer comme compromise et la remplacer. L'historique la contient toujours. Réécrire l'historique peut aider, mais ne garantit rien si elle a été poussée.

## Voir aussi

- [Docker Compose](docker-compose.md)
- [Signatures asymétriques : RS256](../auth/signature-rs256.md)
