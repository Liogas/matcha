# SAVEPOINT
> Un point de retour à l'intérieur d'une [transaction](transactions.md), pour annuler une partie sans tout perdre.

## Mise en situation
La [fixture](../tests/gtest.md) des tests de [repository](../archi/couche-services.md) ouvre une [transaction](transactions.md), qu'elle annule à la fin. Un test vérifie le refus d'un email en double :

```cpp
ASSERT_TRUE(accounts.create("a@example.test"));
EXPECT_FALSE(accounts.create("a@example.test")); // 23505 attendu
EXPECT_TRUE(accounts.findByEmail("a@example.test")); // échoue !
```

La troisième ligne échoue avec :

```text
ERROR: current transaction is aborted, commands ignored until end of transaction block
```

Le refus attendu a mis toute la transaction en échec, comme expliqué dans la fiche transactions. Le test ne peut plus rien faire.

```tableau
titre: L'erreur attendue qui empoisonne le test
colonnes: Test | PostgreSQL
étape: Le premier compte est créé. La transaction est saine.
Test -> PostgreSQL: INSERT a@example.test
+ PostgreSQL: OK
étape: Le doublon est refusé, comme le test l'espère.
Test -> PostgreSQL: INSERT a@example.test
! PostgreSQL: 23505
étape: PostgreSQL considère maintenant toute la transaction comme perdue.
! = transaction aborted
étape: La requête suivante est refusée sans même être lue.
Test -> PostgreSQL: SELECT ...
! PostgreSQL: 25P02
```

### Pourquoi PostgreSQL est si sévère
Ce comportement paraît excessif, mais il protège tes données. Si une requête d'une transaction échoue et que PostgreSQL te laissait continuer, un `COMMIT` validerait une opération incomplète. Il préfère donc refuser tout le reste jusqu'à ce que tu dises explicitement quoi faire : tout annuler, ou revenir à un point précis. Le savepoint est ce point précis.

## La théorie
Un savepoint marque un état dans la [transaction](transactions.md) :

```sql
BEGIN;
INSERT ...;                 -- A
SAVEPOINT avant_doublon;
INSERT ...;                 -- échoue
ROLLBACK TO SAVEPOINT avant_doublon;
SELECT ...;                 -- fonctionne, A existe toujours
COMMIT;
```

- `SAVEPOINT nom` crée le point.
- `ROLLBACK TO SAVEPOINT nom` annule ce qui suit et sort de l'état d'échec. Le savepoint reste utilisable.
- `RELEASE SAVEPOINT nom` oublie le point, sans annuler.

On peut les imbriquer. PostgreSQL les met en œuvre comme des sous-transactions.

### Le coût
Chaque savepoint crée une sous-transaction. En créer des milliers dans une même [transaction](transactions.md) dégrade les performances. Il ne faut pas en mettre un par ligne dans une grosse boucle d'import.

## Exemple complet : la fixture du guide 06
Le guide ajoute un `SAVEPOINT` autour de chaque requête de test : on le pose avant la requête, on le relâche si elle réussit, on y revient si elle échoue. Le test se comporte ainsi comme une suite de requêtes indépendantes :

```tableau
titre: La même suite, avec un savepoint
colonnes: Fixture | PostgreSQL
étape: Avant la requête risquée, on pose un point de retour.
Fixture -> PostgreSQL: SAVEPOINT s
étape: Le doublon échoue. La transaction est en échec, pour l'instant.
Fixture -> PostgreSQL: INSERT doublon
! PostgreSQL: 23505
étape: On revient au point. L'échec est effacé, et tout ce qui précède `s` est conservé.
Fixture -> PostgreSQL: ROLLBACK TO s
étape: La transaction fonctionne de nouveau.
Fixture -> PostgreSQL: SELECT ...
+ PostgreSQL: OK
```

Le test reste dans une seule [transaction](transactions.md), annulée à la fin, voir la [transaction annulée](../tests/transaction-annulee.md).

## Quand s'en passer
Si l'échec est prévu sur un conflit d'unicité, [ON CONFLICT](on-conflict.md) est plus simple, plus rapide, et n'empoisonne pas la [transaction](transactions.md).

## Exemple pas à pas : ce que voit la table
Reprends une suite d'instructions et regarde le contenu de `t` après chacune :

```tableau
titre: Le contenu de la table, ligne après ligne
étape: La transaction commence, la table est vide.
> BEGIN;                        t = {}
étape: Première insertion.
> INSERT 1;                     t = {1}
étape: On pose le point de retour.
> SAVEPOINT s;                  t = {1}
étape: On insère de nouveau, après le point.
> INSERT 2;                     t = {1, 2}
étape: On revient en arrière : seul ce qui suit le point est annulé.
! > ROLLBACK TO s;                t = {1}
étape: La transaction n'est pas terminée, on peut continuer.
> INSERT 3;                     t = {1, 3}
étape: `COMMIT` valide tout ce qui reste. Le 2 n'a jamais existé pour les autres connexions.
+ > COMMIT;                       t = {1, 3}
```

## Erreurs fréquentes
- Croire que `ROLLBACK TO` termine la [transaction](transactions.md) : elle continue.
- Faire un `COMMIT` en pensant valider seulement la partie après le savepoint : tout est validé.
- Utiliser un savepoint là où [`ON CONFLICT`](on-conflict.md) suffit.

## À retenir
Le savepoint ne sert pas à annuler plus proprement : il sert à continuer après une erreur attendue, à l'intérieur de la même [transaction](transactions.md).

## Exercice
Que contient la table à la fin ?

```sql
BEGIN;
INSERT INTO t VALUES (1);
SAVEPOINT s;
INSERT INTO t VALUES (2);
ROLLBACK TO SAVEPOINT s;
INSERT INTO t VALUES (3);
COMMIT;
```

## Corrigé
1 et 3. Le 2 a été annulé par `ROLLBACK TO`, et tout le reste est validé par [`COMMIT`](transactions.md).

## Voir aussi

- [Les transactions et ACID](transactions.md)
- [Tests de repository en transaction annulée](../tests/transaction-annulee.md)
- [INSERT ON CONFLICT](on-conflict.md)

<!-- enrichie -->
