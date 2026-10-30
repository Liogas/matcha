# Les codes SQLSTATE
> Un code standard à cinq caractères qui dit pourquoi une requête a échoué, sans analyser le texte de l'erreur.

## Mise en situation
Deux personnes s'inscrivent avec le même email. L'[index](index.md) unique en refuse une. Le client doit recevoir `409`, avec un message utile, et non `500`. Chercher le mot `duplicate` dans le message serait fragile : le texte dépend de `lc_messages` et peut changer.

## La théorie
Un code SQLSTATE comporte 5 caractères. Les deux premiers forment la classe, les trois suivants précisent :

- **Classe 23, violation de [contrainte](contraintes.md)** : `23505` unicité, `23503` clé étrangère, `23514` `CHECK`, `23502` `NOT NULL`.
- **Classe 22, donnée invalide** : `22001` texte trop long, `22P02` conversion impossible, comme `abc` en entier.
- **Classe 40, annulation** : `40001` échec de sérialisation, `40P01` [deadlock](deadlock.md).
- **Classe 08, connexion** : problème de réseau.
- **Classe 53, ressources** : `53300` trop de connexions.
- **Classe 57** : `57P01` arrêt du serveur.
- **Classe 25** : `25P02` [transaction](transactions.md) déjà en échec.
- **Classe 42, syntaxe** : `42P01` table inconnue, `42601` erreur de syntaxe.

## Exemple déroulé
`pg::errorCodeFor` organise ces codes selon ce que l'appelant peut faire :

```cpp
if (sqlState == "23505") return core::ErrorCode::Conflict;
if (sqlState.starts_with("23") || sqlState.starts_with("22"))
    return core::ErrorCode::Invalid;
if (sqlState == "40001" || sqlState == "40P01" || sqlState.starts_with("08")
    || sqlState == "53300" || sqlState == "57P01")
    return core::ErrorCode::Unavailable;
return core::ErrorCode::Internal;
```

- `Conflict` : la ressource existe déjà, `409`.
- `Invalid` : la donnée ne convient pas, `400`.
- `Unavailable` : réessaie plus tard, `503`.
- `Internal` : une erreur de syntaxe est un bug de ton code, `500` et une trace.

### Laisser la base trancher
```cpp
// Mauvais
if (accounts.exists(email)) return conflit();
accounts.insert(email);

// Bon
auto rows = db.query("INSERT ...", email);
if (!rows && rows.error().code == core::ErrorCode::Conflict) ...
```

Le premier code a une fenêtre entre le test et l'insertion, où une autre requête peut passer. Le second est correct, car seule la [contrainte](contraintes.md) en base est atomique.

### Quelle contrainte a échoué ?
Une inscription peut échouer sur l'email ou sur le nom d'utilisateur. `PQresultErrorField(res, PG_DIAG_CONSTRAINT_NAME)` donne le nom de la [contrainte](contraintes.md), ce qui permet un message précis.

## Erreurs fréquentes
- Analyser le texte de l'erreur.
- Renvoyer le message SQL au client.
- Traiter `40001` comme un bug.

## À retenir
SQLSTATE est le contrat stable entre PostgreSQL et ton code. Traduis-le une fois, en catégories d'action.

## Exercice
Une insertion de photo avec `position = 6` échoue. Quel code obtiens-tu, et quel statut HTTP ?

## Corrigé
La colonne a `CHECK (position BETWEEN 1 AND 5)`, donc `23514`. `errorCodeFor` donne `Invalid`, soit `400`. Le service devrait valider avant pour un meilleur message, mais la [contrainte](contraintes.md) garde le dernier mot.

## Voir aussi

- [Result et std::expected](../cpp/result-expected.md)
- [Les contraintes d'intégrité](contraintes.md)
- [Les niveaux d'isolation](isolation.md)
