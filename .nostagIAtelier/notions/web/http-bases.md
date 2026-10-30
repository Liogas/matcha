# HTTP : méthodes et codes de statut
> Le verbe dit ce qu'on veut faire, le code de statut dit ce qui s'est passé.

## Mise en situation
Le front reçoit une réponse. Il doit décider : afficher une erreur de formulaire, déconnecter, réessayer, ou signaler un bug. Tout repose sur le code de statut. Un mauvais code provoque un mauvais comportement : c'est exactement le bug du `401` à la place d'un `503` (audit 4.8).

## La théorie
### Requête et réponse
HTTP est un échange de texte sans état :

```text
POST /api/profiles/7/like HTTP/1.1
Host: matcha.local
Authorization: Bearer eyJ...

HTTP/1.1 200 OK
Content-Type: application/json

{"liked":true,"matched":false}
```

### Les méthodes
- **GET** : lit, sans effet. Il peut être mis en cache ou préchargé par un navigateur.
- **POST** : crée ou déclenche.
- **PUT** : remplace entièrement.
- **PATCH** : modifie partiellement.
- **DELETE** : supprime.

### Sûreté et idempotence
- **Sûre** : ne modifie rien (GET).
- **Idempotente** : répéter donne le même état (GET, PUT, DELETE).

C'est important pour les réessais : si une réponse se perd, on peut rejouer un PUT sans risque, mais pas forcément un POST.

### Les familles de statuts
- **2xx** : succès. `200` OK, `201` créé, `204` sans contenu.
- **3xx** : redirection.
- **4xx** : erreur du client, réessayer tel quel ne sert à rien.
- **5xx** : erreur du serveur, réessayer peut marcher.

### Les statuts utiles dans matcha
- `400` : entrée invalide.
- `401` : pas authentifié.
- `403` : authentifié mais interdit, comme liker sans photo.
- `404` : introuvable, ou masqué pour un utilisateur bloqué.
- `409` : conflit.
- `413` : corps trop grand.
- `429` : trop de requêtes.
- `500` : bug.
- `503` : indisponible.

## Exemple déroulé : le 401 trompeur
`authenticatedAccountId` transforme `VerificationUnavailable` en `401` :

```text
1. Le JWKS ne répond plus
2. Le jeton de l'utilisateur est valide, mais invérifiable
3. Le serveur répond 401
4. Le front essaie /refresh, puis déconnecte
```

Une panne de quelques secondes déconnecte tout le monde. Avec `503`, le front affiche « réessaie plus tard », et la session reste intacte. Le code de statut est un contrat, pas un détail.

## Erreurs fréquentes
- Modifier des données sur un GET.
- Répondre `200` avec une erreur dans le corps.
- Exposer le détail d'un `500`.
- Confondre `401` et `403`.

## À retenir
Choisis le code selon ce que le client doit faire ensuite.

## Exercice
`DELETE /api/profiles/7/like` est appelé alors que le like n'existe plus. Que répondre ?

## Corrigé
`204`. DELETE est idempotent : l'état voulu, pas de like, est atteint. Répondre `404` compliquerait le front en cas de double clic, sans aucun bénéfice.

## Voir aussi

- [Concevoir une API REST](rest-conception.md)
- [Result et std::expected](../cpp/result-expected.md)
- [Middleware Crow et contexte de requête](middleware-crow.md)
