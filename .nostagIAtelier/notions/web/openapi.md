# OpenAPI
> Un fichier décrit toute l'API : routes, paramètres, réponses et erreurs.

## Mise en situation
Pour connaître le format d'une réponse, il faut lire le C++. La documentation est dispersée dans le README, et elle dérive. Un `docs/openapi.yaml` devient le contrat entre le front et le back, et il se présente très bien en entretien.

## La théorie
OpenAPI est un format standard de description d'API HTTP. Il décrit :

- les chemins et méthodes ;
- les paramètres ;
- le corps des requêtes ;
- les réponses par statut ;
- les schémas de données ;
- l'authentification.

Des outils en font une documentation interactive (Swagger UI, Redoc), des clients générés, ou des tests.

## Exemple déroulé
```yaml
openapi: 3.0.3
info: {title: Matcha API, version: 0.1.0}
paths:
  /api/profiles/{id}/like:
    post:
      summary: Liker un profil
      security: [{bearerAuth: []}]
      parameters:
        - name: id
          in: path
          required: true
          schema: {type: integer}
      responses:
        "200":
          description: Like enregistré
          content:
            application/json:
              schema: {$ref: "#/components/schemas/LikeResult"}
        "403": {description: Pas de photo de profil}
components:
  schemas:
    LikeResult:
      type: object
      properties:
        liked: {type: boolean}
        matched: {type: boolean}
  securitySchemes:
    bearerAuth: {type: http, scheme: bearer}
```

`$ref` réutilise un schéma, pour éviter de le répéter.

## La maintenance
Un fichier écrit à la main dérive. La règle est de le modifier dans le même commit que la route. On peut le valider en [CI](../infra/ci-github-actions.md).

## Erreurs fréquentes
- Ne documenter que les cas réussis.
- Oublier les erreurs communes : `401`, `429`.

## À retenir
Un contrat écrit évite de deviner, et il force à concevoir l'API de façon cohérente.

## Exercice
Quels statuts documenter pour `POST /api/profiles/{id}/block` ?

## Corrigé
`204` en cas de réussite, `400` pour un id invalide, `401` sans authentification, `404` si le profil n'existe pas, et `409` si tu décides qu'un double blocage est un conflit. Bloquer étant naturellement [idempotent](http-bases.md), `204` convient aussi au double blocage.

## Voir aussi

- [Concevoir une API REST](rest-conception.md)
