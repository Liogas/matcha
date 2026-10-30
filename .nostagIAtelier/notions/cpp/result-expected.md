# Result et std::expected
> Une fonction qui peut échouer renvoie soit une valeur, soit une erreur, et le type oblige l'appelant à s'en occuper.

## Mise en situation
Une inscription avec un email déjà pris. Avant cpp-core, chaque couche avait son propre style :

```cpp
bool insert(...);                     // false : mais pourquoi ?
std::optional<Account> find(...);     // vide : introuvable, ou base en panne ?
void update(...);                     // lève std::runtime_error
AccountCreationResult create(...);    // struct maison avec un Status
```

Le handler doit connaître quatre styles. Une panne de base et un compte introuvable se ressemblent, et une exception oubliée finit en `500` générique. Pourtant, le client mérite un `409` pour un doublon, et un `503` pour une panne.

## La théorie
### Les trois façons de signaler un échec
- **Code de retour** (`bool`, `int`) : simple, mais facile à ignorer, et sans explication.
- **Exception** : impossible à ignorer, mais invisible dans la signature, et coûteuse lorsqu'elle est levée.
- **Type somme** : la signature annonce l'échec, et l'erreur transporte une information.

### std::expected
C++23 fournit `std::expected<T, E>` : un objet contient soit un `T`, soit un `E`, jamais les deux. C'est un type somme, comme `Result` en Rust.

Dans cpp-core :

```cpp
template <class T>
using Result = std::expected<T, Error>;

struct Error { ErrorCode code; std::string message; };
```

`using` crée un alias : `core::Result<Rows>` est un `std::expected<Rows, core::Error>`.

### Utiliser un Result
```cpp
auto rows = db.query("SELECT 1");
if (!rows)                     // operator bool : y a-t-il une valeur ?
    return std::unexpected(rows.error());
use(*rows);                    // la valeur, sans vérification
use(rows.value());             // la valeur, ou une exception
```

`std::unexpected` indique qu'on construit le côté erreur. `core::fail` en est un raccourci.

### Exception ou Result ?
La règle du projet :

- **Échec attendu** (introuvable, déjà pris, base momentanément indisponible) : `Result`.
- **Bug** (mauvaise utilisation d'une API, invariant cassé) : exception ou plantage.

`pg::QueryBuilder` l'illustre : un mauvais nombre de `?` lève `std::logic_error`, alors qu'une clé de tri inconnue, qui vient d'une URL, renvoie une erreur `Invalid`.

## Exemple déroulé : la traversée des couches
Un email en double, de la base au client :

1. PostgreSQL renvoie `23505`.
2. `pg::errorCodeFor` donne `ErrorCode::Conflict`.
3. `execute` renvoie un `Result` en erreur.
4. Le [repository](../archi/couche-services.md) le propage :

```cpp
auto rows = _db.query("INSERT ...", email);
if (!rows)
    return std::unexpected(rows.error());
```

5. Le service le propage aussi, sans savoir ce qu'est le HTTP.
6. L'[adaptateur](../archi/lib-coeur-adaptateur.md) traduit à un seul endroit :

```text
Invalid      -> 400
Unauthorized -> 401
Forbidden    -> 403
NotFound     -> 404
Conflict     -> 409
Unavailable  -> 503
Internal     -> 500
```

Chaque couche n'a qu'une décision à prendre : traiter l'erreur, ou la transmettre.

### Ajouter du contexte
Pour une erreur `Internal`, le message d'origine peut contenir du SQL. On le journalise, mais on ne le renvoie pas au client. La réponse reste générique.

### `Result<void>`
Une fonction sans valeur utile renvoie `core::Result<void>`. `commit()` en est un exemple : on teste seulement la réussite.

## Erreurs fréquentes
- Écrire `*result` sans tester : comportement indéfini si c'est une erreur.
- Écrire `if (result)` en croyant tester la valeur contenue : pour un `Result<bool>`, cela teste la [présence](../temps-reel/presence.md) d'une valeur, pas le booléen. Il faut `*result`.
- Avaler l'erreur et renvoyer une valeur par défaut.
- Copier un `Result<Rows>` : il contient une ressource. Déplace-le.

## À retenir
La signature dit la vérité : si une fonction peut échouer, son type de retour le montre, et l'erreur remonte jusqu'à l'unique endroit qui parle HTTP.

## Exercice
Pourquoi un [JWKS](../auth/jwks.md) injoignable doit-il donner `Unavailable` plutôt qu'`Unauthorized` ? Que voit l'utilisateur dans chaque cas ?

## Corrigé
`Unauthorized` signifie « ton jeton n'est pas valide ». Le front en déduit que la session est finie et déconnecte l'utilisateur, à cause d'une panne qui n'a rien à voir avec lui. `Unavailable` donne `503`, qui signifie « réessaie » : la session reste ouverte. C'est le point 4.8 de l'audit.

## Voir aussi

- [Les codes SQLSTATE](../sql/sqlstate.md)
- [HTTP : méthodes et codes de statut](../web/http-bases.md)
- [La couche services](../archi/couche-services.md)
