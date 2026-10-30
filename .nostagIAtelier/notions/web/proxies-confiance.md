# Proxies de confiance
> Un en-tête comme X-Real-IP n'a de valeur que s'il a été écrit par un proxy que tu contrôles.

## Mise en situation
[Nginx](../infra/nginx.md) écrit `X-Real-IP: $remote_addr`, et le [limiteur](../auth/rate-limiting.md) l'utilise (`AuthRouteSupport.cpp:166`). Si le backend est joignable directement, un attaquant envoie lui-même :

```text
X-Real-IP: 1.2.3.4
```

Il change la valeur à chaque essai, et le limiteur le prend pour des milliers de clients.

## La théorie
### Deux sources d'adresse
- **`remote_ip_address`** : l'adresse de la connexion TCP. Elle ne se falsifie pas, car TCP a besoin d'une réponse. Derrière un proxy, c'est l'adresse du proxy.
- **Les en-têtes** : de simples textes, écrits par n'importe qui.

Un en-tête n'est crédible que s'il vient d'un proxy dont on sait qu'il l'écrit lui-même.

### La règle
```cpp
std::string clientIp(const crow::request &req, const TrustedProxies &proxies) {
    if (proxies.contains(req.remote_ip_address))
        return req.get_header_value("X-Real-IP");
    return req.remote_ip_address;
}
```

Si l'appelant direct n'est pas un proxy connu, on ignore l'en-tête.

### Le rôle de Nginx
`proxy_set_header X-Real-IP $remote_addr;` remplace toute valeur du client. C'est essentiel.

### X-Forwarded-For
Cet en-tête accumule les adresses : `client, proxy1, proxy2`. Le client peut écrire ce qu'il veut au début. On lit depuis la droite, et on prend la première adresse qui n'est pas un proxy de confiance.

### L'isolation réseau
Dans Compose, seul [Nginx](../infra/nginx.md) doit publier un port. Le backend reste interne, et personne ne l'atteint sans passer par lui. Les deux protections se complètent.

## Exemple déroulé
```text
Avec TRUSTED_PROXIES=172.20.0.5 :

Client -> Nginx (172.20.0.5) -> Backend
  remote = 172.20.0.5, confiance, X-Real-IP utilisé : bonne IP

Attaquant -> Backend directement
  remote = 203.0.113.9, pas de confiance, en-tête ignoré
```

## Une adresse qui change
Une adresse de conteneur peut changer. On peut déclarer un sous-réseau en CIDR, ou un nom résolu au démarrage.

## Erreurs fréquentes
- Faire confiance à tout le réseau privé.
- Lire le premier élément de `X-Forwarded-For`.
- Oublier de publier seulement [Nginx](../infra/nginx.md).

## À retenir
Une adresse IP dans un en-tête n'est qu'une affirmation. Elle n'a de valeur que si son auteur est de confiance.

## Exercice
Pourquoi `remote_ip_address` est-il fiable alors que `X-Real-IP` ne l'est pas ?

## Corrigé
L'adresse TCP est établie par la poignée de main : avec une fausse adresse, la réponse ne reviendrait jamais et la connexion échouerait. Un en-tête est un texte arbitraire, envoyé après l'établissement.

## Voir aussi

- [Limitation de débit et Redis](../auth/rate-limiting.md)
- [Nginx en proxy inverse](../infra/nginx.md)
