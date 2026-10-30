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

```text
SAVEPOINT s
INSERT doublon -> erreur 23505
ROLLBACK TO s
SELECT -> fonctionne
```

Le test reste dans une seule [transaction](transactions.md), annulée à la fin, voir la [transaction annulée](../tests/transaction-annulee.md).

## Quand s'en passer
Si l'échec est prévu sur un conflit d'unicité, [ON CONFLICT](on-conflict.md) est plus simple, plus rapide, et n'empoisonne pas la [transaction](transactions.md).

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
