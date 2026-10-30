# Le pool de connexions
> Un ensemble de connexions ouvertes d'avance, prêtées aux requêtes puis réutilisées.

## Mise en situation
Avant cpp-pg, tout le serveur partageait une seule connexion, protégée par un [mutex](../cpp/mutex-contention.md). Vingt utilisateurs qui chargent leur profil faisaient donc la queue, et une requête lente bloquait tout le monde. L'autre extrême, une connexion par requête, n'est pas mieux : chaque ouverture coûte plusieurs millisecondes.

## La théorie
### Pourquoi une connexion coûte cher
Ouvrir une connexion, c'est établir TCP, négocier éventuellement TLS, s'authentifier, puis créer un processus côté serveur. PostgreSQL dédie un processus par connexion, avec sa mémoire. Il en supporte 100 par défaut (`max_connections`).

### Le principe du pool
```text
Démarrage : ouvre N connexions -> file des connexions libres
Requête   : prend une connexion, l'utilise, la rend
Saturation: attend, puis échoue après un délai
```

Le nombre de requêtes simultanées en base est borné à N. Les autres attendent en mémoire, ce qui protège PostgreSQL d'une surcharge.

### La taille
Plus de connexions ne veut pas dire plus de débit. Au-delà du nombre de cœurs de la base, les processus se disputent le CPU et les verrous. Une formule de départ, popularisée par le projet HikariCP, est : nombre de cœurs × 2 + nombre de disques. Pour matcha, 4 convient très bien.

### L'emprunt sûr
`acquire()` renvoie une `PooledConnection`, un objet [RAII](../cpp/raii.md) qui rend la connexion à sa destruction. L'attente repose sur une [condition_variable](../cpp/condition-variable.md) avec délai.

### Le retour d'une connexion
Rendre une connexion demande une vérification. Si `PQstatus` indique une connexion cassée, le pool appelle `PQreset`. Ainsi une panne de la base se répare toute seule quand elle revient.

## Exemple déroulé
```cpp
pg::PoolOptions options;
options.size = 4;
options.acquireTimeout = std::chrono::milliseconds{2000};
auto pool = pg::ConnectionPool::open(settings, options, logger);
```

Scénario avec 5 requêtes simultanées :

```text
R1..R4 : prennent une connexion
R5     : dort dans acquire()
R2     : termine, sa PooledConnection meurt, notify_one
R5     : se réveille et prend cette connexion
```

Le `connectAttempts = 12` du démarrage existe parce que Docker peut lancer le backend avant que PostgreSQL soit prêt.

## Le pool comme Executor
`ConnectionPool` hérite de Executor. `pool->query(...)` emprunte une connexion le temps de la requête. C'est idéal pour une lecture isolée. Pour plusieurs requêtes liées, on utilise `pool->begin()`.

## Le piège : deux connexions pour un seul travail
```cpp
auto tx = pool->begin();
LikeRepository likes(*tx);
ProfileRepository profiles(*pool);   // erreur !
```

`profiles` utilise une seconde connexion. Elle ne voit pas les écritures de `tx`. Et si tous les threads font cela, chacun tient une connexion et en attend une autre : c'est un [deadlock](deadlock.md) de pool, voir le deadlock. Dans une [transaction](transactions.md), tout doit passer par `*tx`.

## Erreurs fréquentes
- Tenir une connexion pendant un envoi d'email.
- Détruire le pool avant la fin des requêtes : déclare-le avant le serveur dans `main`.
- Mettre 100 connexions « pour être tranquille ».

## À retenir
Le pool borne la charge sur la base et réutilise un objet coûteux. Dans une [transaction](transactions.md), il n'existe plus : tout passe par elle.

## Exercice
Le serveur renvoie des `503` sous charge, mais PostgreSQL est à 5 % de CPU. Quelle est ta première hypothèse ?

## Corrigé
Les connexions sont occupées sans travailler en base : une requête tient une connexion pendant quelque chose de lent, comme un email ou un calcul, ou une [transaction](transactions.md) reste ouverte. On cherche ce qui garde une connexion trop longtemps. Agrandir le pool ne ferait que repousser le problème.

## Voir aussi

- [std::condition_variable](../cpp/condition-variable.md)
- [RAII](../cpp/raii.md)
- [Les transactions et ACID](transactions.md)
- [libpq, l'API C de PostgreSQL](libpq.md)
