# Les niveaux d'isolation
> Le niveau d'isolation fixe ce qu'une [transaction](transactions.md) a le droit de voir des autres transactions en cours.

## Mise en situation
Alice et Bob se likent à la même milliseconde. Chacun a sa propre [transaction](transactions.md) :

```text
A : BEGIN                       B : BEGIN
A : INSERT like Alice->Bob      B : INSERT like Bob->Alice
A : like Bob->Alice existe ?    B : like Alice->Bob existe ?
    NON (B n'a pas validé)          NON (A n'a pas validé)
A : COMMIT                      B : COMMIT
```

Deux likes sont en base, et aucun match. Aucune requête n'est fausse, et les deux transactions ont fonctionné. Le problème vient de ce que chacune a le droit de voir pendant que l'autre travaille.

## La théorie
### La photo de la base
Chaque requête lit une photo cohérente de la base, appelée snapshot, grâce à MVCC (voir les [transactions](transactions.md)). Le niveau d'isolation répond à une question simple : quand cette photo est-elle prise ?

### READ COMMITTED, le défaut
Une nouvelle photo est prise au début de **chaque instruction**. Une instruction voit donc tout ce qui a été validé avant qu'elle commence.

```text
T1 : SELECT nb FROM t;      -> 10
T2 : UPDATE t SET nb = 20; COMMIT;
T1 : SELECT nb FROM t;      -> 20
```

C'est une lecture non répétable. Elle est autorisée à ce niveau.

### REPEATABLE READ
La photo est prise à la **première instruction** de la [transaction](transactions.md), puis gardée. Le second `SELECT` lit encore 10. Si T1 essaie de modifier une ligne changée depuis sa photo, PostgreSQL l'annule avec `40001`.

### SERIALIZABLE
En plus de la photo fixe, PostgreSQL surveille les dépendances entre [transactions](transactions.md). S'il ne peut pas garantir un résultat équivalent à une exécution l'une après l'autre, il annule l'une d'elles avec `40001`. Il faut alors **rejouer toute la transaction**.

### Les anomalies
- **Lecture sale** : lire une donnée non validée. Impossible dans PostgreSQL, à tous les niveaux.
- **Lecture non répétable** : une ligne relue a changé. Possible en `READ COMMITTED`.
- **Lecture fantôme** : un même filtre renvoie de nouvelles lignes. Possible en `READ COMMITTED`.
- **Write skew** : deux [transactions](transactions.md) lisent le même état, décident séparément, et le résultat global viole une règle. Possible jusqu'à `REPEATABLE READ`. Le like croisé appartient à cette famille.

## Exemple complet : le piège de la solution
Une intuition courante est : « un niveau plus strict est toujours plus sûr ». Testons avec le verrou par paire en `REPEATABLE READ` :

```text
B : BEGIN
B : prise du verrou  ... attend A   <- première instruction, la photo est prise ICI
A : COMMIT
B : obtient le verrou
B : le like d'Alice existe ?  NON   <- photo d'avant le COMMIT de A
```

B attend bien, mais il regarde une vieille photo. Le match n'est pas créé. Le test du guide 06 l'a montré pour de vrai.

En `READ COMMITTED`, l'instruction qui suit démarre après le [`COMMIT`](transactions.md) de A, prend une photo neuve, et voit le like. Le niveau le moins strict est ici le bon.

Un niveau d'isolation plus élevé donne d'autres garanties, pas plus de sécurité dans l'absolu.

## Choisir pour matcha
`SERIALIZABLE` résoudrait le like croisé sans verrou, mais il faudrait rejouer chaque [transaction](transactions.md) sur `40001`, et les risques d'annulation augmenteraient avec la charge. On garde `READ COMMITTED` et on sérialise seulement ce qui en a besoin, par paire d'utilisateurs. Les autres paires ne sont pas ralenties.

Dans cpp-pg, le choix se fait à l'ouverture, avec `pg::Isolation` : `ReadCommitted`, `RepeatableRead` ou `Serializable`. `errorCodeFor` traduit `40001` en `Unavailable`, voir [SQLSTATE](sqlstate.md).

## Erreurs fréquentes
- **Croire qu'une [transaction](transactions.md) évite les courses** : elle ne le fait pas en `READ COMMITTED`.
- **Utiliser `SERIALIZABLE` sans boucle de reprise** : les utilisateurs verront des erreurs au hasard.
- **Lire puis écrire une valeur** en `READ COMMITTED` : préfère `UPDATE t SET n = n + 1`, qui relit la ligne à jour.

## À retenir
La question n'est pas « quel niveau est le plus fort » mais « à quel moment ma photo est-elle prise, et est-ce ce que je veux ? ».

## Exercice
En `READ COMMITTED`, deux [transactions](transactions.md) exécutent en même temps `UPDATE matcha_profiles SET fame_rating = fame_rating + 1 WHERE account_id = 7`. La valeur de départ est 10. Quelle est la valeur finale ? Et si elles avaient fait un `SELECT`, puis un `UPDATE ... SET fame_rating = 11` ?

## Corrigé
Premier cas : 12. La seconde [transaction](transactions.md) attend le verrou de ligne, puis réévalue la ligne à jour et calcule 11 + 1. Second cas : 11. Les deux ont lu 10 et écrivent 11, donc une mise à jour est perdue. C'est pour cela qu'on calcule dans l'`UPDATE`, ou qu'on verrouille la ligne avec `SELECT ... FOR UPDATE`.

## Voir aussi

- [Les transactions et ACID](transactions.md)
- [Les verrous consultatifs](verrous-consultatifs.md)
- [Les codes SQLSTATE](sqlstate.md)
- [Data race et ThreadSanitizer](../cpp/data-race.md)
