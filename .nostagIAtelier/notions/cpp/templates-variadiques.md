# Templates variadiques et fold expressions
> Un template variadique accepte un nombre quelconque d'arguments de types différents, et une fold expression applique une opération à chacun.

## Mise en situation
L'ancien code convertissait chaque paramètre à la main :

```cpp
db.execute(sql, { std::to_string(id), nom, age ? std::to_string(*age) : "" });
```

Trois problèmes : la conversion est répétée partout, une valeur `NULL` n'a pas de bonne écriture, et rien ne vérifie les types. On voudrait écrire simplement :

```cpp
db.query(sql, id, nom, age);
```

avec n'importe quel nombre d'arguments, de types différents.

## La théorie
### Un template
Un template est un modèle de code. Le compilateur le recopie pour chaque type utilisé :

```cpp
template <class T> T max2(T a, T b) { return a > b ? a : b; }
max2(1, 2);       // génère max2<int>
max2(1.0, 2.0);   // génère max2<double>
```

Il n'y a aucun coût à l'exécution, car tout est résolu à la compilation. En contrepartie, le code doit être visible par ses utilisateurs : les templates vivent dans les en-têtes.

### Le paquet de paramètres
```cpp
template <class... Args>
core::Result<Rows> query(std::string_view sql, const Args &...args);
```

- `class... Args` déclare un paquet de types.
- `const Args &...args` déclare un paquet de paramètres.
- `args...` déroule le paquet.
- `sizeof...(Args)` donne le nombre d'éléments, à la compilation.

Pour `query(sql, id, nom)`, le compilateur génère :

```cpp
query<std::int64_t, std::string>(std::string_view sql,
    const std::int64_t &, const std::string &);
```

### La fold expression
Avant C++17, il fallait une récursion. Le fold applique un opérateur à tout le paquet :

```cpp
(result.add(args), ...);
```

est déroulé en :

```cpp
result.add(a), result.add(b), result.add(c);
```

L'opérateur virgule garantit l'ordre, de gauche à droite. Les parenthèses extérieures sont obligatoires. Il existe d'autres formes : `(args + ...)` fait une somme.

### Choisir selon le type
Dans `Params::add`, le comportement dépend du type :

```cpp
template <class T>
void add(const T &value) {
    if constexpr (is_optional<T>) {
        if (value) add(*value); else addNull();
    } else if constexpr (std::is_same_v<T, bool>) {
        addText(value ? "t" : "f");
    } else {
        addText(toText(value));
    }
}
```

`if constexpr` choisit la branche à la compilation : les autres branches ne sont même pas compilées pour ce type. Un simple `if` exigerait que toutes les branches compilent pour tous les types.

## Exemple déroulé
```cpp
std::optional<int> age;   // vide
db.query("UPDATE p SET name = $1, age = $2 WHERE id = $3", "Alice", age, 7);
```

1. `Args` vaut `char[6], std::optional<int>, int`.
2. `query` appelle `pg::params("Alice", age, 7)`.
3. Le fold appelle `add` trois fois, dans l'ordre.
4. `Params` contient `"Alice"`, `NULL`, `"7"`.
5. `execute`, qui est virtuelle, reçoit le tout.

### Pourquoi deux niveaux
`query` est un template et `execute` une fonction virtuelle. Une fonction template ne peut pas être virtuelle : la table des fonctions virtuelles a une taille fixe, alors qu'un template peut générer un nombre illimité de versions. Le template convertit donc les valeurs, puis passe la main à une fonction non template.

## Erreurs fréquentes
- Mettre la définition d'un template dans un `.cpp` : erreur à l'édition de liens.
- Passer un type non géré : le message d'erreur est long, cherche la ligne `required from here`, qui désigne ton appel.
- Oublier que `"texte"` est un tableau de `char`, pas une `std::string`.

## À retenir
Le template donne une API simple et un contrôle à la compilation : les conversions sont écrites une fois, et le compilateur génère le reste.

## Exercice
Écris une fonction `somme` qui additionne n'importe quel nombre d'arguments. Que renvoie `somme(1, 2, 3)` ?

## Corrigé
```cpp
template <class... Args>
auto somme(Args... args) { return (args + ...); }
```

`somme(1, 2, 3)` se déroule en `1 + (2 + 3)` et renvoie 6.

## Voir aussi

- [Interfaces et injection de dépendances](interfaces-injection.md)
- [Requêtes paramétrées et injection SQL](../sql/requetes-parametrees.md)
- [libpq, l'API C de PostgreSQL](../sql/libpq.md)
