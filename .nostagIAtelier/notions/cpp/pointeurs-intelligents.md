# unique_ptr, shared_ptr, weak_ptr
> Ces trois types répondent à la même question : qui possède cet objet, et qui le détruit ?

## Mise en situation
Le hub de messagerie garde des pointeurs bruts :

```cpp
std::unordered_map<std::int64_t, std::set<crow::websocket::connection *>> _connections;
*userData = new std::int64_t(*account);
```

Lis ces deux lignes en te demandant qui libère quoi. Le type ne dit rien. Le `new` doit être compensé par un `delete` à la fermeture. Et si un client se déconnecte alors qu'un autre thread parcourt la liste, le pointeur désigne peut-être un objet détruit. Un pointeur brut n'exprime aucune propriété : il dépend d'une convention dans la tête du développeur.

### Le crash que le type aurait évité
Voici comment le hub actuel peut planter, avec deux threads et une déconnexion au mauvais moment.

```tableau
titre: Pointeur brut : l'objet meurt pendant l'envoi
colonnes: Thread d'envoi | Liste | Thread de fermeture | Connexion
étape: Le thread d'envoi lit l'adresse d'une connexion dans la liste.
Thread d'envoi -> Liste: lit connection*
étape: Au même moment, le client se déconnecte. Crow détruit l'objet.
Thread de fermeture -> Connexion: détruit
! Connexion: mémoire libérée
étape: Le pointeur brut garde son adresse. Il ne peut pas savoir que l'objet n'existe plus.
! Thread d'envoi -> Connexion: send_text
étape: Écriture dans une mémoire libérée : crash, ou corruption silencieuse.
! = use-after-free
```

```tableau
titre: Avec weak_ptr et shared_ptr
colonnes: Thread d'envoi | Bloc de contrôle | Thread de fermeture
étape: La liste contient des `weak_ptr` : ils observent sans posséder.
Thread d'envoi -> Bloc de contrôle: lock()
étape: Si l'objet vit encore, `lock()` renvoie un `shared_ptr`, et le thread devient copropriétaire.
+ Bloc de contrôle -> Thread d'envoi: shared_ptr
étape: La fermeture retire sa propre référence. L'objet survit tant que l'envoi est en cours.
Thread de fermeture -> Bloc de contrôle: reset
étape: L'envoi finit proprement. Le dernier propriétaire détruit l'objet en sortant.
+ Thread d'envoi: objet détruit ici
étape: Si la session était déjà morte, `lock()` aurait renvoyé un pointeur vide, et le thread l'aurait simplement ignorée.
+ = plus de use-after-free
```

## La théorie
### unique_ptr : un seul propriétaire
```cpp
auto pool = std::make_unique<Pool>();
```

Il libère l'objet à sa mort, ne coûte rien de plus qu'un pointeur brut, et ne se copie pas. Il se déplace, ce qui transfère la propriété. C'est le choix par défaut.

`ConnectionPool::open` renvoie un `unique_ptr<ConnectionPool>` : l'appelant possède le [pool](../sql/pool-connexions.md), sans ambiguïté.

### shared_ptr : propriété partagée
Plusieurs `shared_ptr` désignent le même objet. Un compteur de références, stocké dans un bloc de contrôle, compte les propriétaires. Le dernier qui meurt détruit l'objet.

```text
shared_ptr A --+
shared_ptr B --+--> bloc de contrôle (compte = 3) --> objet
shared_ptr C --+
```

Le compteur est atomique, donc l'incrémenter depuis plusieurs threads est sûr. Attention : seul le compteur est protégé. L'objet pointé reste à protéger contre les accès concurrents.

### weak_ptr : observer sans posséder
Un `weak_ptr` pointe vers le bloc de contrôle sans compter comme propriétaire. Il ne garde pas l'objet en vie, mais il sait s'il est mort. Pour l'utiliser, on appelle `lock()` :

```cpp
if (auto session = faible.lock())
    session->send(message);       // vivant, et gardé en vie jusqu'au }
```

`lock()` renvoie un `shared_ptr`, vide si l'objet a disparu. Entre le test et l'usage, l'objet ne peut plus mourir, car on en est devenu copropriétaire.

