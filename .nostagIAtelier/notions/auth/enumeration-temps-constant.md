# Énumération d'utilisateurs et temps constant
> Une réponse ne doit révéler, ni par son contenu ni par sa durée, si un compte existe.

## Mise en situation
Sur un site de rencontre, savoir qu'une adresse a un compte est déjà une information sensible : pense à un conjoint ou un employeur. Le login renvoie le même message dans les deux cas ? Regarde le temps de réponse :

```text
Compte inconnu : pas de hash vérifié      ->   2 ms
Compte réel    : Argon2 vérifie le hash   ->  80 ms
```

Un script compare les durées et sait quels comptes existent. C'est un canal auxiliaire, ou side channel : une information qui fuit par autre chose que la réponse.

## La théorie
### Les canaux d'énumération
- Le **message** : « mot de passe incorrect » contre « compte inconnu ».
- Le **statut** HTTP.
- La **durée** de réponse.
- Le **comportement** : un [limiteur](rate-limiting.md) qui ne compte que les comptes existants.

### La vérification factice
Quand le compte n'existe pas, on vérifie quand même un hash calculé d'avance :

```cpp
const auto compte = comptes.findByLogin(identifiant);
const std::string &hash = compte ? compte->passwordHash : _hashFactice;
const bool correct = hasher.verify(motDePasse, hash);
if (!compte || !correct)
    return erreurIdentifiants();
```

Le coût d'[Argon2](argon2id.md) est payé dans tous les cas, et le message est identique. Le hash factice doit utiliser les mêmes paramètres : sinon la durée diffère.

### Comparaisons de secrets
`==` s'arrête au premier octet différent. En mesurant le temps, un attaquant pourrait deviner un [secret](../infra/secrets-fichiers.md), octet par octet. Pour des secrets bruts, on utilise `sodium_memcmp`, qui lit tout. Pour un token, on compare plutôt des hashs : l'attaquant ne contrôle pas le résultat du hash.

## Exemple déroulé : les autres routes
**Reset de mot de passe.** La route répond toujours la même chose :

```text
Si ce compte existe, un email a été envoyé.
```

Mais si l'email est envoyé dans la requête, la durée trahit le compte : 600 ms contre 3 ms. Une file d'envoi, voir l'[outbox](../temps-reel/outbox.md), règle aussi ce problème.

**Inscription.** `409 Email already in use` révèle qu'un compte existe. Le sujet n'exige pas de masquer cela. Le masquer demanderait d'envoyer un email dans tous les cas. C'est un compromis à connaître et à expliquer.

**Login par nom d'utilisateur.** Les noms sont publics sur les profils, donc l'énumération a moins d'impact. La vérification factice reste simple, et elle s'applique de la même façon à l'email.

## Erreurs fréquentes
- Retourner tôt quand le compte est absent.
- Un message différent sur le statut non vérifié avant de contrôler le mot de passe : il dévoilerait qu'un compte existe à quelqu'un qui n'a pas le mot de passe.
- Un [limiteur](rate-limiting.md) qui ignore les comptes inconnus.

## À retenir
Pense en attaquant : tout ce qui diffère, même 50 ms, est une réponse à la question « ce compte existe-t-il ? ».

## Exercice
Pourquoi faut-il vérifier le mot de passe avant de dire qu'un compte n'est pas vérifié ?

## Corrigé
Sinon, n'importe qui peut apprendre l'état d'un compte en donnant un mauvais mot de passe. Après vérification, on sait qu'on parle au vrai propriétaire, et le message est alors utile et sans risque.

## Voir aussi

- [Argon2id : stocker un mot de passe](argon2id.md)
- [Limitation de débit et Redis](rate-limiting.md)
- [L'outbox et publier après le commit](../temps-reel/outbox.md)
