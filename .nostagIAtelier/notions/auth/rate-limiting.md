# Limitation de débit et Redis
> On borne le nombre d'essais par fenêtre de temps, avec un compteur atomique qui expire tout seul.

## Mise en situation
Un script essaie un dictionnaire sur un compte, ou bombarde `/api/auth/login`, où chaque essai coûte 64 Mo de RAM et 80 ms de CPU. Il faut le ralentir sans gêner les utilisateurs normaux. matcha a déjà deux étages :

- [Nginx](../infra/nginx.md) : 5 requêtes par minute par IP sur le login ;
- `RedisLoginAttemptLimiter` : un compteur par couple compte et IP.

## La théorie
### Les algorithmes
- **Fenêtre fixe** : un compteur remis à zéro toutes les N secondes. Simple, mais une rafale à cheval sur deux fenêtres passe.
- **Fenêtre glissante** : plus précise, plus coûteuse.
- **Token bucket** : un seau se remplit régulièrement, chaque requête prend un jeton. Il tolère de petites rafales. C'est le modèle de `limit_req` de [Nginx](../infra/nginx.md), avec son paramètre `burst`.

### Pourquoi Redis
Le compteur doit être partagé entre threads et instances, très rapide, et expirer tout seul. [Redis](../infra/redis.md) offre `INCR` et `EXPIRE`.

### L'atomicité
```lua
local count = redis.call('INCR', KEYS[1])
if count == 1 then
    redis.call('EXPIRE', KEYS[1], ARGV[1])
end
if count > tonumber(ARGV[2]) then
    return {0, redis.call('TTL', KEYS[1])}
end
return {1, 0}
```

[Redis](../infra/redis.md) exécute un script en entier, sans s'interrompre. Si `INCR` et `EXPIRE` étaient deux commandes séparées, un plantage entre les deux laisserait une clé sans expiration : le compte resterait bloqué pour toujours.

### Le choix de la clé
- Par IP : un réseau d'entreprise partage une adresse, et tout le monde est bloqué.
- Par compte : un attaquant peut verrouiller la victime exprès.
- Par couple : le compromis du code. La limite globale par IP vient de [Nginx](../infra/nginx.md).

La clé contient un hachage, pour éviter d'écrire des emails en clair dans [Redis](../infra/redis.md).

## Exemple déroulé
Limite : 5 essais en 300 secondes.

```text
Essai 1 : INCR -> 1, EXPIRE 300, autorisé
Essai 2 à 5 : INCR -> 2..5, autorisé
Essai 6 : INCR -> 6, refusé, TTL 240
```

Le serveur répond `429` avec `Retry-After: 240`. Après une connexion réussie, `clear` supprime la clé.

## Dépendance à l'IP
Le limiteur est aussi fiable que l'adresse reçue. Tant que [`X-Real-IP`](../web/proxies-confiance.md) est lu sans contrôle, un attaquant change d'IP à chaque requête, voir les proxies de confiance.

## Erreurs fréquentes
- Ne limiter que les comptes existants : voir l'[énumération](enumeration-temps-constant.md).
- Oublier le reset de mot de passe et les renvois d'email.
- Ne pas décider du comportement si [Redis](../infra/redis.md) tombe : pour un login, refuser en `503` est prudent.

## À retenir
Une limite efficace est atomique, expire seule, et repose sur une identité fiable.

## Exercice
Un attaquant teste 1000 comptes avec un seul mot de passe, depuis une seule IP. Le limiteur par couple suffit-il ?

## Corrigé
Non : chaque couple n'a qu'un essai, donc aucun seuil n'est atteint. C'est une pulvérisation de mots de passe. La limite par IP de [Nginx](../infra/nginx.md) (5 requêtes par minute) couvre ce cas : d'où l'intérêt des deux étages.

## Voir aussi

- [Proxies de confiance](../web/proxies-confiance.md)
- [Nginx en proxy inverse](../infra/nginx.md)
- [Redis](../infra/redis.md)
- [Énumération d'utilisateurs et temps constant](enumeration-temps-constant.md)
