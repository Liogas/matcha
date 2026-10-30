# Mutex et contention de verrou
> Un mutex laisse passer un thread à la fois. Plus il est tenu longtemps, plus les autres attendent.

## Mise en situation
Crow traite les requêtes sur plusieurs threads. Le hub actuel partage une table de connexions, protégée par un unique mutex :

```cpp
void MessagingWebSocketHub::sendToAccount(std::int64_t id, const std::string &payload) {
    std::lock_guard lock(_mutex);
    const auto it = _connections.find(id);
    if (it == _connections.end())
        return;
    for (auto *connection : it->second)
        connection->send_text(payload);
}
```

Tous les envois, toutes les ouvertures et toutes les fermetures de [WebSocket](../temps-reel/websocket.md) passent par ce même verrou. Chaque envoi retarde tous les autres utilisateurs. Plus il y a de connexions, plus l'attente grandit.

## La théorie
### Le problème à résoudre
Deux threads qui modifient une `unordered_map` en même temps peuvent la corrompre. Un mutex, pour exclusion mutuelle, garantit qu'un seul thread exécute une portion de code à la fois. Cette portion s'appelle la section critique.

```cpp
_mutex.lock();
// section critique
_mutex.unlock();
```

On n'écrit jamais ces appels à la main : une exception oublierait `unlock`. On utilise le [RAII](raii.md) :

- `std::lock_guard` : verrouille jusqu'au `}` ;
- `std::unique_lock` : peut être relâché avant, et il est requis par [condition_variable](condition-variable.md) ;
- `std::scoped_lock` : prend plusieurs mutex sans risque de [deadlock](../sql/deadlock.md).

### La contention
Quand plusieurs threads veulent le même verrou, ils attendent. Dans cette zone, le programme est séquentiel, quel que soit le nombre de cœurs. Selon la loi d'Amdahl, si 10 % du travail est séquentiel, le gain maximal est de 10, même avec 1000 cœurs. Réduire la section critique est donc souvent plus efficace qu'ajouter des threads.

### La règle
Sous un verrou, on ne manipule que de la mémoire, et rapidement. Jamais de réseau, de disque, de SQL ni de journalisation lente.

## Exemple déroulé : copier sous verrou, envoyer hors verrou
```cpp
void Hub::publish(const std::string &topic, const std::string &payload) {
    std::vector<std::weak_ptr<ISession>> cibles;
    {
        std::lock_guard lock(_mutex);      // [1]
        const auto it = _topics.find(topic);
        if (it != _topics.end())
            cibles = it->second;           // [2]
    }                                      // [3]
    for (auto &faible : cibles)            // [4]
        if (auto session = faible.lock())
            session->send(payload);
}
```

1. Verrou pris.
2. Copie d'une petite liste, en quelques microsecondes.
3. Le bloc `{ }` détruit `lock` : verrou libéré. Sans ce bloc, le verrou serait tenu jusqu'à la fin de la fonction, et tout le bénéfice serait perdu.
4. L'envoi se fait sans verrou. Si une session est lente, seule cette boucle ralentit.

[`weak_ptr`](pointeurs-intelligents.md) règle le dernier problème : une session peut mourir pendant l'envoi, voir les pointeurs intelligents.

## Autre exemple : l'ancien Database
`Database` protégeait une connexion unique par un mutex :

```text
Thread A : lock, BEGIN, unlock
Thread B : lock, UPDATE (dans la transaction de A !), unlock
Thread A : lock, INSERT, unlock
```

Le mutex protégeait chaque appel, pas la séquence. C'est le défaut classique : la protection doit couvrir toute l'opération logique. Le [pool](../sql/pool-connexions.md) règle ce problème : un emprunteur est seul sur sa connexion.

## Dans matcha
`StreamLogger` formate hors verrou et écrit sous verrou, avec la même logique. Le [pool](../sql/pool-connexions.md) utilise un mutex seulement pour manipuler sa liste de connexions libres, jamais pendant une requête.

## Erreurs fréquentes
- **Callback sous verrou** : tu ne sais pas ce qu'il fait, et s'il reprend le même mutex, le thread se bloque lui-même.
- **Lire sans verrou** ce qui est écrit sous verrou : c'est toujours un [data race](data-race.md).
- **Tester puis agir en deux sections critiques** : l'état peut changer entre les deux.
- **Un seul mutex pour tout** : plusieurs mutex plus fins réduisent la contention, mais augmentent le risque de [deadlock](../sql/deadlock.md).

## À retenir
Un verrou doit protéger une petite opération sur la mémoire. Si tu fais une entrée-sortie sous verrou, c'est presque toujours un bug de conception.

## Exercice
Ce code protège-t-il correctement le compteur ?

```cpp
if (_compteurs.contains(id)) {           // sans verrou
    std::lock_guard lock(_mutex);
    _compteurs[id] -= 1;
}
```

## Corrigé
Non. `contains` lit la table sans verrou pendant qu'un autre thread peut l'écrire : c'est un [data race](data-race.md). Le test et l'action doivent aussi être faits ensemble. Il faut prendre le verrou avant le `if`.

## Voir aussi

- [Data race et ThreadSanitizer](data-race.md)
- [std::condition_variable](condition-variable.md)
- [Le deadlock](../sql/deadlock.md)
- [Le pool de connexions](../sql/pool-connexions.md)
