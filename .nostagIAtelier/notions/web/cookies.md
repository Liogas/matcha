# Cookies : HttpOnly, Secure, SameSite
> Les attributs d'un cookie décident qui peut le lire et quand il est envoyé.

## Mise en situation
Le [refresh token](../auth/refresh-rotation.md) doit survivre à un rechargement de page, sans jamais être accessible au JavaScript, ni partir avec une requête venue d'un autre site. Un cookie sait faire cela, à condition de le configurer correctement.

## La théorie
Un cookie est déposé par le serveur, et renvoyé automatiquement par le navigateur :

```text
Set-Cookie: refresh=abc; HttpOnly; Secure; SameSite=Strict; Path=/api/auth; Max-Age=2592000
```

- **`HttpOnly`** : `document.cookie` ne le voit pas.
- **`Secure`** : envoyé seulement en HTTPS.
- **`SameSite`** : contrôle les requêtes venant d'autres sites.
- **`Path`** : limite les routes concernées.
- **`Max-Age`** : [durée de vie](../cpp/duree-de-vie-objets.md). Sans lui, le cookie meurt à la fermeture du navigateur.
- **`Domain`** : à omettre pour rester sur le domaine exact.

### SameSite en détail
- `Strict` : jamais envoyé depuis un autre site.
- `Lax` : envoyé lors d'une navigation de premier niveau en GET, comme un clic sur un lien.
- `None` : toujours envoyé, ce qui exige `Secure`.

### Ce que HttpOnly ne protège pas
Une [XSS](xss-echappement.md) ne peut pas lire le cookie, mais elle peut envoyer des requêtes depuis la page : elles partiront avec le cookie. HttpOnly empêche le vol du [secret](../infra/secrets-fichiers.md), pas son utilisation pendant que la page est compromise.

## Exemple déroulé
```text
1. Login : Set-Cookie refresh
2. Le JWT expire
3. Le front appelle POST /api/auth/refresh
4. Le navigateur ajoute le cookie, car le Path correspond
5. Un appel à /api/profile ne l'envoie pas
```

`Path=/api/auth` réduit son exposition.

## Supprimer un cookie
On envoie le même cookie avec `Max-Age=0`, et les mêmes `Path` et `Domain`. Sinon, le navigateur considère que c'est un autre cookie.

## L'injection d'en-tête
Une valeur placée dans `Set-Cookie` ne doit jamais contenir `\r\n`, sinon un attaquant ajoute ses propres en-têtes. Le token est donc validé strictement.

## Erreurs fréquentes
- `Secure` en HTTP local : le cookie n'est pas renvoyé.
- Stocker un jeton dans `localStorage`.
- Oublier `SameSite`.

## À retenir
Un cookie de session se configure toujours avec `HttpOnly`, `Secure`, `SameSite` et un `Path` étroit.

## Exercice
Pourquoi le jeton d'accès n'est-il pas aussi dans un cookie ?

## Corrigé
Il serait envoyé automatiquement à toutes les routes, ce qui exposerait toutes les actions au [CSRF](csrf.md). Avec `Authorization: Bearer`, seul notre JavaScript l'ajoute, donc une page tierce ne peut pas s'en servir.

## Voir aussi

- [CSRF et le contrôle d'Origin](csrf.md)
- [XSS et échappement](xss-echappement.md)
- [Rotation des refresh tokens](../auth/refresh-rotation.md)
