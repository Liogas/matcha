# Réauthentification et prise de compte
> Une action sensible exige de reprouver son identité, même avec une session valide.

## Mise en situation
Quelqu'un trouve ton téléphone déverrouillé, avec matcha ouvert. S'il peut changer l'email sans vérification, il déclenche ensuite un reset vers sa propre adresse, et le compte lui appartient pour toujours. C'est un account takeover.

## La théorie
### Session et identité
Une session prouve qu'un navigateur s'est connecté à un moment donné. Elle ne prouve pas que la personne actuelle est la même. Pour les actions irréversibles, on redemande une preuve.

### Les actions sensibles
Changer d'email, changer de mot de passe, supprimer le compte, déconnecter tous les appareils.

### Le changement d'email
```text
1. POST /api/account/email-change { new_email, password }
2. Vérifier le mot de passe
3. Créer un token lié à new_email
4. Envoyer le lien à la nouvelle adresse
5. Envoyer une alerte à l'ancienne
6. Au clic : basculer
```

Sans l'étape 2, un voleur de session réussit. Sans l'étape 4, on pourrait associer n'importe quelle adresse, sans en être propriétaire. Sans l'étape 5, la victime ne sait rien.

## Exemple déroulé : la bascule
```sql
UPDATE auth_accounts SET email = $1
WHERE id = $2;
```

Entre la demande et le clic, quelqu'un a pu prendre cette adresse : l'[index](../sql/index.md) unique sur `LOWER(email)` refuse, avec `23505`, qui donne `409`. Voir [SQLSTATE](../sql/sqlstate.md).

Dans la même [transaction](../sql/transactions.md), on consomme le token, puis on révoque les autres sessions. Si l'attaquant avait une session ouverte, elle tombe.

## Erreurs fréquentes
- Basculer avant le clic.
- Laisser un lien valide après un changement de mot de passe.
- Traiter l'alerte comme optionnelle.
- Oublier le [limiteur](rate-limiting.md) : la vérification du mot de passe est aussi une cible.

## À retenir
Plus une action est difficile à annuler, plus elle exige une preuve récente, et une trace visible par le propriétaire.

## Exercice
Pourquoi ne pas envoyer le lien de confirmation à l'ancienne adresse, plutôt qu'à la nouvelle ?

## Corrigé
Le but est de prouver que l'utilisateur contrôle la nouvelle adresse. L'ancienne reçoit l'alerte, qui permet de réagir. Dans le cas d'une ancienne adresse perdue, le lien y serait inutile.

## Voir aussi

- [Les tokens opaques à usage unique](tokens-opaques.md)
- [Rotation des refresh tokens](refresh-rotation.md)
