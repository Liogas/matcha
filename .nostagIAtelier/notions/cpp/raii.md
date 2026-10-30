# RAII
> Une ressource est liée à la vie d'un objet : le constructeur l'acquiert, le destructeur la libère, quoi qu'il arrive.

## Mise en situation
Voici un like écrit sans précaution :

```cpp
PGconn *c = pool.take();
exec(c, "BEGIN");
if (!insererLike(c)) {
    exec(c, "ROLLBACK");
    pool.giveBack(c);
    return false;
}
insererMatch(c);          // peut lever une exception
exec(c, "COMMIT");
pool.giveBack(c);
return true;
```

Il y a déjà deux copies du nettoyage, et un chemin oublié : si `insererMatch` lève une exception, la [transaction](../sql/transactions.md) reste ouverte et la connexion n'est jamais rendue. Après quatre incidents, le [pool](../sql/pool-connexions.md) de quatre connexions est vide. Le serveur attend, puis renvoie des erreurs à tout le monde, sans aucun message clair.

## La théorie
### Qu'est-ce qu'une ressource
Une ressource est quelque chose qu'on prend et qu'il faut rendre : mémoire, connexion, verrou, fichier, [transaction](../sql/transactions.md). Le problème n'est pas de la prendre, mais de garantir qu'elle sera rendue sur tous les chemins.

### La garantie du langage
C++ garantit qu'à la sortie d'un bloc, par fin normale, `return`, `break` ou exception, les destructeurs des objets locaux sont appelés. Cette garantie s'appelle le déroulement de pile, ou stack unwinding.

RAII, Resource Acquisition Is Initialization, utilise cette garantie :

- acquérir la ressource dans le constructeur ;
- la libérer dans le destructeur ;
- placer l'objet sur la pile.

Le nettoyage n'est écrit qu'une fois, et il ne peut pas être oublié.

```cpp
{
    std::lock_guard lock(_mutex);  // verrouille
    if (erreur) return;            // déverrouille
    faireQuelqueChose();           // exception : déverrouille
}                                  // déverrouille
```

### Pourquoi un destructeur ne doit pas lever d'exception
Pendant le déroulement de pile, une exception est déjà en cours. Une seconde exception sortant d'un destructeur provoque `std::terminate` : le programme s'arrête. Depuis C++11, les destructeurs sont d'ailleurs `noexcept` par défaut. Dans `~Transaction`, le [`ROLLBACK`](../sql/transactions.md) peut échouer : on journalise et on continue.

### Non copiable, déplaçable
Si un objet RAII était copié, les deux copies libéreraient la même ressource. On interdit donc la copie, et on autorise le [déplacement](semantique-deplacement.md), qui transfère la propriété :

```cpp
PooledConnection(const PooledConnection &) = delete;
PooledConnection(PooledConnection &&other) noexcept;
```

### Le destructeur décide selon l'état
`~Transaction` n'annule pas systématiquement. Il regarde `_open` :

```cpp
pg::Transaction::~Transaction() {
    if (_open)
        rollbackQuietly();
}
```

`commit()` met `_open` à `false`. Le destructeur n'a alors plus rien à faire.

## Exemple déroulé
```cpp
core::Result<void> service(pg::ConnectionPool &pool) {
    auto tx = pool.begin();              // [1]
    if (!tx) return std::unexpected(tx.error());
    auto a = tx->query("INSERT ...");    // [2]
    if (!a) return std::unexpected(a.error()); // [3]
    return tx->commit();                 // [4]
}                                        // [5]
```

Si tout va bien :
1. `[1]` emprunte une connexion et envoie `BEGIN`.
2. `[4]` envoie [`COMMIT`](../sql/transactions.md), et `_open` passe à `false`.
3. `[5]` : `tx` meurt. Le destructeur ne fait rien, puis `PooledConnection` meurt et rend la connexion.

Si `[2]` échoue :
1. `[3]` retourne.
2. `tx` meurt avec `_open` à `true` : `ROLLBACK`.
3. La connexion revient au [pool](../sql/pool-connexions.md).

Le même nettoyage s'exécute en cas d'exception, sans ligne supplémentaire.

## Dans matcha
- `pg::PooledConnection` rend la connexion ;
- `pg::Transaction` annule ;
- [`unique_ptr`](pointeurs-intelligents.md) avec un deleter `PQfinish` ferme un [`PGconn`](../sql/libpq.md) ;
- `Rows` libère un `PGresult` avec `PQclear` ;
- [`std::lock_guard`](mutex-contention.md) libère un mutex.

## Erreurs fréquentes
- **Objet sans nom** : `std::lock_guard<std::mutex>{m};` verrouille et déverrouille au même `;`, car c'est un [temporaire](duree-de-vie-objets.md).
- **Oublier de neutraliser la source** d'un [déplacement](semantique-deplacement.md) : double libération.
- **Lever une exception dans un destructeur**.
- **Détenir une ressource trop longtemps** : un verrou tenu jusqu'au `}` englobe peut-être un appel lent. Ajoute un bloc `{ }` pour raccourcir sa vie.

## À retenir
Tu n'écris pas « je dois penser à libérer » : tu écris un type dont la mort libère. La correction du code ne dépend plus de ta mémoire.

## Exercice
Dans ce code, où s'exécute le [`ROLLBACK`](../sql/transactions.md), et le [mutex](mutex-contention.md) est-il encore tenu à ce moment ?

```cpp
{
    auto tx = pool.begin();
    std::lock_guard lock(_mutex);
    throw std::runtime_error("boom");
}
```

## Corrigé
L'ordre de destruction est l'inverse de la création : `lock` meurt d'abord, et libère le [mutex](mutex-contention.md). Puis `tx` meurt et exécute le [`ROLLBACK`](../sql/transactions.md). Le mutex n'est donc plus tenu pendant le `ROLLBACK`. Si les déclarations étaient inversées, le `ROLLBACK` s'exécuterait sous verrou, ce qu'on veut éviter.

## Voir aussi

- [La durée de vie des objets](duree-de-vie-objets.md)
- [La sémantique de déplacement](semantique-deplacement.md)
- [Les transactions et ACID](../sql/transactions.md)
- [Le pool de connexions](../sql/pool-connexions.md)
