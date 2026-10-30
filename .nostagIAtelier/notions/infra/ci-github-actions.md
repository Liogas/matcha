# Intégration continue avec GitHub Actions
> Chaque push est compilé et testé automatiquement sur une machine propre.

## Mise en situation
Ça marche sur ta machine. Mais un fichier oublié dans le commit, un test cassé, ou une dépendance présente seulement chez toi casse le dépôt, parfois juste avant la soutenance. La CI le voit à chaque push.

## La théorie
### Le principe
À chaque événement, GitHub lance une machine neuve, qui exécute les étapes d'un fichier `.github/workflows/*.yml`.

```yaml
name: backend
on:
  push:
    branches: [main]
  pull_request:

jobs:
  test:
    runs-on: ubuntu-latest
    services:
      postgres:
        image: postgres:17
        env:
          POSTGRES_PASSWORD: test
        options: >-
          --health-cmd pg_isready
    steps:
      - uses: actions/checkout@v4
      - run: docker build --target deps -t matcha-deps backend
      - run: docker run --rm -e PG_TEST_HOST=... matcha-deps ctest --preset debug
```

### Les étapes utiles pour matcha
1. Compiler en `debug` : [`-Werror`](../build/warnings.md) prouve l'absence de warnings.
2. Lancer les tests avec une base.
3. Lancer le [preset](../build/presets.md) `sanitize`.
4. Optionnel : clang-format et clang-tidy.

### Le cache
La compilation des dépendances est longue. On peut mettre en cache l'image `deps` ou le répertoire binaire de [vcpkg](../build/vcpkg.md). Sans cache, chaque exécution peut durer très longtemps.

### Les secrets en CI
Les vraies clés vont dans les [secrets](secrets-fichiers.md) du dépôt. Les tests utilisent des valeurs fictives, comme `POSTGRES_PASSWORD: test` ci-dessus.

## Exemple déroulé
```text
1. Tu pousses libs/cpp-pg-2
2. Le workflow démarre
3. La compilation échoue : un en-tête manque, présent seulement dans ton build
4. Tu reçois une croix rouge sur le commit
```

Une pull request ne devrait être fusionnée qu'avec une CI verte.

## Erreurs fréquentes
- Une CI lente que personne ne regarde.
- Des tests instables, qu'on relance jusqu'à ce qu'ils passent.
- Un [secret](secrets-fichiers.md) réel écrit dans le fichier.

## À retenir
La CI est un collègue qui recompile tout depuis zéro, à chaque fois, sans se fatiguer.

## Exercice
Pourquoi une machine propre attrape-t-elle des erreurs que la tienne ne voit pas ?

## Corrigé
Ta machine contient des fichiers non commités, d'anciens objets de compilation, et des dépendances installées à la main. La CI ne voit que ce qui est dans le dépôt.

## Voir aussi

- [Les presets CMake](../build/presets.md)
- [Les sanitizers : ASan, UBSan, LSan](../build/sanitizers.md)
