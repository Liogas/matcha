# Le problème N+1
> Une requête pour la liste, puis une requête par élément : N+1 allers-retours au lieu d'un seul.

## Mise en situation
`GET /api/likes/received` renvoie seulement des ids. Pour afficher 40 cartes, le front appelle 40 fois `GET /api/profiles/<id>`. Chaque appel prend 5 ms, ce qui paraît correct. Mais il y a 40 passages par [Nginx](../infra/nginx.md), 40 authentifications et 40 requêtes SQL. La page met une seconde.

Le même problème existe dans le backend :

```cpp
auto likes = repo.receivedLikes(me);      // 1 requête
for (auto &like : likes)
    profils.push_back(repo.find(like.id)); // N requêtes
```

```tableau
titre: Afficher 3 profils, version N+1
colonnes: Service | PostgreSQL
étape: Une requête pour obtenir la liste des ids.
Service -> PostgreSQL: SELECT ids
PostgreSQL -> Service: 3 ids
étape: Puis une requête par id. Chaque aller-retour attend la réponse du précédent.
! Service -> PostgreSQL: SELECT profil 1
! Service -> PostgreSQL: SELECT profil 2
! Service -> PostgreSQL: SELECT profil 3
étape: 4 requêtes pour 3 profils. Avec 40 profils, ce serait 41, et le code n'aurait pas changé.
! = 1 + N requêtes
étape: La jointure ramène tout en un seul aller-retour, quel que soit N.
+ Service -> PostgreSQL: SELECT ... JOIN
+ PostgreSQL -> Service: 3 profils complets
```

### Pourquoi personne ne l'a vu
Fais le calcul avec de vrais chiffres. En développement, tu as 3 likes reçus : 4 requêtes à 1 ms sur le réseau local du conteneur, soit 4 ms. La page paraît instantanée. Le jour de la soutenance, le correcteur génère 500 profils. Le compte le plus liké en a reçu 120 : 121 requêtes, chacune avec son passage par le [pool](pool-connexions.md) et son aller-retour. Le code n'a pas changé, seule la quantité de données a changé. C'est ce qui rend ce défaut traître : il ne se voit jamais sur la machine de celui qui l'écrit.

## La théorie
Le coût d'une requête vient surtout de l'aller-retour, pas du travail. N+1 multiplie les allers-retours. Il est difficile à voir, car chaque requête est rapide, et il passe inaperçu avec peu de données.

Il existe deux solutions.

### La jointure
```sql
SELECT p.account_id, p.first_name, ph.id AS photo_id, l.created_at
FROM profile_likes l
JOIN matcha_profiles p ON p.account_id = l.liker_account_id
LEFT JOIN matcha_profile_photos ph
    ON ph.profile_account_id = p.account_id AND ph.is_primary
WHERE l.liked_account_id = $1
ORDER BY l.created_at DESC
LIMIT 20;
```

Une requête, 20 profils. `LEFT JOIN` garde les profils sans photo.

### Le chargement groupé
Pour des données à plusieurs lignes par profil, comme les tags, une jointure dupliquerait les profils. On charge donc en deux temps, pour un total de 2 requêtes :

```sql
SELECT pi.profile_account_id, i.name
FROM matcha_profile_interests pi
JOIN matcha_interests i ON i.id = pi.interest_id
WHERE pi.profile_account_id = ANY($1);
```

`$1` est le tableau des ids de la page. On regroupe ensuite en mémoire avec une `std::unordered_map`.

## Exemple déroulé
Pour 20 profils avec tags :

```tableau
titre: Le compte, pour 20 profils avec leurs tags
étape: Avant : une liste, puis deux boucles.
! = 1 (liste) + 20 (profils) + 20 (tags) = 41 requêtes
étape: Une jointure remplace la liste et la boucle des profils.
= 1 (profils joints) + 20 (tags) = 21 requêtes
étape: Le chargement groupé remplace la deuxième boucle : `ANY($1)` prend tous les ids d'un coup.
+ = 1 (profils joints) + 1 (tags) = 2 requêtes
```

La réponse API contient directement des profils résumés, et le front n'a plus qu'un appel à faire, voir l'[API REST](../web/rest-conception.md).

## Comment le repérer
- Compte les requêtes par appel HTTP, avec `log_statement = 'all'` en développement.
- Dans le navigateur, regarde l'onglet réseau.
- Écris un test qui compte les requêtes exécutées.

## Exemple pas à pas : réécrire la boucle
Voici la transformation à savoir refaire sans réfléchir.

```cpp
// Avant : une requête cachée dans la boucle
std::vector<ProfileSummary> out;
for (auto &like : repo.receivedLikes(me))
    out.push_back(repo.findSummary(like.id));
```

**Étape 1.** Repère ce dont la boucle a besoin : pour chaque like, le profil. Les deux tables se rejoignent sur `liker_account_id`.

**Étape 2.** Écris une seule méthode qui ramène le résultat final, au lieu de deux méthodes à combiner :

```cpp
// Après : tout en une requête
auto out = repo.receivedLikeSummaries(me, limit);
```

**Étape 3.** Pour ce qui compte plusieurs lignes par profil, les tags, charge en bloc puis range en mémoire :

```cpp
std::vector<std::int64_t> ids;
for (auto &profil : out)
    ids.push_back(profil.accountId);

auto tags = repo.tagsFor(ids);          // ANY($1), une requête
for (auto &profil : out)
    profil.tags = std::move(tags[profil.accountId]);
```

`tags` est une `std::unordered_map<std::int64_t, std::vector<std::string>>` : retrouver les tags d'un profil ne coûte aucune requête. Si la map n'a pas la clé, `operator[]` crée un vecteur vide, ce qui convient à un profil sans tag.

## Erreurs fréquentes
- Tout joindre : deux relations multiples dans une même requête multiplient les lignes, c'est le produit cartésien.
- Corriger seulement le backend, en gardant une API qui renvoie des ids.

## À retenir
Quand une boucle contient une requête, demande-toi si une seule requête ne pourrait pas tout ramener.

## Exercice
Pourquoi le N+1 n'a-t-il pas été remarqué pendant le développement ?

## Corrigé
Le seed ne compte que 10 profils, avec peu de likes : quelques requêtes rapides passent inaperçues. Le problème croît avec les données : c'est une raison concrète d'avoir 500 profils et de tester avec eux.

## Voir aussi

- [Les relations N-N](relation-nn.md)
- [Tableaux et unnest](tableaux-unnest.md)
- [Concevoir une API REST](../web/rest-conception.md)
