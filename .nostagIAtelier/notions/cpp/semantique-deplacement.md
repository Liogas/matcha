# La sémantique de déplacement
> Déplacer un objet, c'est lui prendre ses ressources au lieu de les copier, en le laissant vide mais destructible.

## Mise en situation
`pool.acquire()` doit renvoyer une `PooledConnection`. Or ce type ne se copie pas : deux copies rendraient deux fois la même connexion au [pool](../sql/pool-connexions.md). Comment une fonction peut-elle alors renvoyer un tel objet ? Et comment le ranger dans un `Result<PooledConnection>` ?

La réponse est le déplacement : au lieu de dupliquer, on transfère la propriété.

## La théorie
### Copier ou déplacer
Une `std::string` de 1 Mo contient un pointeur vers un tampon. La copier alloue 1 Mo et recopie tout. La déplacer revient à voler le pointeur et à laisser la source vide : quelques instructions.

```text
Avant :  a -> [ tampon ]     b -> (rien)
Après b = std::move(a) :
         a -> (rien)         b -> [ tampon ]
```

### lvalue et rvalue
- Une **lvalue** a un nom et une adresse : elle continue d'exister après l'expression.
- Une **rvalue** est un [temporaire](duree-de-vie-objets.md) : personne ne s'en servira plus.

Le compilateur peut donc voler une rvalue sans risque. `T &&` est une référence sur rvalue, et elle sélectionne le constructeur de déplacement :

```cpp
T(const T &other);  // copie : other doit rester intact
T(T &&other);       // déplacement : on peut vider other
```

### std::move ne déplace rien
`std::move(x)` est une simple conversion vers `T &&`. Elle dit : « traite `x` comme un [temporaire](duree-de-vie-objets.md) ». C'est le constructeur ou l'opérateur appelé ensuite qui fait le travail. Après un déplacement, `x` existe encore, et son destructeur sera appelé. Il est dans un état valide, mais non spécifié : on peut le détruire ou lui réaffecter une valeur, pas le lire.

### La règle d'or : laisser une source inoffensive
Le destructeur de la source s'exécute toujours. Si la source croit encore posséder la ressource, elle la libère une seconde fois.

```cpp
pg::PooledConnection::PooledConnection(PooledConnection &&other) noexcept
    : _pool(other._pool), _connection(std::move(other._connection))
{
    other._pool = nullptr;
}
```

`other._pool = nullptr` est la ligne qui compte. Le destructeur teste `_pool` : pour un objet déplacé, il n'a rien à rendre. D'où le commentaire dans `Pool.hpp` : `nullptr : rien à rendre (objet déplacé)`.

### noexcept
Quand un `std::vector` grandit, il doit transférer ses éléments. Si un déplacement pouvait lever une exception, le vecteur resterait à moitié transféré. Il ne déplace donc que si le constructeur est `noexcept`, et copie sinon. Sans `noexcept`, un type copiable perd donc tout l'intérêt du déplacement.

### Règle du zéro, règle des cinq
Si tes membres gèrent leurs ressources, comme `std::string` et [`unique_ptr`](pointeurs-intelligents.md), n'écris ni destructeur, ni copie, ni déplacement : le compilateur génère ce qu'il faut. C'est la règle du zéro.

Si tu écris l'une des cinq fonctions spéciales, réfléchis aux cinq : destructeur, copie, affectation par copie, déplacement, affectation par déplacement. Par exemple, déclarer un destructeur supprime le déplacement implicite.

## Exemple déroulé
```cpp
core::Result<PooledConnection> ConnectionPool::acquire() {
    Connection c = prendreDansLaFile();      // [1]
    return PooledConnection(*this, std::move(c)); // [2]
}
auto connection = pool.acquire();            // [3]
```

1. `c` possède la connexion.
2. `std::move(c)` la transfère dans le paramètre, puis dans `_connection`. `c` devient vide. Le [temporaire](duree-de-vie-objets.md) `PooledConnection` est déplacé dans le `Result`.
3. Le `Result` arrive chez l'appelant, sans copie. À chaque étape, la source est neutralisée, et il n'y a qu'une connexion vivante.

Quand `c` meurt à la fin de la fonction, son destructeur ne ferme rien, car elle ne possède plus rien.

## Pièges
- **`return std::move(local);`** : le compilateur sait déjà traiter le retour d'une locale comme un déplacement, et il peut même construire directement l'objet chez l'appelant. `std::move` empêche cette optimisation. Écris `return local;`.
- **`const T x; auto y = std::move(x);`** : un `const` ne peut pas être vidé, donc c'est une copie silencieuse.
- **Lire un objet déplacé** : un bug qui passe souvent inaperçu en test.
- **Ajouter un membre sans mettre à jour le déplacement manuel** : il n'est pas transféré.

## Dans matcha
`Connection`, `PooledConnection`, [`Transaction`](../sql/transactions.md) et `Rows` sont déplaçables et non copiables. `Transaction &operator=(Transaction &&) = delete` interdit l'affectation : c'est plus simple que de gérer le `ROLLBACK` de la transaction écrasée.

## À retenir
Déplacer, c'est transférer la propriété. La source survit, donc elle doit devenir inoffensive.

## Exercice
Pourquoi ce code affiche-t-il parfois une chaîne vide, et comment le corriger ?

```cpp
std::string nom = "alice";
stocker(std::move(nom));
std::cout << nom;
```

## Corrigé
`stocker` a pu voler le contenu de `nom`, qui reste valide mais non spécifié. Il ne faut pas le lire. Si on a encore besoin de la valeur, on passe `nom` sans `std::move`, ce qui fait une copie, ou on affiche avant l'appel.

## Voir aussi

- [RAII](raii.md)
- [La durée de vie des objets](duree-de-vie-objets.md)
- [unique_ptr, shared_ptr, weak_ptr](pointeurs-intelligents.md)
