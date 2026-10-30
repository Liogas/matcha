# vcpkg : gérer les dépendances
> Un fichier manifeste déclare les dépendances, et vcpkg les compile avec les bonnes versions.

## Mise en situation
Le backend dépend de Crow, [libpq](../sql/libpq.md), libsodium, curl, redis-plus-plus et [GoogleTest](../tests/gtest.md). Les installer à la main diffère selon les machines : mauvaises versions, chemins différents. Et ton conteneur doit être reproductible.

## La théorie
### C++ et ses paquets
Il n'y a pas d'outil officiel, comme `npm` ou `cargo`. vcpkg, de Microsoft, compile les bibliothèques depuis leurs sources.

### Le mode manifeste
`backend/vcpkg.json` :

```json
{
  "name": "matcha-backend",
  "version-string": "0.1.0",
  "dependencies": [
    "crow", "libpq", "gtest", "libsodium",
    {"name": "curl", "features": ["openssl"]}
  ]
}
```

`features` active des options, comme OpenSSL pour curl.

### Le lien avec CMake
La toolchain vcpkg indique à [CMake](cmake-cibles.md) où chercher :

```json
"toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
```

`find_package(PostgreSQL)` trouve alors la version vcpkg.

### La reproductibilité
Une `builtin-baseline`, un commit de vcpkg, fixe les versions de toutes les dépendances. Sans elle, deux compilations à des dates différentes peuvent donner des versions différentes.

## Exemple déroulé : le cache Docker
```dockerfile
WORKDIR /opt/manifest
COPY vcpkg.json ./
RUN /opt/vcpkg/vcpkg install --x-install-root="$VCPKG_INSTALLED_DIR"
```

Seul le manifeste est copié avant l'installation. Tant qu'il ne change pas, Docker garde cette couche, et plusieurs minutes de compilation sont épargnées. Si tout le code était copié avant, chaque modification de `.cpp` recompilerait toutes les dépendances.

`VCPKG_MANIFEST_INSTALL` vaut `OFF` dans le [preset](presets.md), car l'installation a déjà été faite à cette étape.

Chaque lib a aussi son propre `vcpkg.json`, pour être extraite un jour.

## Erreurs fréquentes
- Ajouter un paquet sans reconstruire l'image `deps`.
- Oublier une dépendance dans le `vcpkg.json` d'une lib.

## À retenir
Les dépendances sont déclarées dans un fichier versionné, et non installées à la main.

## Exercice
Que se passe-t-il pour le cache si tu ajoutes une ligne à `vcpkg.json` ?

## Corrigé
La couche `COPY vcpkg.json` change, donc elle et toutes les suivantes sont refaites : vcpkg réinstalle tout. C'est long mais normal, et cela reste rare.

## Voir aussi

- [Docker : build multi-étapes](../infra/docker-multi-etapes.md)
- [Rendre une lib installable](package-cmake.md)
