# JWT : les jetons d'accès
> Un jeton signé contient l'identité de l'utilisateur, et le serveur peut le vérifier sans consulter la base.

## Mise en situation
À chaque requête, le serveur doit savoir qui appelle. Avec une session classique, il lit une table à chaque fois. Un JWT, JSON Web Token, transporte directement l'information, avec une preuve qu'elle n'a pas été modifiée.

## La théorie
### Structure
Un JWT comporte trois parties en Base64URL, séparées par des points :

```text
eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiI0MiJ9.signature
```

```json
{"alg":"RS256","typ":"JWT","kid":"2026-1"}
{"sub":"42","iss":"matcha","aud":"matcha-api","iat":1790000000,"exp":1790000900}
```

La signature couvre les deux premières parties. Modifier un seul caractère l'invalide. Mais rien n'est chiffré : tout le monde peut lire le contenu. Aucun [secret](../infra/secrets-fichiers.md) ne doit s'y trouver.

### Les claims
- `sub` : le sujet, l'id du compte.
- `iss` : qui a émis le jeton.
- `aud` : pour qui il est prévu.
- `exp` : expiration.
- `iat` : date d'émission.
- `jti` : identifiant unique.

### La vérification
`TokenValidator` vérifie dans l'ordre : structure, algorithme attendu, clé (`kid`), signature, `exp`, `iss`, `aud`. Un seul échec suffit à refuser.

### L'attaque sur l'algorithme
L'en-tête est fourni par le client. Si le serveur le croit :

- `alg: none` : pas de signature du tout ;
- `alg: HS256` : l'attaquant signe en HMAC, avec la [clé publique](signature-rs256.md) comme [secret](../infra/secrets-fichiers.md). Le serveur vérifie avec cette même clé publique, et le jeton passe.

La parade est simple : le vérificateur impose RS256, quelle que soit la valeur de l'en-tête. Voir RS256.

### Pourquoi une durée courte
Un JWT reste valide jusqu'à `exp`, même après une déconnexion, car le serveur ne consulte rien. On limite donc sa durée, à 15 minutes dans le projet, et un [refresh token](refresh-rotation.md) le renouvelle. C'est le compromis : rapide et sans état, mais non révocable.

## Exemple déroulé
```text
1. Connexion : le serveur signe un JWT de 15 minutes
2. Le front garde le JWT en mémoire
3. Chaque appel : Authorization: Bearer <JWT>
4. Le serveur vérifie, sans requête SQL, et obtient sub = 42
5. Après 15 minutes : 401, le front appelle /api/auth/refresh
```

Le JWT reste en mémoire JavaScript, et jamais dans `localStorage`, où une [XSS](../web/xss-echappement.md) le lirait durablement.

## Erreurs fréquentes
- Mettre un JWT dans une URL : il finit dans les logs. C'est le cas du [WebSocket](../temps-reel/websocket.md) actuel, voir le [ticket WebSocket](../temps-reel/ticket-websocket.md).
- Oublier de vérifier `aud`.
- Stocker des droits qui changent : ils restent périmés jusqu'à `exp`.

## À retenir
Un JWT est une carte d'identité signée et lisible par tous, à durée courte.

## Exercice
Un utilisateur se déconnecte. Quelqu'un a copié son JWT. Que peut-il faire, et pendant combien de temps ?

## Corrigé
Il peut agir à sa place jusqu'à `exp`, soit 15 minutes au plus, car la déconnexion ne touche que le [refresh token](refresh-rotation.md). Pour faire mieux, il faudrait une liste de révocation consultée à chaque requête, ce qui retire une partie de l'intérêt du JWT.

## Voir aussi

- [Signatures asymétriques : RS256](signature-rs256.md)
- [JWKS et StaticJwksProvider](jwks.md)
- [Rotation des refresh tokens](refresh-rotation.md)
- [Le ticket WebSocket](../temps-reel/ticket-websocket.md)
