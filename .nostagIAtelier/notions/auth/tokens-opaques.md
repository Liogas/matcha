# Les tokens opaques à usage unique
> Une longue valeur aléatoire sans signification, stockée hachée, consommée une fois.

## Mise en situation
La vérification d'email, le reset de mot de passe et le changement d'email ont le même besoin : un lien impossible à deviner, limité dans le temps, utilisable une fois. `OpaqueToken` et `EmailVerificationToken` font aujourd'hui presque la même chose en double.

## La théorie
### Opaque
Un token opaque ne contient aucune information. Sa valeur n'est qu'une clé vers une ligne en base. Contrairement à un [JWT](jwt.md), le serveur doit consulter la base, mais il peut le révoquer.

### Le cycle de vie
```text
1. token = 32 octets aléatoires, écrits en 64 caractères hexadécimaux
2. Base : SHA-256(token), compte, expires_at, used_at
3. Email : lien contenant le token en clair
4. Clic : SHA-256(reçu), recherche, consommation
```

### Pourquoi l'aléa cryptographique
`randombytes_buf` de libsodium lit l'aléa du système, imprévisible. `rand()` ou `std::mt19937` sont déterministes : observer quelques valeurs permet de prédire les suivantes. Ils ne conviennent jamais à un [secret](../infra/secrets-fichiers.md).

Avec 256 bits, deviner est impossible : 2 puissance 256 possibilités.

### Pourquoi hacher en base
Si la table fuite, les liens de reset en cours ne servent à rien : l'attaquant a l'empreinte, pas le token. SHA-256 suffit, car un token aléatoire n'est pas attaquable par dictionnaire.

## Exemple déroulé : consommer sans course
Mauvais :

```text
T1 : SELECT ... not used -> valide
T2 : SELECT ... not used -> valide
T1 : UPDATE used_at
T2 : UPDATE used_at
```

Le lien a servi deux fois. Bon, en une instruction :

```sql
UPDATE password_reset_tokens
SET used_at = NOW()
WHERE token_hash = $1 AND used_at IS NULL AND expires_at > NOW()
RETURNING account_id;
```

Une seule [transaction](../sql/transactions.md) peut modifier la ligne, car la seconde attend, puis réévalue `used_at IS NULL`, qui est devenu faux. Une ligne renvoyée signifie un succès.

## Fusionner les deux types
Un module de cpp-auth propose un générateur unique, qui produit le token et son empreinte. Les différences entre usages (durée, table) sont des paramètres.

## Erreurs fréquentes
- Token dans une URL partagée : durée courte et `Referrer-Policy` limitent le risque.
- Valider le format après usage.
- Laisser un ancien token valide lors d'une nouvelle demande : invalide les précédents.

## À retenir
Un token à usage unique, c'est de l'aléa fort, une empreinte en base, et une consommation atomique.

## Exercice
Pourquoi une page intermédiaire avec un bouton de confirmation est-elle utile pour la vérification d'email ?

## Corrigé
Certains clients de messagerie et antivirus ouvrent les liens automatiquement. Une action déclenchée par un simple GET serait consommée sans l'utilisateur. Un bouton impose un POST, et donc une intention réelle.

## Voir aussi

- [Rotation des refresh tokens](refresh-rotation.md)
- [Réauthentification et prise de compte](reauthentification.md)
- [Requêtes paramétrées et injection SQL](../sql/requetes-parametrees.md)