### Le cycle
Si A possède B et B possède A en `shared_ptr`, les compteurs ne tombent jamais à zéro : la mémoire fuit. Un des deux côtés doit être un `weak_ptr`, typiquement l'enfant qui regarde son parent.

## Exemple déroulé : le hub de cpp-realtime
Le problème est une course entre un envoi et une fermeture :

```text
Thread 1 : publish() copie la liste des sessions
Thread 2 : le client se ferme, sa session est détruite
Thread 1 : session->send()  -> use-after-free avec un pointeur brut
```

Avec `weak_ptr`, la dernière ligne devient sûre :

```cpp
std::vector<std::weak_ptr<ISession>> cibles;
{
    std::lock_guard lock(_mutex);
    cibles = _topics[topic];
}
for (auto &faible : cibles)
    if (auto session = faible.lock())
        session->send(payload);
```

Déroulons le cas où la fermeture arrive pendant la boucle :

1. Le hub retire la session, et le dernier `shared_ptr` meurt : la session est détruite.
2. `lock()` renvoie un pointeur vide, et on passe à la suivante.

Si la fermeture arrive juste après `lock()`, notre `shared_ptr` local garde la session jusqu'au `}`. Dans les deux cas, il n'y a pas d'accès à une mémoire libérée. L'envoi se fait hors verrou.

## Le deleter
`unique_ptr` peut appeler autre chose que `delete`, ce qui est idéal pour une ressource C :

```cpp
struct PgConnDeleter {
    void operator()(PGconn *c) const { PQfinish(c); }
};
std::unique_ptr<PGconn, PgConnDeleter> _handle;
```

## Lequel choisir
1. Un objet local ou un membre simple : pas de pointeur.
2. Un propriétaire unique, avec allocation : `unique_ptr`.
3. Une vie que personne ne contrôle seul : `shared_ptr`.
4. Un observateur qui peut survivre à l'objet : `weak_ptr`.
5. Un observateur qui vit moins longtemps que l'objet : `T &` ou `T *` brut.

Un pointeur brut reste correct s'il observe sans posséder. C'est `new` et `delete` à la main qu'il faut éviter.

## Exemple pas à pas : quel pointeur choisir ?
Pose les questions dans cet ordre. Elles règlent presque tous les cas du backend.

```tableau
titre: Choisir un type de pointeur
étape: L'objet peut-il être une simple valeur, membre de sa classe ? C'est toujours le plus simple.
+ = oui : pas de pointeur du tout
étape: Un seul propriétaire clairement identifié ? Exemple : `main` possède le pool.
+ = oui : std::unique_ptr
étape: Plusieurs propriétaires qui peuvent mourir dans n'importe quel ordre ? Exemple : une session vue par plusieurs threads.
= oui : std::shared_ptr
étape: Tu dois seulement regarder un objet possédé ailleurs, sans le garder en vie ?
= oui : std::weak_ptr, ou une référence si sa durée de vie est garantie
```

## Erreurs fréquentes
- Deux `shared_ptr` créés depuis le même pointeur brut : deux compteurs, double destruction. Utilise `make_shared`.
- Passer un `shared_ptr` par valeur partout : coût atomique inutile. Passe `const T &` si la fonction ne garde rien.
- Choisir `shared_ptr` par réflexe : la propriété devient floue.

## À retenir
Le type du pointeur documente la propriété, et le compilateur la fait respecter.

## Exercice
Une `Session` garde un `shared_ptr<Hub>`, et le `Hub` garde des `shared_ptr<Session>`. Quel problème apparaît ? Corrige.

## Corrigé
C'est un cycle : le hub garde les sessions en vie, qui gardent le hub en vie, donc rien n'est jamais détruit. Le hub est le propriétaire naturel des sessions. La session doit garder un `Hub &` si le hub vit toujours plus longtemps, ou un `weak_ptr<Hub>` sinon.

## Voir aussi

- [RAII](raii.md)
- [La durée de vie des objets](duree-de-vie-objets.md)
- [WebSocket](../temps-reel/websocket.md)
- [Mutex et contention de verrou](mutex-contention.md)
