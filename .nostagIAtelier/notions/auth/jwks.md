# JWKS et StaticJwksProvider
> Un JWKS publie les clés publiques d'un émetteur en JSON, identifiées par un kid.

## Mise en situation
Le backend valide ses propres jetons en téléchargeant sa propre [clé publique](signature-rs256.md) :

```text
JWT_JWKS_URL=http://backend:18080/.well-known/jwks.json
```

C'est circulaire. Au démarrage, le serveur n'écoute pas encore. Et il faut `allowInsecureHttp = true`, une option dangereuse. Si la requête échoue, un jeton valide est traité comme invalide.

## La théorie
### Le format
```json
{"keys":[{"kty":"RSA","kid":"2026-1","use":"sig","alg":"RS256",
          "n":"0vx7ag...","e":"AQAB"}]}
```

`n` (module) et `e` (exposant) forment la [clé publique](signature-rs256.md) RSA, en Base64URL. `kid` est son identifiant.

### Le flux de vérification
1. Lire le `kid` dans l'en-tête du [JWT](jwt.md).
2. Chercher la clé dans le cache.
3. Si elle est absente, recharger le JWKS.
4. Vérifier la signature.

### La rotation
Pour changer de clé sans déconnecter tout le monde :

```text
Étape 1 : publier la nouvelle clé dans le JWKS
Étape 2 : signer avec la nouvelle
Étape 3 : garder l'ancienne publiée jusqu'à l'expiration des derniers jetons
Étape 4 : retirer l'ancienne
```

Les deux cohabitent, grâce au `kid`.

### L'interface JwksProvider
Le validateur dépend d'une [interface](../cpp/interfaces-injection.md). Il ne sait pas d'où viennent les clés.

- `HttpJwksProvider` : pour un émetteur tiers.
- `StaticJwksProvider` : pour un émetteur dans le même processus.

## Exemple déroulé
```cpp
RsaTokenSigner signer(clePrivee);
StaticJwksProvider keys(signer.publicJwk());
TokenValidator validator(keys, config);
```

Plus de réseau, plus de circularité, plus d'option dangereuse. `HttpClientConfig::allowInsecureHttp` disparaît. Le validateur n'a pas changé d'une ligne : c'est tout l'intérêt de l'[interface](../cpp/interfaces-injection.md).

## Le lien avec le 503
`HttpJwksProvider` peut renvoyer `VerificationUnavailable` si le réseau échoue. Ce cas n'est pas un jeton invalide, et il doit donner `503`. Avec le provider statique, il ne se produit plus. Le code reste correct pour d'autres providers.

## Erreurs fréquentes
- Recharger à chaque `kid` inconnu : un attaquant peut forcer des requêtes réseau en envoyant des `kid` inventés. Il faut un délai minimum entre deux rechargements.
- Autoriser HTTP en production.
- Oublier le cache.

## À retenir
Le JWKS sert à ceux qui ne partagent pas ton processus. Pour toi-même, une référence à ta clé suffit.

## Exercice
Pourquoi garder l'ancienne clé publiée pendant une [rotation](refresh-rotation.md), et combien de temps ?

## Corrigé
Des jetons signés par l'ancienne clé circulent encore, et ils doivent rester vérifiables. Il faut la garder au moins la [durée de vie](../cpp/duree-de-vie-objets.md) maximale d'un jeton, soit 15 minutes ici, plus une marge pour les caches.

## Voir aussi

- [Signatures asymétriques : RS256](signature-rs256.md)
- [JWT : les jetons d'accès](jwt.md)
- [HTTP : méthodes et codes de statut](../web/http-bases.md)
- [Interfaces et injection de dépendances](../cpp/interfaces-injection.md)
