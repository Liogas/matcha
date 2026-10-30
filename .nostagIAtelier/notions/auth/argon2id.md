# Argon2id : stocker un mot de passe
> On ne stocke jamais un mot de passe, mais une empreinte volontairement lente et gourmande en mémoire.

## Mise en situation
La base de matcha fuite. Si elle contient les mots de passe en clair, tous les comptes tombent, et ceux des autres sites aussi, car beaucoup de gens réutilisent leur mot de passe. Le sujet donne 0 dans ce cas.

Avec un simple SHA-256, ce n'est guère mieux : une carte graphique en teste des milliards par seconde. Un attaquant passe un dictionnaire entier sur tes hashs, et une grande partie tombe en quelques heures.

## La théorie
### Hachage ou chiffrement
Un chiffrement se déchiffre avec une clé : si la clé fuite, tout fuite. Un hachage est à sens unique : on ne remonte pas au mot de passe, on ne peut que tester des candidats. Pour vérifier une connexion, on hache l'essai et on compare.

### Pourquoi la lenteur est une qualité
SHA-256 est conçu pour être rapide. L'utilisateur légitime fait un seul essai, l'attaquant en fait des milliards. Une fonction qui prend 100 ms est invisible pour le premier et ruine le second : on passe de milliards d'essais par seconde à quelques dizaines.

### Le coût mémoire
Les GPU ont de très nombreux cœurs, mais peu de mémoire par cœur. Argon2 remplit un grand tableau en mémoire et y fait des accès dépendants des données. Sans cette mémoire, le calcul devient bien plus lent. L'avantage des GPU et des circuits spécialisés fond.

Argon2id combine Argon2i, résistant aux attaques par observation des accès mémoire, et Argon2d, résistant aux GPU. C'est la variante recommandée. Argon2 a gagné la Password Hashing Competition en 2015.

### Le sel
Un sel est une valeur aléatoire, unique par compte, mélangée au mot de passe :

```text
hash = Argon2id(mot_de_passe, sel)
```

Il a deux effets. Deux comptes avec le même mot de passe ont des hashs différents. Et une table précalculée devient inutile, car il faudrait une table par sel. Le sel n'est pas [secret](../infra/secrets-fichiers.md) : il est stocké à côté du hash. Son rôle est de forcer l'attaquant à recommencer pour chaque compte.

### Le poivre
Le poivre est un [secret](../infra/secrets-fichiers.md) global, gardé hors de la base. Une fuite de la base seule ne suffit alors plus. Il est optionnel et complique la [rotation](refresh-rotation.md).

## Exemple déroulé
`PasswordHasher` s'appuie sur libsodium :

```cpp
char hash[crypto_pwhash_STRBYTES];
crypto_pwhash_str(hash, password.c_str(), password.size(),
    crypto_pwhash_OPSLIMIT_INTERACTIVE,
    crypto_pwhash_MEMLIMIT_INTERACTIVE);
```

Le résultat ressemble à :

```text
$argon2id$v=19$m=65536,t=2,p=1$<sel en base64>$<hash en base64>
```

Il contient l'algorithme, sa version, la mémoire (64 Mo ici), le nombre de passes et le sel. Tout est dans une seule chaîne. Pour vérifier :

```cpp
crypto_pwhash_str_verify(stocke, essai.c_str(), essai.size()) == 0;
```

La fonction relit les paramètres et le sel, recalcule, et compare en [temps constant](enumeration-temps-constant.md). Cela permet aussi de renforcer les paramètres plus tard : les anciens hashs restent lisibles.

## Erreurs fréquentes
- Comparer des hashs avec `==`.
- Ne pas borner la longueur du mot de passe.
- Écrire son propre schéma.
- Bloquer les threads : chaque connexion consomme 64 Mo et du CPU, d'où le [limiteur](rate-limiting.md).
- Vérifier le hash seulement si le compte existe : voir l'[énumération](enumeration-temps-constant.md).

## À retenir
La sécurité vient du coût imposé à l'attaquant, et du sel qui l'oblige à attaquer chaque compte séparément.

## Exercice
Un attaquant récupère la base. Deux comptes ont le même mot de passe. Le voit-il ? Et sans sel ?

## Corrigé
Avec sel, non : les hashs sont différents. Sans sel, les hashs seraient identiques : il repérerait les mots de passe partagés, et casser l'un casserait tous les autres d'un coup.

## Voir aussi

- [Énumération d'utilisateurs et temps constant](enumeration-temps-constant.md)
- [Politique de mot de passe (NIST 800-63B)](politique-mot-de-passe.md)
- [Limitation de débit et Redis](rate-limiting.md)
