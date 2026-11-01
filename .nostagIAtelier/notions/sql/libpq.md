# libpq, l'API C de PostgreSQL
> La bibliothèque officielle qui parle au serveur PostgreSQL. cpp-pg l'enveloppe en C++.

## Mise en situation
Tu veux comprendre ce qui se passe sous `db.query(...)`, par exemple pour expliquer un `PGresult` qui fuit, ou pourquoi l'ancien `getTimestamp` ne fonctionnait jamais. Tout passe par une poignée de fonctions C.

### Ce qui se passe vraiment sous `db.query`
Prends une requête banale : afficher le pseudo du compte 42. Côté [repository](../archi/couche-services.md), une ligne. Côté libpq, quatre choses doivent se passer, dans cet ordre, et chacune peut échouer. Si tu oublies la dernière, ton serveur perd un peu de mémoire à chaque requête. Après 100 000 requêtes, le conteneur grossit sans que rien ne plante, et le bug devient très difficile à relier à sa cause.

```tableau
titre: Une requête, vue depuis libpq
colonnes: cpp-pg | libpq | PostgreSQL
étape: Tu passes le SQL et les valeurs **séparément**. Le serveur ne peut pas confondre les deux.
cpp-pg -> libpq: PQexecParams(conn, sql, values)
libpq -> PostgreSQL: requête + paramètres
étape: La réponse arrive entière dans la mémoire de ton processus, dans un `PGresult`. C'est toi qui en es responsable à partir de maintenant.
PostgreSQL -> libpq: lignes
libpq -> cpp-pg: PGresult *
étape: Rien n'a encore été vérifié. Un `PGresult` existe même quand la requête a échoué : il faut demander son état.
cpp-pg -> libpq: PQresultStatus(res)
libpq -> cpp-pg: PGRES_TUPLES_OK
étape: Tout arrive en texte. `"42"` n'est pas encore un entier : c'est à toi de le convertir.
cpp-pg -> libpq: PQgetvalue(res, 0, 0)
libpq -> cpp-pg: "alice"
étape: Sans cet appel, la mémoire n'est jamais rendue. C'est exactement ce que le destructeur de `pg::Rows` fait pour toi.
! cpp-pg -> libpq: PQclear(res)
```

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

## Exemple déroulé : lire une colonne qui peut être vide
La colonne `biography` de `matcha_profiles` accepte `NULL`. Regarde ce que renvoie libpq pour trois comptes :

```tableau
titre: Trois comptes, trois cas
étape: Biographie remplie : `PQgetvalue` renvoie le texte.
> "J'aime la randonnée"     PQgetisnull = 0
étape: Biographie vide, mais renseignée : chaîne vide.
> ""                        PQgetisnull = 0
étape: Biographie jamais saisie : `PQgetvalue` renvoie **aussi** une chaîne vide.
! > ""                        PQgetisnull = 1
étape: Sans `PQgetisnull`, les deux derniers cas sont indiscernables. Le typage de `Row::get<std::optional<std::string>>` règle ce problème une fois pour toutes.
+ = NULL devient std::nullopt, "" reste une chaîne vide
```

C'est la raison d'être de `Row::get<T>` : la vérification de `NULL` n'est plus une chose à penser à chaque lecture.

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
