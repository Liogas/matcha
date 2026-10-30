# Le deadlock
> Deux [transactions](transactions.md) s'attendent mutuellement, et aucune ne peut avancer seule.

## Mise en situation
Alice bloque Bob pendant que Bob, au même instant, bloque Alice. Le blocage touche deux lignes, et les deux chemins de code ne les prennent pas dans le même ordre :

```text
T1 : UPDATE ligne Alice   (verrou obtenu)
T2 : UPDATE ligne Bob     (verrou obtenu)
T1 : UPDATE ligne Bob     (attend T2)
T2 : UPDATE ligne Alice   (attend T1)
```

Personne ne peut avancer. Sans intervention, cela durerait toujours.

## La théorie
### Quatre conditions
Un deadlock exige quatre conditions réunies :

1. **Exclusion mutuelle** : une ressource a un seul détenteur.
2. **Détention et attente** : on garde ce qu'on a en attendant autre chose.
3. **Pas de préemption** : personne ne peut retirer un verrou à son détenteur.
4. **Attente circulaire** : A attend B, B attend A.

Il suffit d'en supprimer une. En pratique, on supprime la quatrième.

### La détection par PostgreSQL
Quand une [transaction](transactions.md) attend depuis `deadlock_timeout`, 1 seconde par défaut, PostgreSQL construit le graphe des attentes. S'il y trouve un cycle, il annule une transaction avec le code `40P01`. L'autre continue normalement.

```text
ERROR: deadlock detected
DETAIL: Process 123 waits for ShareLock on transaction 456; blocked by process 124.
```

### La prévention : un ordre fixe
Si tout le monde prend les verrous dans le même ordre, le cycle est impossible : le second attend le premier sans rien détenir de ce que l'autre veut.

```text
T1 : verrou Alice, puis Bob
T2 : verrou Alice ... attend
T1 : verrou Bob, travail, COMMIT
T2 : verrou Alice obtenu, verrou Bob obtenu
```

D'où `LEAST` et `GREATEST` pour les paires, et la paire canonique `first_account_id < second_account_id`.

Pour plusieurs lignes, on les trie :

```sql
SELECT account_id FROM matcha_profiles
WHERE account_id = ANY($1) ORDER BY account_id FOR UPDATE;
```

### Réagir à 40P01
La [transaction](transactions.md) entière est perdue. On annule, on attend un peu, puis on rejoue tout depuis `BEGIN`. Rejouer seulement la dernière requête n'a pas de sens : les écritures précédentes ont été annulées. Dans cpp-pg, ce code devient `Unavailable`, voir [SQLSTATE](sqlstate.md).

## Exemple complet : le deadlock de pool
Celui-ci n'est pas détecté par PostgreSQL, car il vit dans ton programme. Le [pool](pool-connexions.md) a 4 connexions, et 4 requêtes arrivent :

```text
Chaque requête : tx = pool.begin()          -> les 4 connexions sont prises
Chaque requête : repo(pool).find(...)       -> demande une 5e connexion
Tout le monde attend une connexion que personne ne rendra.
```

Le délai d'attente de 2 secondes transforme ce blocage en erreurs `Unavailable`. La vraie correction est de passer l'Executor de la [transaction](transactions.md) à tout ce qui s'exécute à l'intérieur.

## Le même problème en C++
Deux [mutex](../cpp/mutex-contention.md) pris dans un ordre différent produisent exactement le même blocage, sans aucune détection. `std::scoped_lock lock(a, b);` prend les deux sans risque.

## Erreurs fréquentes
- **[Transactions](transactions.md) longues** : plus de verrous, plus longtemps.
- **Deux services modifiant les mêmes tables dans un ordre différent** : fixe un ordre.
- **Rejouer sans limite** : borne les tentatives.

## À retenir
Un deadlock n'est pas un bug de PostgreSQL, c'est un ordre de verrouillage incohérent dans ton code. Trie, et garde les [transactions](transactions.md) courtes.

## Exercice
`transferer(a, b)` verrouille `a` puis `b`. Deux appels simultanés `transferer(1, 2)` et `transferer(2, 1)` peuvent bloquer. Corrige sans changer la signature.

## Corrigé
On verrouille d'abord le plus petit identifiant, puis le plus grand : `std::min(a, b)`, puis `std::max(a, b)`. Les deux appels prennent alors 1 avant 2, et le cycle devient impossible.

## Voir aussi

- [Les verrous consultatifs](verrous-consultatifs.md)
- [Les codes SQLSTATE](sqlstate.md)
- [Mutex et contention de verrou](../cpp/mutex-contention.md)
- [Le pool de connexions](pool-connexions.md)
