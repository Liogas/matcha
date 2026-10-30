# Compatibilité d'orientation
> Deux profils ne sont compatibles que si chacun cherche le genre de l'autre.

## Mise en situation
Le filtre le plus simple regarde ce que je cherche. Mais une femme lesbienne verrait alors dans ses suggestions... d'autres femmes hétérosexuelles qui ne s'intéressent pas à elle. Le sujet demande des profils intéressants. La règle est symétrique.

## La théorie
### Une relation à deux sens
```text
cherche(A, B) : le genre de B est dans ce que cherche A
compatibles(A, B) = cherche(A, B) ET cherche(B, A)
```

### Les valeurs
Le sujet demande de gérer l'hétérosexualité et la bisexualité, avec un défaut bisexuel si rien n'est renseigné. Le comportement dépend des valeurs retenues : à décider, documenter et encoder en énumérations.

```cpp
enum class Gender { Man, Woman, NonBinary };
enum class Orientation { Heterosexual, Homosexual, Bisexual };
```

### Une table de vérité
Il y a peu de combinaisons, on peut toutes les tester :

```text
Homme hétéro   cherche Femme
Femme hétéro   cherche Homme
Homme homo     cherche Homme
Femme homo     cherche Femme
Bisexuel       cherche Homme, Femme
```

## Exemple déroulé
```cpp
bool seeks(Orientation orientation, Gender self, Gender other) {
    switch (orientation) {
    case Orientation::Bisexual:     return true;
    case Orientation::Heterosexual: return other != self;
    case Orientation::Homosexual:   return other == self;
    }
    return false;
}

bool compatible(const Person &a, const Person &b) {
    return seeks(a.orientation, a.gender, b.gender)
        && seeks(b.orientation, b.gender, a.gender);
}
```

Alice (femme, homo) et Bob (homme, hétéro) :

```text
seeks(Alice -> Bob) : homo, autre genre : non
```

Ils ne sont pas compatibles, sans évaluer le second sens. Le cas non binaire demande une décision explicite : la version ci-dessus le rend compatible avec tout hétérosexuel, ce qui n'est pas forcément voulu.

### Tester
Un test paramétré couvre toute la table. Ajouter une valeur à l'[énumération](../auth/enumeration-temps-constant.md) oblige à décider de chaque cas.

### Dans la requête SQL
Le filtre doit être fait en SQL. Filtrer en C++ après `LIMIT 20` donnerait des pages presque vides. La règle existe donc à deux endroits. On peut soit générer le fragment SQL depuis la table, soit vérifier par un test que le C++ et le SQL donnent les mêmes résultats sur toutes les combinaisons.

## Erreurs fréquentes
- Ne regarder qu'un seul sens.
- Oublier le défaut bisexuel.
- Traiter `NULL` comme non compatible.

## À retenir
Une règle de compatibilité est une fonction symétrique, petite et testable sur tous ses cas.

## Exercice
Pourquoi ne pas filtrer en C++ après une requête paginée ?

## Corrigé
La [pagination](../sql/pagination-keyset.md) s'applique avant le filtre. Sur 20 lignes reçues, beaucoup seraient retirées, et certaines pages seraient presque vides. Le filtre doit s'appliquer avant le `LIMIT`, donc dans la requête.

## Voir aussi

- [Le score de matching](score-matching.md)
- [GoogleTest : fixtures et tests paramétrés](../tests/gtest.md)
- [Un QueryBuilder sûr](../sql/query-builder.md)
