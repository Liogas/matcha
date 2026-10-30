# La durée de vie des objets
> Un objet C++ naît et meurt à des moments précis. Un pointeur ou une référence ne le garde jamais en vie.

## Mise en situation
Tu écris une petite fonction qui renvoie le prénom d'un compte :

```cpp
std::string_view prenom(pg::Rows &rows) {
    std::string nom = rows[0].get<std::string>("first_name");
    return nom;
}
```

Tout compile. En test, ça affiche parfois le bon prénom. En production, le texte devient du charabia. [ASan](../build/sanitizers.md) finit par s'arrêter sur `stack-use-after-return`. Pour comprendre, il faut savoir exactement quand `nom` meurt, et ce que `string_view` est en réalité.

## La théorie
### Objet et stockage
Un objet occupe de la mémoire pendant sa durée de vie. Sa vie commence à la fin de son constructeur et s'arrête au début de son destructeur. Lire un objet en dehors de cette période est un comportement indéfini : le programme peut sembler marcher, puis casser n'importe quand.

### Les quatre durées de vie
- **Automatique** : variable locale, détruite au `}` du bloc qui la déclare.
- **Temporaire** : objet sans nom, détruit au `;` de l'instruction complète qui l'a créé.
- **Dynamique** : créé par `new`, ou possédé par un [pointeur intelligent](pointeurs-intelligents.md), détruit quand son propriétaire le libère.
- **Statique** : variable globale ou `static`, détruite à la fin du programme.

### L'ordre de destruction
Dans un bloc, les objets meurent dans l'ordre inverse de leur création. Dans une classe, les membres meurent dans l'ordre inverse de leur déclaration. C'est pour cela que `main.cpp` doit déclarer le [pool](../sql/pool-connexions.md) avant le serveur, et que `App.hpp` précise que les vues gardent des références sur ce qui les précède.

### Posséder ou observer
C'est la distinction la plus utile en C++ :

- `std::string`, `std::vector`, [`unique_ptr`](pointeurs-intelligents.md) **possèdent** : ils libèrent la mémoire.
- `T *`, `T &`, `std::string_view`, `std::span` **observent** : ils pointent vers quelque chose qui vit ailleurs.

Un observateur ne prolonge jamais la vie de ce qu'il regarde. Quand tu vois un observateur, pose la question : qui possède, et vit-il assez longtemps ?

## Exemple déroulé : le string_view
Reprenons `prenom` ligne par ligne.

```cpp
std::string nom = rows[0].get<std::string>("first_name");
```

`nom` est une variable locale. Il possède un tampon qui contient `Alice`.

```cpp
return nom;
```

La fonction renvoie un `string_view` : un pointeur et une taille, qui désignent le tampon de `nom`.

```cpp
}
```

`nom` meurt, et son tampon est rendu. Le `string_view` renvoyé contient toujours l'adresse, mais elle ne désigne plus rien. C'est un pointeur pendant, dit dangling.

La correction consiste à renvoyer un possesseur : `std::string prenom(...)`.

## Exemple déroulé : le temporaire et le pool
```cpp
const std::string &ref = pool.acquire()->name();
std::cout << ref;
```

Déroulons la première ligne, sans raccourci :

1. `pool.acquire()` crée un temporaire `PooledConnection`, qui détient une connexion.
2. `->name()` renvoie une référence vers un champ de cette connexion.
3. `ref` se lie à ce champ.
4. Au `;`, le temporaire meurt : son destructeur rend la connexion au [pool](../sql/pool-connexions.md).

À la ligne suivante, `ref` désigne un champ d'une connexion peut-être prêtée à un autre thread. La correction consiste à donner un nom :

```cpp
auto connection = pool.acquire();     // vit jusqu'au }
const std::string &ref = connection->name();
```

### La prolongation de vie, et sa limite
Une référence `const` liée **directement** à un temporaire prolonge sa vie jusqu'à la fin du bloc :

```cpp
const std::string &s = fabriquer();  // sûr
```

Mais elle ne s'applique pas à une référence renvoyée par une méthode de ce temporaire. Le compilateur voit seulement « une référence renvoyée », il ne sait pas qu'elle pointe dans le temporaire. C'est pourquoi l'exemple du [pool](../sql/pool-connexions.md) est faux.

### Le piège du for sur un temporaire
```cpp
for (const auto &row : pool.query("SELECT 1").value()) { ... }
```

Avant C++23, le temporaire renvoyé par `query` mourait avant la boucle. Depuis C++23, la vie des temporaires de l'expression est prolongée pendant toute la boucle. Comme tous les compilateurs ne suivent pas encore cette règle, donne un nom au résultat.

## La méthode de lecture
À chaque `;` et à chaque `}` :

1. Quels objets meurent ici ?
2. Qui pointe encore vers eux ?

Ces deux questions suffisent pour la grande majorité de ces bugs.

## Dans matcha
- `Rows` possède le [`PGresult`](../sql/libpq.md), et `Row` l'observe : ne garde jamais un `Row` après la mort de ses `Rows`.
- `PQgetvalue` renvoie un pointeur à l'intérieur du `PGresult`, donc il meurt avec lui.
- Les routes Crow capturent `[&]` : c'est sûr ici, car tout ce qui est capturé vit dans `main` jusqu'à l'arrêt du serveur.
- `Executor::query` prend un `string_view` : la chaîne n'a besoin de vivre que pendant l'appel.

## Erreurs fréquentes
- Renvoyer une référence ou un `string_view` vers une locale.
- Stocker un `string_view` dans un objet qui survit à la chaîne.
- Garder un itérateur après avoir modifié un `vector`.
- Capturer `[&]` dans une lambda exécutée plus tard, sur un autre thread.

## À retenir
La question n'est jamais « est-ce que ça compile ? » mais « qui possède, et jusqu'à quand ? ».

## Exercice
Que se passe-t-il ici ?

```cpp
auto &row = pool.query("SELECT 1").value()[0];
std::cout << row.get<int>(0);
```

## Corrigé
1. `pool.query(...)` crée un temporaire `Result<Rows>`.
2. `.value()` renvoie une référence vers le `Rows` qu'il contient.
3. `[0]` donne une `Row`, et `row` s'y lie.
4. Au `;`, le temporaire meurt avec ses `Rows`, et le [`PGresult`](../sql/libpq.md) est libéré.

`row` est pendante, et la ligne suivante est un comportement indéfini. Correction : `auto rows = pool.query(...);` puis `rows->...`.

## Voir aussi

- [RAII](raii.md)
- [unique_ptr, shared_ptr, weak_ptr](pointeurs-intelligents.md)
- [La sémantique de déplacement](semantique-deplacement.md)
- [Les sanitizers : ASan, UBSan, LSan](../build/sanitizers.md)
