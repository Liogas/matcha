# La pyramide de tests
> Beaucoup de tests purs rapides, moins de tests d'intégration, et quelques tests de bout en bout.

## Mise en situation
Tu dois rendre un projet sans régression, avec du temps limité. Tout tester par le navigateur serait lent et fragile. Ne tester que des fonctions pures ne verrait pas une requête SQL fausse. Comment répartir l'effort ?

## La théorie
### Les étages
```text
        /\        Bout en bout : parcours complets, lents
       /  \
      /____\      Intégration : avec une vraie base
     /      \
    /________\    Unitaires : logique pure, instantanés
```

- **Unitaires** : validateurs, `computeFame`, `haversineKm`, [`QueryBuilder`](../sql/query-builder.md). Des millisecondes.
- **Intégration** : [repositories](../archi/couche-services.md) sur PostgreSQL, en [transaction annulée](transaction-annulee.md).
- **Bout en bout** : requêtes HTTP, ou scénario complet.

### Le critère
Plus on monte, plus le test est réaliste, mais plus il est lent, fragile, et difficile à diagnostiquer. On place chaque vérification le plus bas possible.

### Ce qui va où
- Une règle de calcul : unitaire.
- Une [contrainte](../sql/contraintes.md) SQL ou une requête : intégration.
- Le blocage appliqué à une route : intégration, ou API.
- Inscription, like mutuel, chat : bout en bout.

## Exemple déroulé : le like sans photo
- Unitaire, avec des faux : `MatchService` renvoie `Forbidden`.
- API : `POST /api/profiles/7/like` renvoie `403`.
- Le test de [contrainte](../sql/contraintes.md) SQL n'est pas nécessaire.

Le like simultané est lui un test d'intégration avec deux threads et une vraie base, car c'est justement le comportement de PostgreSQL qu'il faut vérifier.

## Pour matcha
`ProfileRepository`, `LikeRepository` et `MessagingRepository` n'ont pas encore de tests. Les priorités :

1. les règles exigées par le sujet ;
2. les autorisations, avec un test par point d'entrée, voir la règle centralisée ;
3. les bugs déjà rencontrés.

Un bug corrigé doit donner un test : il ne reviendra pas.

## La couverture
Un chiffre de couverture dit ce qui a été exécuté, pas ce qui a été vérifié. 100 % sans assertion ne prouve rien.

## Erreurs fréquentes
- Tout tester en bout en bout.
- Des tests lents que personne ne lance.
- Des tests instables qu'on ignore.

## À retenir
Un test vaut par la confiance qu'il donne, divisée par ce qu'il coûte.

## Exercice
À quel étage tester le fait qu'une biographie de 501 caractères est refusée ?

## Corrigé
Unitaire, sur le validateur : il couvre tous les cas limites instantanément. Un seul test d'API suffit ensuite à vérifier que la route l'utilise bien.

## Voir aussi

- [GoogleTest : fixtures et tests paramétrés](gtest.md)
- [Les faux objets de test](faux-objets.md)
