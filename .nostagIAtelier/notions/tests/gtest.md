# GoogleTest : fixtures et tests paramétrés
> Structurer les tests pour réutiliser leur préparation, et couvrir beaucoup de cas sans répétition.

## Mise en situation
Tu dois vérifier la [compatibilité d'orientation](../metier/orientation.md) sur toutes les combinaisons, et plusieurs tests de [pool](../sql/pool-connexions.md) ont besoin d'une base. Recopier la même préparation dans chaque test les rend longs, et une correction doit se faire partout.

## La théorie
### Un test
```cpp
TEST(HaversineTest, ParisLyon) {
    EXPECT_NEAR(haversineKm(kParis, kLyon), 392.0, 5.0);
}
```

Un bon test suit trois temps : préparer, agir, vérifier.

### EXPECT et ASSERT
- `EXPECT_*` signale l'échec et continue.
- `ASSERT_*` signale l'échec et sort du test.

On utilise `ASSERT` quand la suite n'a pas de sens :

```cpp
auto rows = db.query("SELECT 1");
ASSERT_TRUE(rows);                // sinon *rows planterait
EXPECT_EQ(rows->size(), 1);
```

### La fixture
```cpp
class PoolTest : public ::testing::Test {
protected:
    void SetUp() override { /* avant chaque test */ }
    void TearDown() override { /* après chaque test */ }
    std::unique_ptr<pg::ConnectionPool> pool;
};

TEST_F(PoolTest, TimesOutWhenExhausted) { ... }
```

Un nouvel objet est créé pour chaque test : ils ne partagent pas d'état, donc l'ordre n'a pas d'importance.

### Les tests paramétrés
```cpp
struct Cas { Orientation o1; Gender g1; Orientation o2; Gender g2; bool attendu; };

class CompatibilityTest : public ::testing::TestWithParam<Cas> {};

TEST_P(CompatibilityTest, MatchesTable) {
    const Cas &c = GetParam();
    EXPECT_EQ(compatible({c.o1, c.g1}, {c.o2, c.g2}), c.attendu);
}

INSTANTIATE_TEST_SUITE_P(Table, CompatibilityTest, ::testing::ValuesIn(kCas));
```

Une ligne par cas, et un rapport pour chacune.

### Sauter un test
```cpp
if (!std::getenv("PG_TEST_HOST"))
    GTEST_SKIP() << "PG_TEST_HOST non défini";
```

La lib reste compilable et testable sans base.

## Exemple déroulé
```text
[ RUN      ] PoolTest.TimesOutWhenExhausted
[       OK ] PoolTest.TimesOutWhenExhausted (2003 ms)
```

Sur un échec, GoogleTest donne fichier, ligne, valeur attendue et valeur reçue. Filtrer un seul test :

```sh
./tests --gtest_filter='PoolTest.*'
```

## Erreurs fréquentes
- `EXPECT_EQ` sur des `double` : utilise `EXPECT_NEAR`.
- Dépendre de l'ordre des tests.
- N'avoir jamais vu le test échouer. Casse le code exprès, une fois.
- Tester l'implémentation plutôt que le comportement.

## À retenir
Un test doit dire clairement ce qu'il vérifie, et échouer pour une seule raison.

## Exercice
Quand préférer `ASSERT_TRUE` à `EXPECT_TRUE` ?

## Corrigé
Quand la suite dépend du résultat. Si `pool.begin()` échoue, utiliser le résultat serait un comportement indéfini. `ASSERT` arrête ce test proprement. `EXPECT` convient quand plusieurs vérifications indépendantes peuvent toutes être rapportées.

## Voir aussi

- [Les faux objets de test](faux-objets.md)
- [Tests de repository en transaction annulée](transaction-annulee.md)
- [La pyramide de tests](pyramide.md)
