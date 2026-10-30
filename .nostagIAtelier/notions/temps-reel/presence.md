# Présence et dernière connexion
> En ligne signifie qu'au moins une connexion est ouverte. Sinon, on affiche la dernière date connue.

## Mise en situation
Le sujet demande de voir si quelqu'un est en ligne, et sinon sa dernière connexion. L'information change sans arrêt, mais personne n'exige une précision à la seconde. Toute la conception revient à éviter d'écrire en base à chaque mouvement.

## La théorie
### Définir en ligne
Une personne a plusieurs onglets. Elle est en ligne si au moins une connexion est ouverte : il faut un compteur, pas un booléen.

```text
onOpen  : compteur + 1 ; si passage de 0 à 1, en ligne
onClose : compteur - 1 ; si passage de 1 à 0, hors ligne
```

Sans compteur, fermer un onglet ferait passer hors ligne une personne encore présente dans un autre.

### Deux données, deux stockages
- `online` : vit en mémoire, très volatile.
- `last_seen_at` : vit en base, écrite rarement, voir le [debounce](heartbeat.md).

### Concurrence
Plusieurs threads ouvrent et ferment des connexions. Le compteur doit être protégé, sinon il dérive : c'est l'exemple de la fiche [data race](../cpp/data-race.md).

### Plusieurs instances
Une clé [Redis](../infra/redis.md) `presence:{id}` avec un TTL rafraîchi par le [heartbeat](heartbeat.md). Si un serveur plante, la clé expire seule, ce qu'un état écrit à la déconnexion ne ferait pas.

## Exemple déroulé
```cpp
void PresenceTracker::onClose(AccountId id) {
    bool offline = false;
    {
        std::lock_guard lock(_mutex);
        auto it = _counts.find(id);
        if (it != _counts.end() && --it->second == 0) {
            _counts.erase(it);
            offline = true;
        }
    }
    if (offline)
        _onOffline(id); // hors verrou : écrit last_seen_at
}
```

Le rappel écrit en base, donc il doit être hors verrou. Le test de passage à zéro se fait sous verrou, sinon deux threads pourraient tous les deux déclencher le rappel.

## Dans le profil
`GET /api/profiles/<id>` renvoie `online` et `last_seen_at`. Un utilisateur bloqué ne doit plus voir ces informations.

## Après un redémarrage
Les compteurs repartent de zéro, les clients se reconnectent, et l'état se reconstruit. C'est simple et correct.

## Erreurs fréquentes
- Un booléen au lieu d'un compteur.
- Écrire en base sous verrou.
- Oublier le cas d'une connexion morte, sans `onClose` : voir le [heartbeat](heartbeat.md).

## À retenir
Présence : en mémoire, approximative et bon marché. Dernière connexion : en base, rare et durable.

## Exercice
Que se passe-t-il si l'utilisateur ferme son unique onglet et en rouvre un 100 ms plus tard ?

## Corrigé
Il passe hors ligne, puis en ligne. Une notification de présence s'envoie deux fois, et `last_seen_at` est écrit. Pour éviter ce clignotement, on peut attendre quelques secondes avant de déclarer le passage hors ligne.

## Voir aussi

- [Heartbeat : détecter une connexion morte](heartbeat.md)
- [Data race et ThreadSanitizer](../cpp/data-race.md)
- [Redis](../infra/redis.md)
