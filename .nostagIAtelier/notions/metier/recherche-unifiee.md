# Un seul moteur pour suggestions et recherche
> Les suggestions et la recherche partagent une seule requête, paramétrée par un objet.

## Mise en situation
Les deux pages filtrent et trient par âge, popularité, localisation et tags. Écrites séparément, elles divergeront : un filtre corrigé ici et oublié là. Plus grave, le blocage pourrait manquer dans l'une d'elles.

## La théorie
### Un objet décrit la demande
```cpp
struct ProfileQuery {
    std::optional<int> ageMin, ageMax;
    std::optional<int> fameMin, fameMax;
    std::optional<double> distanceMaxKm;
    std::vector<std::string> tags;
    std::string sort = "score";
    std::optional<Cursor> cursor;
};
```

La route lit l'URL, valide, remplit l'objet. Le [repository](../archi/couche-services.md) traduit l'objet en SQL.

### Deux niveaux de règles
- **Règles fixes**, toujours appliquées : profil complet, pas soi-même, compatibilité, pas de blocage.
- **Filtres choisis**, optionnels.

Les premières sont dans la fonction, sans paramètre pour les désactiver : on ne peut pas les oublier.

## Exemple déroulé
```cpp
core::Result<pg::Rows> ProfileRepository::search(AccountId viewer, const ProfileQuery &q) {
    pg::QueryBuilder query(kProfileSelect);
    query.where("p.account_id <> ?", viewer)
         .where("p.completed_at IS NOT NULL")
         .where("NOT is_blocked(?, p.account_id)", viewer)
         .whereIf(q.fameMin, "p.fame_rating >= ?")
         .whereIf(q.fameMax, "p.fame_rating <= ?")
         .orderBy(q.sort, kSorts)
         .limit(20);
    auto built = query.build();
    if (!built)
        return std::unexpected(built.error());
    return _database.execute(built->sql, built->params);
}
```

`/api/suggestions` et `/api/search` appellent la même fonction. Seul le tri par défaut diffère.

### Les tags
Plusieurs tags : tous, ou au moins un ? Il faut décider. Pour tous :

```sql
AND (SELECT COUNT(*) FROM matcha_profile_interests pi
     JOIN matcha_interests i ON i.id = pi.interest_id
     WHERE pi.profile_account_id = p.account_id
       AND LOWER(i.name) = ANY($n)) = $m
```

### L'URL
```text
GET /api/suggestions?sort=score&age_min=20&age_max=35&tags=vegan,geek&cursor=...
```

Validation stricte : `age_min <= age_max`, tags normalisés, clé de tri connue.

## Tester
Un test par filtre, et un test du blocage : il couvre automatiquement les deux routes.

## Erreurs fréquentes
- Une route qui construit sa propre requête « pour aller plus vite ».
- Rendre les règles fixes désactivables.
- Trop de paramètres sans limite.

## À retenir
Une seule requête pour un même besoin : une correction vaut pour toutes les pages.

## Exercice
Quel bug de sécurité évite un moteur unique ?

## Corrigé
Oublier le blocage dans une des deux pages, ce qui ferait réapparaître un utilisateur bloqué. La règle fixe s'applique dans la seule fonction qui interroge les profils.

## Voir aussi

- [Un QueryBuilder sûr](../sql/query-builder.md)
- [Pagination keyset ou OFFSET](../sql/pagination-keyset.md)
- [Le score de matching](score-matching.md)
- [Une règle transversale centralisée](policy-centralisee.md)
