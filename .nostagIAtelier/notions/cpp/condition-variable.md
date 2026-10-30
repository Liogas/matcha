# std::condition_variable
> Permet à un thread de dormir jusqu'à ce qu'un autre lui signale qu'une condition est peut-être devenue vraie.

## Mise en situation
Le [pool](../sql/pool-connexions.md) a 4 connexions, toutes prises. Une cinquième requête arrive.

- Si elle échoue tout de suite, on renvoie des erreurs alors qu'une connexion se libérera 3 ms plus tard.
- Si elle boucle en testant sans arrêt, elle consomme un cœur à ne rien faire.
- Si elle fait `sleep(10ms)`, elle attend parfois pour rien, ou réagit tard.

Il faut un moyen de dormir, et d'être réveillé au bon moment.

## La théorie
### Les trois ingrédients
Une attente correcte demande trois choses :

- une **condition** sur un état partagé : `!_idle.empty()` ;
- un [mutex](mutex-contention.md) qui protège cet état ;
- une `condition_variable` qui sert de sonnette.

La variable de condition ne contient aucune donnée. Elle ne sait pas ce que tu attends. C'est seulement un mécanisme de réveil.

### Ce que fait wait
```cpp
std::unique_lock lock(_mutex);
_available.wait(lock, [this] { return !_idle.empty(); });
```

Équivalent à :

```cpp
while (_idle.empty()) {
    // relâche le mutex et s'endort, de manière atomique
    // au réveil, reprend le mutex
}
```

Il y a quatre points clés :

1. **Le [mutex](mutex-contention.md) est relâché pendant l'attente**, sinon personne ne pourrait rendre de connexion. C'est pour cela qu'il faut un `unique_lock`, et pas un `lock_guard`.
2. **Le relâchement et l'endormissement sont atomiques** : aucune notification ne peut tomber entre les deux.
3. **Le mutex est repris au réveil**, donc le test se fait sous protection.
4. **La condition est retestée** à chaque réveil.

### Pourquoi retester
- **Le réveil intempestif** (spurious wakeup) : un thread peut se réveiller sans notification, c'est permis par la norme.
- **Le vol** : entre `notify_one` et le réveil, un autre thread peut avoir pris la connexion.

La forme avec prédicat fait la boucle pour toi : utilise-la toujours.

### L'attente limitée
```cpp
bool ok = _available.wait_for(lock, _options.acquireTimeout, [this] {
    return !_idle.empty();
});
```

`wait_for` renvoie la dernière valeur du prédicat : `false` signifie que le délai a expiré.

## Exemple déroulé : emprunter et rendre
Côté emprunt :

```cpp
core::Result<PooledConnection> ConnectionPool::acquire() {
    std::unique_lock lock(_mutex);
    if (!_available.wait_for(lock, _options.acquireTimeout,
            [this] { return !_idle.empty(); }))
        return core::fail(core::ErrorCode::Unavailable, "connection pool exhausted");
    Connection connection = std::move(_idle.back());
    _idle.pop_back();
    lock.unlock();
    return PooledConnection(*this, std::move(connection));
}
```

Côté retour :

```cpp
{
    std::lock_guard lock(_mutex);
    _idle.push_back(std::move(connection));
}
_available.notify_one();
```

Scénario avec un [pool](../sql/pool-connexions.md) plein :

```text
Thread E : lock, _idle vide, relâche le mutex, dort
Thread A : finit, lock, push, unlock, notify_one
Thread E : réveil, reprend le mutex, _idle non vide, prend la connexion
```

Si personne ne rend de connexion en 2 secondes, E renvoie `Unavailable`, puis un `503`. Sans délai, une fuite de connexion bloquerait tous les threads de Crow, sans aucun message.

## notify_one ou notify_all
Une connexion libérée ne satisfait qu'un thread : `notify_one`. `notify_all` les réveillerait tous, pour que la plupart se rendorment. Il sert quand l'événement concerne tout le monde, comme la fermeture du [pool](../sql/pool-connexions.md).

## Erreurs fréquentes
- Modifier la condition hors du [mutex](mutex-contention.md) : la notification peut être perdue.
- Utiliser `wait` sans prédicat.
- Oublier `notify` dans un chemin d'erreur.
- Détruire la condition pendant qu'un thread attend.

## À retenir
La variable de condition est une sonnette. La vérité est dans la condition protégée par le [mutex](mutex-contention.md), et c'est elle qu'on reteste toujours.

## Exercice
Pourquoi le test de [pool](../sql/pool-connexions.md) épuisé doit-il durer environ `acquireTimeout`, et comment vérifier que le thread n'a pas consommé de CPU ?

## Corrigé
Le test emprunte toutes les connexions, puis appelle `acquire()`. Le thread dort jusqu'à l'expiration du délai, donc la durée mesurée doit être proche du délai, avec une petite marge. Un thread endormi n'exécute rien : le temps CPU du processus reste presque nul, ce que `getrusage` ou `time` permet de vérifier. Une boucle active consommerait un cœur complet.

## Voir aussi

- [Mutex et contention de verrou](mutex-contention.md)
- [Le pool de connexions](../sql/pool-connexions.md)
