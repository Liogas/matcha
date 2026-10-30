# Rotation des refresh tokens
> Chaque refresh token ne sert qu'une fois, et sa réutilisation signale un vol.

## Mise en situation
Un [JWT](jwt.md) de 15 minutes oblige à se reconnecter sans arrêt. On ajoute un refresh token, à longue [durée de vie](../cpp/duree-de-vie-objets.md), dans un [cookie](../web/cookies.md). Mais s'il est volé, l'attaquant garde l'accès pendant des semaines, sans que personne ne le sache.

## La théorie
### Deux jetons, deux rôles
- **Jeton d'accès** : courte durée, envoyé à chaque requête, vérifié sans base.
- **Refresh token** : longue durée, envoyé à une seule route, vérifié en base, donc révocable.

### La rotation
À chaque utilisation :

1. Le serveur vérifie le token, stocké haché.
2. Il le marque comme consommé.
3. Il émet un nouveau token dans la même session.

### La détection de vol
```text
Victime et attaquant ont R1.
L'attaquant utilise R1 : il obtient R2.
La victime utilise R1 : déjà consommé !
```

Un token consommé qui réapparaît prouve que deux personnes le possèdent. Le serveur ne sait pas laquelle est légitime, alors il révoque toute la session. La victime doit se reconnecter, et l'attaquant perd R2.

Si la victime rafraîchit en premier, c'est l'attaquant qui déclenche l'alerte. Dans les deux cas, le vol se termine.

### Stockage
Le token est un [jeton opaque](tokens-opaques.md), stocké en SHA-256. Une fuite de la table ne donne aucune session. Dans le navigateur, il vit dans un [cookie](../web/cookies.md) `HttpOnly`, `SameSite=Strict`.

### Expirations
- **Idle** : expire sans activité.
- **Absolue** : expire de toute façon.

## Exemple déroulé : le bug d'ordre
Le code actuel de `handleRefreshSession` fait :

```text
1. rotateRefreshToken   -> ancien consommé
2. tokenGenerator.generate
3. renvoie les deux au client
```

Si l'étape 2 échoue, le client n'a rien reçu, et son token est consommé. Sa prochaine tentative ressemble à un vol, et il perd sa session. Correction (audit 4.11) :

```text
1. Générer le JWT
2. Faire la rotation
3. Renvoyer les deux
```

L'étape qui peut échouer sans effet passe en premier.

Reste le cas d'une réponse perdue sur le réseau. Une petite période de tolérance, pendant laquelle l'ancien token reste accepté, le couvre.

## Deux onglets
Deux onglets rafraîchissent en même temps avec le même token : le second ressemble à un vol. Il faut une tolérance de quelques secondes, ou un verrou côté front.

## Erreurs fréquentes
- Stocker les tokens en clair.
- Oublier de révoquer les sessions après un changement de mot de passe.
- Envoyer le [cookie](../web/cookies.md) sur tout le site : `Path` le limite.

## À retenir
La rotation transforme un [secret](../infra/secrets-fichiers.md) à longue durée en une suite de secrets à usage unique : un vol devient détectable.

## Exercice
Pourquoi un SHA-256 suffit-il pour le refresh token, alors qu'un mot de passe exige [Argon2](argon2id.md) ?

## Corrigé
Un mot de passe a peu d'entropie : on peut tester un dictionnaire. Un token de 256 bits aléatoires n'est dans aucun dictionnaire, et le tester par force brute est impossible. Une fonction rapide suffit.

## Voir aussi

- [Les tokens opaques à usage unique](tokens-opaques.md)
- [JWT : les jetons d'accès](jwt.md)
- [Cookies : HttpOnly, Secure, SameSite](../web/cookies.md)
- [Les transactions et ACID](../sql/transactions.md)
