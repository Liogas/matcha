# Signatures asymétriques : RS256
> On signe avec une clé privée, et n'importe qui peut vérifier avec la clé publique.

## Mise en situation
Le backend signe des [JWT](jwt.md). Si un second service doit les vérifier, avec une signature symétrique, il lui faudrait le même [secret](../infra/secrets-fichiers.md), ce qui lui permettrait aussi d'en fabriquer. Une fuite chez lui compromettrait tout le système. Il faut un système où vérifier ne permet pas de signer.

## La théorie
### Symétrique ou asymétrique
- **Symétrique**, comme HS256 (HMAC) : une seule clé signe et vérifie.
- **Asymétrique**, comme RS256 : une paire de clés mathématiquement liées. La clé privée signe, la clé publique vérifie, et on ne peut pas en pratique déduire la privée de la publique.

### Comment fonctionne RS256
RS256 signifie RSA avec SHA-256 :

```text
Signature :    empreinte = SHA-256(entête.contenu)
               signature = RSA_chiffre(empreinte, clé privée)
Vérification : empreinte' = RSA_déchiffre(signature, clé publique)
               comparer avec SHA-256(entête.contenu)
```

Seul le possesseur de la clé privée a pu produire une signature qui se vérifie avec la clé publique. La sécurité de RSA repose sur la difficulté de factoriser un très grand nombre. On utilise au moins 2048 bits.

### Signature et chiffrement
Une signature garantit l'origine et l'intégrité. Elle ne cache rien. Un [JWT](jwt.md) signé reste lisible.

### Les alternatives
- **ES256** : courbes elliptiques, signatures plus courtes.
- **EdDSA (Ed25519)** : rapide et simple.

Ces algorithmes demandent des clés plus courtes pour une sécurité équivalente. Grâce à `ITokenSigner` et `ITokenSignatureVerifier`, un changement ne toucherait pas le reste.

## Exemple déroulé
```text
secrets/jwt-private.pem     : lu par le backend, jamais dans Git
/.well-known/jwks.json      : expose la clé publique
```

Un attaquant modifie `sub` dans un [JWT](jwt.md) :

1. Il change le contenu, qu'il peut lire.
2. Il ne peut pas produire la nouvelle signature, faute de clé privée.
3. Le serveur recalcule l'empreinte : elle ne correspond plus.
4. Réponse : `401`.

## La gestion des clés
- La clé privée vit dans un fichier [secret](../infra/secrets-fichiers.md), avec des droits restreints.
- Une clé différente par environnement.
- Le `kid` permet de changer de clé, voir le [JWKS](jwks.md).
- Une clé privée qui fuite oblige à tout changer.

## Erreurs fréquentes
- Accepter l'algorithme indiqué dans le jeton, voir le [JWT](jwt.md).
- Committer la clé.
- Confondre encodage, signature et chiffrement.

## À retenir
Asymétrique veut dire : un seul peut signer, tout le monde peut vérifier.

## Exercice
Quelle différence y a-t-il entre Base64URL et chiffrement ?

## Corrigé
Base64URL est un simple encodage, réversible par tous, sans clé, pour faire passer des octets dans une URL. Un chiffrement exige une clé pour lire. Le contenu d'un [JWT](jwt.md) est donc public.

## Voir aussi

- [JWT : les jetons d'accès](jwt.md)
- [JWKS et StaticJwksProvider](jwks.md)
- [Secrets et fichiers .env](../infra/secrets-fichiers.md)
