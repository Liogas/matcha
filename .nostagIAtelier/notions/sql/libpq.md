# libpq, l'API C de PostgreSQL
> La bibliothèque officielle qui parle au serveur PostgreSQL. cpp-pg l'enveloppe en C++.

## Mise en situation
Tu veux comprendre ce qui se passe sous `db.query(...)`, par exemple pour expliquer un `PGresult` qui fuit, ou pourquoi l'ancien `getTimestamp` ne fonctionnait jamais. Tout passe par une poignée de fonctions C.

## La théorie
### Les deux objets
- **`PGconn *`** : une connexion. Créée par `PQconnectdb`, fermée par `PQfinish`.
- **`PGresult *`** : le résultat d'une requête, stocké en mémoire côté client. Libéré par `PQclear`.

### Exécuter
```c
const char *values[1] = {"42"};
PGresult *res = PQexecParams(conn,
    "SELECT username FROM auth_accounts WHERE id = $1",
    1,        /* nombre de paramètres */
    NULL,     /* types : déduits par le serveur */
    values,   /* valeurs en texte */
    NULL,     /* longueurs : inutiles en texte */
    NULL,     /* formats : texte */
    0);       /* résultat en texte */
```

`PQexec` envoie un texte sans paramètres, mais peut contenir plusieurs instructions. `PQexecParams` n'en accepte qu'une, ce qui empêche les injections par empilement.

### Vérifier
```c
ExecStatusType status = PQresultStatus(res);
```

- `PGRES_COMMAND_OK` : réussi, sans lignes (`INSERT` sans `RETURNING`).
- `PGRES_TUPLES_OK` : réussi, avec des lignes.
- `PGRES_FATAL_ERROR` : erreur. `PQresultErrorField(res, PG_DIAG_SQLSTATE)` donne le code.

### Lire
- `PQntuples(res)` : nombre de lignes.
- `PQnfields(res)` : nombre de colonnes.
- `PQfnumber(res, "id")` : numéro d'une colonne.
- `PQgetvalue(res, ligne, colonne)` : un `char *`.
- `PQgetisnull(res, ligne, colonne)` : obligatoire, car `PQgetvalue` renvoie `""` pour `NULL`.
- `PQftype(res, colonne)` : l'OID du type.

### Le format texte
Par défaut, tout arrive en texte : `"42"`, `"t"`, `"2026-10-30 10:42:00+01"`. C'est à toi de convertir. Le format binaire existe, mais il dépend de la représentation interne.

### Les OID
Chaque type a un numéro. `TIMESTAMPOID` vaut 1114, `TIMESTAMPTZOID` 1184. L'ancien `getTimestamp` n'acceptait que le premier, alors que toutes les colonnes utilisent le second : il renvoyait toujours `nullopt`. C'est le point 4.4 de l'audit.

## Exemple déroulé : envelopper
```cpp
class Rows {
    std::shared_ptr<PGresult> _result; // PQclear à la fin
};

template <class T>
T Row::get(std::string_view column) const {
    const int index = PQfnumber(_result, std::string(column).c_str());
    return convert<T>(PQgetvalue(_result, _row, index));
}
```

Le wrapper apporte la libération automatique, l'accès par nom et des types. L'accès par nom évite le bug où ajouter une colonne décale toutes les autres, sans aucune erreur.

## Une connexion, un utilisateur
Un `PGconn` n'est pas thread-safe : deux threads ne doivent pas l'utiliser en même temps. Le [pool](pool-connexions.md) garantit l'exclusivité.

## Erreurs fréquentes
- Oublier `PQclear` : [LSan](../build/sanitizers.md) le signale.
- Garder un `char *` de `PQgetvalue` après `PQclear`.
- Oublier `PQgetisnull`.
- Passer `std::string_view::data()` sans zéro final à une fonction C qui attend une chaîne terminée : un `string_view` n'en garantit pas.

## À retenir
libpq est simple mais ne libère rien, ne vérifie rien et rend tout en texte. C'est exactement ce que cpp-pg compense.

## Exercice
Pourquoi faut-il `std::string(column).c_str()` plutôt que `column.data()` pour appeler `PQfnumber` ?

## Corrigé
`PQfnumber` attend une chaîne C terminée par `\0`. Un `std::string_view` peut désigner une partie d'une chaîne, sans terminaison à cet endroit. La copie en `std::string` garantit le zéro final. Le [temporaire](../cpp/duree-de-vie-objets.md) vit jusqu'au `;`, ce qui suffit pour l'appel.

## Voir aussi

- [Requêtes paramétrées et injection SQL](requetes-parametrees.md)
- [Les codes SQLSTATE](sqlstate.md)
- [Le pool de connexions](pool-connexions.md)
- [RAII](../cpp/raii.md)
