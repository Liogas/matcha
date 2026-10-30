# Warnings en erreur
> Un warning bloque la compilation, ce qui prouve la règle « aucun warning » du sujet.

## Mise en situation
Le sujet exige zéro warning. Sans [contrainte](../sql/contraintes.md), ils s'accumulent : 30 lignes défilent à chaque compilation, et le warning important passe inaperçu. Et comment prouver à un correcteur qu'il n'y en a aucun ?

## La théorie
### Ce que détecte un warning
Beaucoup signalent de vrais bugs : variable non initialisée, comparaison signé et non signé, `switch` incomplet, conversion qui perd des données, paramètre inutilisé.

```cpp
int size = 0;
if (size == vec.size()) ...  // -Wsign-compare
```

### Les options
- `-Wall` : les warnings courants, malgré le nom.
- `-Wextra` : d'autres encore.
- `-Wpedantic` : respect strict de la norme.
- `-Werror` : un warning devient une erreur.

### Où les mettre
`cmake/Warnings.cmake` ajoute les trois premières. `-Werror` vient du [preset](presets.md), par `CMAKE_COMPILE_WARNING_AS_ERROR`.

Pourquoi séparer ? Un nouveau compilateur ajoute de nouveaux warnings. Si une lib publiée impose `-Werror`, son consommateur ne peut plus compiler. Le contrôle reste du côté du développeur.

### Les en-têtes tiers
On ne peut pas corriger les warnings d'une dépendance. Inclus en `-isystem`, ses en-têtes sont muets. [CMake](cmake-cibles.md) le fait pour les cibles importées.

### Les exceptions
Le projet a une exception :

```cmake
$<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
```

C'est un faux positif de GCC 12 dans picojson. Toute exception doit être étroite et justifiée par un commentaire.

## Exemple déroulé
```text
error: comparison of integer expressions of different signedness [-Werror=sign-compare]
```

On ne fait pas taire le message par un cast aveugle. On se demande quel type est correct : ici `std::size_t`. Si un cast est vraiment nécessaire, `static_cast` documente l'intention.

## Erreurs fréquentes
- `#pragma GCC diagnostic ignored` partout.
- Corriger le symptôme au lieu de la cause.
- `-Werror` sur une lib publiée.

## À retenir
Un warning toléré finit par en cacher un vrai. La seule quantité stable est zéro.

## Exercice
Pourquoi un `switch` sur une `enum class` sans `default` est-il utile avec `-Wall` ?

## Corrigé
`-Wswitch` signale tout cas non traité. Si on ajoute une valeur à l'[énumération](../auth/enumeration-temps-constant.md), le compilateur indique chaque `switch` à compléter. Un `default` ferait taire ce contrôle.

## Voir aussi

- [Les presets CMake](presets.md)
- [Les sanitizers : ASan, UBSan, LSan](sanitizers.md)
