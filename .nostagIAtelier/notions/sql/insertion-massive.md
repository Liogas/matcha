# Insertion massive : COPY et generate_series
> Pour charger beaucoup de lignes, COPY est bien plus rapide qu'une suite d'INSERT.

## Mise en situation
Le sujet exige 500 profils, avec photos, tags, likes et visites, soit des milliers de lignes. Avec un `INSERT` par ligne hors [transaction](transactions.md), chaque ligne déclenche un aller-retour et une écriture forcée sur disque, et la génération prend plusieurs minutes.

## La théorie
### Pourquoi un INSERT est lent
Chaque instruction indépendante coûte un aller-retour réseau, une analyse, et un [`COMMIT`](transactions.md) qui attend le disque, voir les transactions. Trois optimisations :

1. Tout mettre dans une seule transaction : un seul `COMMIT`.
2. Envoyer plusieurs lignes par instruction, avec [unnest](tableaux-unnest.md).
3. Utiliser `COPY`, le protocole prévu pour cela.

### COPY
`COPY table FROM STDIN` envoie un flux de lignes sans analyser chaque instruction :

```c
PQexec(conn, "COPY t (a, b) FROM STDIN");
PQputCopyData(conn, "1\tAlice\n", 8);
PQputCopyEnd(conn, NULL);
```

Le format texte sépare par tabulation, écrit `NULL` en `\N`, et échappe `\`, tabulation et saut de ligne.

### generate_series
```sql
SELECT n, 'user' || n, 'user' || n || '@example.test'
FROM generate_series(1, 500) AS n;
```

C'est utile pour un seed entièrement en SQL.

### La reproductibilité
Une graine fixe donne les mêmes données à chaque exécution :

```cpp
std::mt19937 rng(42);
```

Les profils de démonstration restent identiques, et un bug se reproduit.

### Le mot de passe
[Argon2](../auth/argon2id.md) est lent exprès : 500 hachages prendraient des minutes. On calcule un seul hash, réutilisé pour tous les comptes de seed. C'est acceptable pour des données de démonstration.

## Exemple déroulé : données crédibles
Le générateur doit produire de quoi faire fonctionner chaque fonctionnalité :

- genres et orientations équilibrés, pour que chacun ait des suggestions ;
- positions autour de villes réelles, avec une dispersion aléatoire, pour tester la distance ;
- 3 à 8 tags tirés d'un vocabulaire limité, pour qu'il y ait des tags communs ;
- likes et visites avec une distribution inégale, pour étaler la popularité ;
- emails en `example.test`, un domaine réservé et non routable.

Il faut aussi vérifier le résultat : compter par genre, regarder l'étalement de `fame_rating`.

## Erreurs fréquentes
- Générer des comptes vérifiés sans cohérence : un profil incomplet n'apparaît pas.
- Oublier d'échapper une donnée dans `COPY`.
- Lancer le seed deux fois et créer des doublons. Prévois un nettoyage ou une vérification.

## À retenir
Groupe les écritures : une [transaction](transactions.md), un flux, et une graine fixe.

## Exercice
Pourquoi 500 `INSERT` dans une seule [transaction](transactions.md) sont-ils bien plus rapides que 500 `INSERT` séparés ?

## Corrigé
Le coût principal d'un [`COMMIT`](transactions.md) est l'écriture forcée du journal sur disque. Dans une transaction, ce coût n'est payé qu'une fois. Les allers-retours réseau restent, ce que `COPY` supprime en plus.

## Voir aussi

- [Tableaux et unnest](tableaux-unnest.md)
- [Argon2id : stocker un mot de passe](../auth/argon2id.md)
