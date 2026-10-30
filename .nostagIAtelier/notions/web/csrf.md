# CSRF et le contrôle d'Origin
> Un site tiers fait envoyer à ton navigateur une requête authentifiée, à ton insu.

## Mise en situation
Tu es connecté à matcha, puis tu ouvres un site piégé. Il contient :

```html
<form action="https://matcha.local/api/auth/logout-all" method="POST"></form>
<script>document.forms[0].submit()</script>
```

Le navigateur envoie la requête et ajoute ton [cookie](cookies.md), car il l'ajoute pour tout ce qui va vers ce domaine. Le serveur voit une requête authentifiée.

## La théorie
### La cause
Un [cookie](cookies.md) est une autorité ambiante : il accompagne les requêtes, sans que personne ne le demande. Le serveur ne sait pas si la requête vient de ton application.

### La politique de même origine
Une origine, c'est le schéma, le domaine et le port. Le navigateur empêche une page de lire les réponses d'une autre origine. Mais il ne l'empêche pas d'envoyer un formulaire. Le CSRF n'a pas besoin de lire la réponse, seulement de déclencher l'action.

### Les défenses
- **`SameSite=Strict`** : le [cookie](cookies.md) n'accompagne plus une requête venue d'ailleurs.
- **Contrôle d'`Origin`** : le navigateur ajoute cet en-tête, et une page ne peut pas le falsifier. Le serveur refuse tout ce qui n'est pas `allowedBrowserOrigin`.
- **Jeton anti-CSRF** : une valeur secrète, ajoutée à chaque formulaire.
- **Bearer** : une route protégée par `Authorization` n'est pas concernée, car le navigateur ne l'ajoute jamais automatiquement.

### CORS n'est pas une protection CSRF
CORS décide si une page d'une autre origine peut lire une réponse, et il ajoute une requête préalable pour certaines requêtes. Un formulaire simple part sans cette étape. Ne compte pas dessus.

## Exemple déroulé
Dans matcha, seul le [refresh token](../auth/refresh-rotation.md) est dans un [cookie](cookies.md). Les routes exposées sont donc `/api/auth/refresh`, `/logout` et `/logout-all`. Pour l'attaque ci-dessus :

```text
1. La requête part du site piégé
2. SameSite=Strict : le cookie n'est pas joint
3. Si un navigateur le joignait, Origin ne correspond pas : refus
```

Deux barrières indépendantes.

## Erreurs fréquentes
- Une action sensible sur un GET.
- Accepter un `Origin` absent sur une route sensible.
- Lire l'en-tête `Referer` seul.

## À retenir
Le CSRF abuse d'un navigateur qui ajoute le [cookie](cookies.md) tout seul. Évite les cookies pour l'authentification courante, et protège ceux qui restent.

## Exercice
Une attaque CSRF peut-elle lire le profil de la victime ?

## Corrigé
Non : la politique de même origine empêche de lire la réponse. Le CSRF sert à déclencher une action, pas à voler des données. Une [XSS](xss-echappement.md), elle, s'exécute dans la page, et peut tout lire.

## Voir aussi

- [Cookies : HttpOnly, Secure, SameSite](cookies.md)
- [HTTP : méthodes et codes de statut](http-bases.md)
