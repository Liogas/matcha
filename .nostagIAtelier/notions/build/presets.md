# Les presets CMake
> Un fichier versionné nomme les configurations de build, pour que tout le monde lance la même chose.

## Mise en situation
Sans preset, il faut retenir une longue ligne :

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCPP_CORE_BUILD_TESTS=ON -DCPP_PG_BUILD_TESTS=ON ...
```

Une option oubliée donne un build différent, et personne ne sait pourquoi.

## La théorie
`CMakePresets.json` décrit des configurations nommées, avec héritage. Dans le projet :

- `base` : caché, définit le dossier, la toolchain et [`-Werror`](warnings.md) ;
- `debug` : hérite de `base`, active les tests ;
- `release` : optimisé, sans tests ;
- `sanitize` : hérite de `debug`, ajoute les [sanitizers](sanitizers.md).

```json
{
  "name": "debug",
  "inherits": "base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "CPP_PG_BUILD_TESTS": "ON"
  }
}
```

`binaryDir` vaut `${sourceDir}/build/${presetName}` : chaque preset a son dossier. C'est indispensable, car `sanitize` et `debug` ne peuvent pas partager leurs fichiers objets : ils ne sont pas compilés avec les mêmes options.

### Le cache
Au premier `cmake --preset`, les choix sont écrits dans `CMakeCache.txt`. Les changements de preset ne sont pas toujours repris : en cas de doute, supprime ce dossier de build.

### Variables d'environnement
`$env{VCPKG_ROOT}` lit l'environnement, ce qui évite un chemin en dur.

## Exemple déroulé
Ajouter `cpp-geo` :

1. `new-lib.sh cpp-geo geo` génère la lib.
2. `libs/CMakeLists.txt` : `add_subdirectory(cpp-geo)`, après ses dépendances.
3. Preset `debug` : `"CPP_GEO_BUILD_TESTS": "ON"`.
4. `cmake --preset debug` puis `ctest --preset debug`.

Sans l'étape 3, les tests ne sont pas compilés, et rien ne le signale.

## Erreurs fréquentes
- Mettre des chemins personnels dans le fichier versionné.
- Oublier l'option de tests d'une nouvelle lib.

## À retenir
La configuration de build est du code : elle est versionnée, nommée et partagée.

## Exercice
Pourquoi les tests sont-ils désactivés par défaut en sous-projet ?

## Corrigé
Un consommateur d'une lib ne veut pas compiler ses tests : cela ralentit le build et ajoute des dépendances. Le preset `debug` les active là où on travaille sur ces libs.

## Voir aussi

- [Les sanitizers : ASan, UBSan, LSan](sanitizers.md)
- [Warnings en erreur](warnings.md)
- [CMake : cibles et visibilité](cmake-cibles.md)
