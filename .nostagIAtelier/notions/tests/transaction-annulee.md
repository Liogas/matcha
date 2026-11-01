# Tests de repository en transaction annulée
> Le test écrit dans une [transaction](../sql/transactions.md) jamais validée : il voit ses propres données, et ne laisse aucune trace.

## Mise en situation
Les anciens tests de [repository](../archi/couche-services.md) écrivaient dans la base de développement. Après chaque lancement, il restait des comptes de test. Au deuxième passage, `create("a@example.test")` échouait sur un doublon. Pire, ces données se mêlaient à tes profils de seed.

Tu pourrais vider les tables avant chaque test avec `TRUNCATE`, mais ce serait lent, et dangereux si le test tourne sur la mauvaise base.

### Ce que tu évites, concrètement
Imagine la suite complète sans cette technique. Tu lances `make test` une première fois : tout passe. Tu relances : `create("a@example.test")` échoue en `23505`, car le compte existe encore. Tu ajoutes un `TRUNCATE` : les tests passent, mais tu viens de vider ta base de développement et tes profils de seed. Tu crées alors une base dédiée, et deux tests lancés en parallèle se marchent dessus.

Chaque solution crée le problème suivant. La transaction annulée règle tout d'un coup, car elle ne laisse rien à nettoyer.

## La théorie
### L'idée
Ce qui n'est pas validé n'existe que pour la connexion qui l'a écrit. Une [transaction](../sql/transactions.md) ouverte au début du test et annulée à la fin efface donc tout, quoi qu'il se soit passé.

```tableau
titre: La vie d'un test
colonnes: Fixture | Test | PostgreSQL
étape: `SetUp` ouvre une transaction avant que le test commence.
Fixture -> PostgreSQL: BEGIN
étape: Le test écrit. Il voit ses lignes, car il est sur la même connexion.
Test -> PostgreSQL: INSERT compte
Test -> PostgreSQL: SELECT compte
+ PostgreSQL -> Test: il existe
étape: `TearDown` détruit la transaction, même si le test a échoué.
Fixture -> PostgreSQL: ROLLBACK
étape: La base est comme avant. Le test suivant peut recréer le même email.
+ = aucune trace sur le disque
```

C'est rapide, car rien n'atteint le disque. Et les tests sont isolés, car aucun autre ne voit ces lignes.

### Ce qui rend cela possible
Le [repository](../archi/couche-services.md) reçoit un `pg::Executor &`, une [interface](../cpp/interfaces-injection.md). En production, on lui donne le [pool](../sql/pool-connexions.md). En test, on lui donne la [transaction](../sql/transactions.md). Il n'en sait rien, et son code ne change pas. Sans cette interface, la technique serait impossible : le repository prendrait sa propre connexion et validerait ses écritures.

## Exemple complet
```cpp
class AccountRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        _tx.emplace(pool().begin().value());
    }
    void TearDown() override {
        _tx.reset(); // détruit la Transaction : ROLLBACK
    }
    pg::Transaction &db() { return *_tx; }
private:
    std::optional<pg::Transaction> _tx;
};

TEST_F(AccountRepositoryTest, FindsCreatedAccount) {
    AccountRepository accounts(db());
    auto id = accounts.create("a@example.test", "alice");
    ASSERT_TRUE(id);
    EXPECT_TRUE(accounts.findById(*id));
}
```

Pourquoi `std::optional<pg::Transaction>` ? Une [`Transaction`](../sql/transactions.md) ne peut pas exister vide et n'est pas réaffectable. `optional` permet de la créer dans `SetUp` et de la détruire dans `TearDown`, à des moments choisis. C'est un bon exemple de [durée de vie](../cpp/duree-de-vie-objets.md) contrôlée.

### L'erreur attendue
Une violation de [contrainte](../sql/contraintes.md) empoisonne la [transaction](../sql/transactions.md), d'où le [SAVEPOINT](../sql/savepoint.md) par requête dans la [fixture](gtest.md).

## Quand ne pas l'utiliser
Trois cas exigent une vraie validation :

- **Tester `commit()` lui-même** ;
- **Tester deux connexions** : le like simultané a besoin de deux [transactions](../sql/transactions.md) parallèles, et l'une doit voir ce que l'autre a validé ;
- **Tester un [trigger](../sql/denormalisation.md) différé**, qui ne se déclenche qu'au commit.

Dans ces cas, on utilise une base de test en `tmpfs` et `TRUNCATE` avant chaque test, comme les tests de cpp-pg.

## Exemple pas à pas : deux tests qui utilisent le même email
Les deux tests créent `a@example.test`. Sans annulation, le second échouerait. Suis ce qui existe dans la base à chaque instant :

```tableau
titre: Deux tests, le même email
colonnes: Test 1 | PostgreSQL | Test 2
étape: Test 1 ouvre sa transaction et crée le compte. Seule sa connexion voit la ligne.
Test 1 -> PostgreSQL: BEGIN, INSERT a@
étape: À la fin du test 1, tout est annulé.
Test 1 -> PostgreSQL: ROLLBACK
+ PostgreSQL: table vide
étape: Test 2 démarre sur une base identique à celle du test 1.
Test 2 -> PostgreSQL: BEGIN, INSERT a@
+ PostgreSQL: OK
étape: Pas de doublon : aux yeux de la base, le test 1 n'a jamais existé.
Test 2 -> PostgreSQL: ROLLBACK
```

L'ordre des tests n'a plus d'importance, et `ctest -j8` peut les lancer en parallèle : chacun vit dans sa [transaction](../sql/transactions.md).

## Erreurs fréquentes
- Utiliser `NOW()` pour vérifier une date : dans une [transaction](../sql/transactions.md), `NOW()` renvoie le début de la transaction, et reste identique pendant tout le test. `clock_timestamp()` donne l'heure réelle.
- Faire appeler le [pool](../sql/pool-connexions.md) par du code sous test : il ne voit pas les données de la transaction.
- Croire que `BIGSERIAL` revient en arrière : les séquences ne sont pas annulées, donc ne teste jamais une valeur précise d'id.

## À retenir
La transaction annulée donne des tests rapides et propres, à condition que le code reçoive son exécuteur de l'extérieur.

## Exercice
Pourquoi le test « deux threads likent en même temps » ne peut-il pas utiliser cette [fixture](gtest.md) ?

## Corrigé
Il faut deux [transactions](../sql/transactions.md) sur deux connexions, et le test repose sur le fait que l'une voie ce que l'autre a validé. Or une [fixture](gtest.md) annulée partage une seule transaction, jamais validée. Le second thread ne verrait rien, et les deux threads se bloqueraient sur la même connexion.

## Voir aussi

- [SAVEPOINT](../sql/savepoint.md)
- [Interfaces et injection de dépendances](../cpp/interfaces-injection.md)
- [GoogleTest : fixtures et tests paramétrés](gtest.md)
