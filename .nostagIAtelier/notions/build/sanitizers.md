# Les sanitizers : ASan, UBSan, LSan
> Le compilateur instrumente le programme pour détecter, à l'exécution, des bugs mémoire invisibles.

## Mise en situation
Un `string_view` pointe sur une chaîne détruite. Le programme affiche souvent le bon texte : la mémoire n'a pas encore été réécrite. Le bug peut dormir des mois, puis provoquer un plantage impossible à reproduire. Un sanitizer l'arrête au premier accès.

## La théorie
### Le principe
Le compilateur ajoute une vérification à chaque accès mémoire. ASan garde une mémoire parallèle, dite shadow memory, qui indique pour chaque octet s'il est accessible. Autour de chaque allocation, il place des zones rouges. La mémoire libérée est mise en quarantaine.

### Les outils
- **AddressSanitizer** : accès hors limites, [use-after-free](../cpp/duree-de-vie-objets.md), stack-use-after-return.
- **LeakSanitizer** : à la fin du programme, mémoire jamais libérée. Il tourne avec ASan.
- **UndefinedBehaviorSanitizer** : débordement d'entier signé, décalage invalide, déréférencement nul, conversion invalide.
- **[ThreadSanitizer](../cpp/data-race.md)** : voir le data race.

```sh
-fsanitize=address,undefined -fno-omit-frame-pointer
```

ASan et TSan sont incompatibles : ils utilisent la mémoire différemment.

### Le coût
Environ un facteur 2 en temps. C'est réservé aux tests, jamais à la production.

### Sa limite
Il ne voit que le code exécuté. Sans test qui passe par le bug, rien n'est signalé. C'est pour cela qu'on les lance sur toute la suite.

## Exemple déroulé : lire un rapport
```text
==123==ERROR: AddressSanitizer: heap-use-after-free on address 0x602...
READ of size 8 at 0x602... thread T0
    #0 pg::Row::get  Rows.cpp:42
    #1 lireProfil    ProfileRepository.cpp:30

freed by thread T0 here:
    #0 PQclear
    #1 pg::Rows::~Rows  Rows.cpp:12

previously allocated by thread T0 here:
    #0 PQexecParams
    #1 pg::Connection::execute  Connection.cpp:88
```

Trois piles racontent la vie de l'objet :

1. où on a lu ;
2. où on a libéré ;
3. où on a alloué.

Ici, une `Row` a été utilisée après la mort de ses `Rows`. Toute l'analyse de [durée de vie](../cpp/duree-de-vie-objets.md) est dans ce rapport.

### Une fuite
```text
ERROR: LeakSanitizer: detected memory leaks
Direct leak of 136 byte(s) in 1 object(s) allocated from:
    #0 malloc
    #1 PQmakeEmptyPGresult
```

Un [`PGresult`](../sql/libpq.md) non libéré : il manque un `PQclear`, donc un [RAII](../cpp/raii.md).

## Dans matcha
```sh
cmake --preset sanitize && cmake --build --preset sanitize
ctest --preset sanitize
```

## Erreurs fréquentes
- Lire seulement la première ligne du rapport.
- Lancer sans tests.
- Compiler sans `-g` : les lignes manquent.

## À retenir
Un code qui « marche » peut être faux. Les sanitizers transforment un comportement indéfini silencieux en plantage immédiat et expliqué.

## Exercice
Pourquoi faut-il un dossier de build séparé pour `sanitize` ?

## Corrigé
Tous les fichiers doivent être compilés avec l'instrumentation : mélanger des objets avec et sans donnerait de faux résultats, et [CMake](cmake-cibles.md) devrait tout recompiler à chaque changement de [preset](presets.md).

## Voir aussi

- [La durée de vie des objets](../cpp/duree-de-vie-objets.md)
- [Data race et ThreadSanitizer](../cpp/data-race.md)
- [Les presets CMake](presets.md)
