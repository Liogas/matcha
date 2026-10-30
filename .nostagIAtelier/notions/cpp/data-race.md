# Data race et ThreadSanitizer
> Deux threads touchent la même mémoire sans synchronisation, et au moins l'un écrit : le programme n'a plus de sens défini.

## Mise en situation
[`PresenceTracker`](../temps-reel/presence.md) compte les connexions de chaque compte. Deux onglets du même utilisateur se ferment au même instant :

```cpp
_compteurs[id] -= 1;   // sans protection, depuis deux threads
```

Cette ligne n'est pas une action unique :

```text
Thread 1 : lit 2
Thread 2 : lit 2
Thread 1 : écrit 1
Thread 2 : écrit 1
```

Il restait 0 connexion, mais le compteur vaut 1. L'utilisateur restera en ligne pour toujours. Et `_compteurs` étant une table de hachage, une insertion simultanée peut la réorganiser pendant qu'un autre thread la lit, et faire planter le serveur.

## La théorie
### La définition exacte
Il y a data race quand deux accès à la même mémoire :

1. viennent de threads différents ;
2. ne sont pas ordonnés par une synchronisation ;
3. comprennent au moins une écriture.

Pour la norme C++, c'est un comportement indéfini : le compilateur suppose que cela n'arrive jamais, et il optimise en conséquence.

### Pourquoi ce n'est pas seulement une valeur fausse
Le compilateur peut garder une variable dans un registre, et réordonner des instructions. Le processeur a des caches et peut réordonner ses écritures. Exemple classique :

```cpp
bool pret = false;
// Thread 1 : donnee = 42; pret = true;
// Thread 2 : while (!pret) {}  use(donnee);
```

Le thread 2 peut boucler pour toujours, car le compilateur a le droit de lire `pret` une seule fois. Il peut aussi voir `pret` à `true` avant `donnee`.

### Happens-before
Une synchronisation crée une relation « arrive avant ». Le déverrouillage d'un [mutex](mutex-contention.md) arrive avant le verrouillage suivant : tout ce qui a été écrit dans la section critique est visible par le thread suivant. Sans cette relation, un thread n'a aucune garantie de voir les écritures d'un autre.

### Les outils
- [std::mutex](mutex-contention.md) : pour plusieurs données ou une opération composée.
- `std::atomic<T>` : pour une valeur simple. `compteur.fetch_sub(1)` est une opération indivisible.

Une `atomic` ne protège que son propre accès. Deux `atomic` modifiées ensemble ne forment pas une opération atomique.

### Data race et race condition
Ce sont deux notions différentes :

- **Data race** : un problème de mémoire, détectable par un outil.
- **Race condition** : le résultat dépend de l'ordre d'exécution, même si chaque accès est protégé.

Le like mutuel est une race condition sans aucun data race : la mémoire C++ est saine, c'est l'ordre des requêtes SQL qui pose problème, voir l'[isolation](../sql/isolation.md).

## ThreadSanitizer
TSan, activé avec `-fsanitize=thread`, instrumente chaque accès mémoire et chaque synchronisation. Il signale deux accès non ordonnés, même si le bug ne s'est pas manifesté à cette exécution :

```text
WARNING: ThreadSanitizer: data race
  Write of size 4 at 0x... by thread T2:
    #0 PresenceTracker::onClose PresenceTracker.cpp:42
  Previous read of size 4 at 0x... by thread T1:
    #0 PresenceTracker::isOnline PresenceTracker.cpp:17
```

Le rapport donne les deux lignes en cause. TSan ne se combine pas avec [ASan](../build/sanitizers.md), il faut donc un [preset](../build/presets.md) séparé. Le programme tourne 5 à 15 fois moins vite. Il ne voit que le code réellement exécuté : il faut donc un test qui lance plusieurs threads.

## Exemple déroulé : un test utile
```cpp
TEST(PresenceTest, ConcurrentCloseReachesZero) {
    PresenceTracker tracker;
    for (int i = 0; i < 1000; ++i) tracker.onOpen(7);
    std::vector<std::jthread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&] {
            for (int i = 0; i < 250; ++i) tracker.onClose(7);
        });
    threads.clear();                 // attend la fin des threads
    EXPECT_FALSE(tracker.isOnline(7));
}
```

Sans [mutex](mutex-contention.md), ce test échoue parfois, et TSan le signale à chaque fois.

## Erreurs fréquentes
- Croire qu'un `int` ou un `bool` est sûr parce qu'il est petit.
- Déclarer `volatile` : cela ne synchronise rien en C++.
- Protéger l'écriture, mais pas la lecture.
- Tester puis agir en deux étapes.

## À retenir
Tout état partagé et modifié doit être protégé, à la lecture comme à l'écriture. « Ça a marché en test » ne prouve rien.

## Exercice
Pourquoi TSan ne détecte-t-il pas le bug du like mutuel ?

## Corrigé
Parce qu'aucune mémoire C++ n'est partagée sans synchronisation. Chaque thread a sa connexion et ses objets. Le problème est dans l'ordre des [transactions](../sql/transactions.md) dans PostgreSQL, un processus que TSan n'observe pas. Il faut un [test d'intégration](../tests/pyramide.md) avec deux threads et une vérification du nombre de matchs.

## Voir aussi

- [Mutex et contention de verrou](mutex-contention.md)
- [Les sanitizers : ASan, UBSan, LSan](../build/sanitizers.md)
- [Les verrous consultatifs](../sql/verrous-consultatifs.md)
