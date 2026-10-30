# Concevoir une API REST
> Des URL qui désignent des ressources, des verbes qui agissent dessus, et des réponses adaptées aux écrans.

## Mise en situation
Le front affiche la fiche d'un profil : photo, popularité, distance, tags communs, en ligne ou non, like reçu ou non. Si chaque information demande un appel, l'écran fait 6 requêtes. Une API se conçoit depuis son utilisation.

## La théorie
### Les principes
- Une URL désigne une ressource : `/api/profiles/7`, pas `/api/getProfile?id=7`.
- Le verbe HTTP donne l'action.
- Le serveur ne garde pas d'état de conversation : chaque requête se suffit à elle-même.
- Les réponses ont un format stable.

### Les sous-ressources
```text
GET    /api/profiles/7
POST   /api/profiles/7/like
DELETE /api/profiles/7/like
POST   /api/profiles/7/block
GET    /api/profile/visitors
```

### Les actions
Certaines actions ne sont pas des ressources : `POST /api/notifications/read`. C'est acceptable.

### Les listes
Une liste a toujours une limite, un tri, des filtres, et un [curseur](../sql/pagination-keyset.md) :

```text
GET /api/suggestions?sort=fame&age_min=20&cursor=abc
{"items":[...],"next_cursor":"def"}
```

### Les paramètres typés
Tout arrive en texte. Il faut convertir et borner. `std::from_chars` convient bien : il ne lève pas d'exception, et on vérifie que toute la chaîne a été lue.

```cpp
int age = 0;
auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), age);
if (ec != std::errc{} || ptr != s.data() + s.size() || age < 18 || age > 120)
    return erreur400();
```

### Le format d'erreur
Toujours le même :

```json
{"error":"Profile not found"}
```

## Exemple déroulé : le profil consulté
```json
{"id":7,"first_name":"Alice","fame_rating":62,
 "online":false,"last_seen_at":"2026-10-30T09:12:00Z",
 "distance_km":3,"interests":["geek"],"common_interests":["geek"],
 "relationship":{"i_liked":true,"liked_me":false,"connected":false,"blocked":false}}
```

Une seule requête alimente tout l'écran. Elle ne contient ni email ni mot de passe, comme l'exige le sujet. Pour construire `relationship`, plusieurs sous-requêtes `EXISTS` suffisent.

## Résumé ou détail
Une liste renvoie un profil résumé : id, prénom, âge, photo, distance. Le détail arrive à l'ouverture.

## Les dates
Toujours en ISO 8601 UTC. Le front les affiche dans le fuseau local.

## Erreurs fréquentes
- Exposer les noms de colonnes tels quels.
- Renvoyer un tableau nu : on ne peut plus ajouter de métadonnées.
- Renvoyer des ids obligeant à des appels en cascade : voir le [N+1](../sql/n-plus-un.md).

## À retenir
Pars de l'écran : quelle réponse permet de l'afficher en un appel ?

## Exercice
Pourquoi `GET /api/profile/visitors` ne doit-il pas renvoyer une liste d'ids ?

## Corrigé
Le front devrait charger chaque profil, soit N appels. Il faut renvoyer des profils résumés, en une seule requête jointe et paginée.

## Voir aussi

- [HTTP : méthodes et codes de statut](http-bases.md)
- [Le problème N+1](../sql/n-plus-un.md)
- [OpenAPI](openapi.md)
- [Pagination keyset ou OFFSET](../sql/pagination-keyset.md)
