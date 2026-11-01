# GoogleTest : fixtures et tests paramétrés
> Structurer les tests pour réutiliser leur préparation, et couvrir beaucoup de cas sans répétition.

## Mise en situation
Tu dois vérifier la [compatibilité d'orientation](../metier/orientation.md) sur toutes les combinaisons, et plusieurs tests de [pool](../sql/pool-connexions.md) ont besoin d'une base. Recopier la même préparation dans chaque test les rend longs, et une correction doit se faire partout.

### Une suite de tests qui se dégrade
Tu écris un premier test de [pool](../sql/pool-connexions.md) : dix lignes de préparation, trois de vérification. Tu copies pour le test suivant, puis encore cinq fois. Un mois plus tard, le constructeur du pool prend un paramètre de plus : tu corriges sept endroits. Pour les orientations, tu as 6 orientations multipliées par 2 genres, de chaque côté : 144 combinaisons. Personne n'écrit 144 fois `TEST`. On écrit donc 5 cas, et les 139 autres ne sont jamais testés.

Fixture et tests paramétrés répondent à ces deux problèmes : une préparation à un seul endroit, et une table de cas au lieu d'une pile de copier-coller.

```tableau
titre: Ce que GoogleTest fait pour chaque TEST_F
colonnes: GoogleTest | Fixture | Ton test
étape: Il crée un objet fixture **neuf**. Rien ne reste du test précédent.
GoogleTest -> Fixture: new PoolTest
étape: Il prépare le terrain.
GoogleTest -> Fixture: SetUp()
étape: Le corps du test s'exécute. Dans le code, c'est une méthode d'une classe fille de la fixture : d'où l'accès direct à `pool`.
GoogleTest -> Ton test: corps du test
étape: Un `ASSERT` qui échoue sort du test, mais `TearDown` est quand même appelé.
! Ton test: ASSERT échoué
GoogleTest -> Fixture: TearDown()
étape: L'objet est détruit, puis tout recommence pour le test suivant.
GoogleTest -> Fixture: delete
```

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

## Exemple pas à pas : EXPECT ou ASSERT ?
Le choix se décide en une question : si cette vérification échoue, la ligne suivante a-t-elle encore un sens ?

```cpp
TEST_F(ProfileRepositoryTest, FindsCompletedProfile) {
    auto id = accounts.create("a@example.test", "alice");
    ASSERT_TRUE(id);                    // sans id, *id plante juste après

    auto profil = profiles.find(*id);
    ASSERT_TRUE(profil);                // sans profil, profil-> plante

    EXPECT_EQ(profil->firstName, "Alice"); // on veut voir tous les écarts d'un coup
    EXPECT_EQ(profil->age, 30);
}
```

Les deux premières vérifications sont des `ASSERT` : la suite déréférence la valeur. Les deux dernières sont des `EXPECT` : si le prénom est faux, tu veux quand même savoir si l'âge l'est aussi.

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
