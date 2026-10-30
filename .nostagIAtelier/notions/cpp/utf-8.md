# UTF-8 : octets et caractères
> UTF-8 code un caractère sur 1 à 4 octets : la taille d'une chaîne en octets n'est pas son nombre de caractères.

## Mise en situation
Tu limites la biographie à 500 caractères :

```cpp
if (bio.size() > 500) return erreur();
```

Une utilisatrice qui écrit en russe est refusée à 250 lettres, car chacune prend deux octets. Et si tu coupes une chaîne à 500 octets pour l'afficher, tu peux tomber au milieu d'un caractère : PostgreSQL refuse alors le texte avec `invalid byte sequence for encoding "UTF8"`.

## La théorie
### Trois niveaux
- **Octet** : 8 bits, ce que compte `std::string::size()`.
- **Point de code** : le numéro d'un caractère dans Unicode. `é` est U+00E9, `€` est U+20AC.
- **Graphème** : ce que l'humain voit comme une lettre. Un drapeau ou une famille en emoji combine plusieurs points de code.

### Le codage
UTF-8 écrit chaque point de code ainsi :

```text
U+0000  - U+007F    0xxxxxxx
U+0080  - U+07FF    110xxxxx 10xxxxxx
U+0800  - U+FFFF    1110xxxx 10xxxxxx 10xxxxxx
U+10000 - U+10FFFF  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
```

Le premier octet annonce la longueur, et les suivants commencent toujours par `10`. Ce système a trois qualités : l'ASCII reste inchangé, un octet de suite se reconnaît immédiatement, et on peut retrouver le début d'un caractère depuis n'importe quel point.

### Exemple : coder é
`é` vaut U+00E9, soit 233, en binaire `11101001`. Cela demande 2 octets :

```text
110xxxxx 10xxxxxx
11000011 10101001  =  C3 A9
```

### Ce qu'une validation doit refuser
- un octet de suite isolé ;
- une séquence tronquée ;
- un encodage trop long, comme `C0 AF` pour `/`, classique pour contourner des filtres ;
- U+D800 à U+DFFF, réservés à UTF-16 ;
- tout ce qui dépasse U+10FFFF.

## Exemple déroulé : compter et valider
Compter les points de code revient à compter les octets qui ne sont pas de suite :

```cpp
std::size_t length(std::string_view text) {
    std::size_t count = 0;
    for (unsigned char byte : text)
        if ((byte & 0b11000000) != 0b10000000)
            ++count;
    return count;
}
```

`byte & 0b11000000` garde les deux bits de poids fort. Le type `unsigned char` est important : un `char` signé donnerait des valeurs négatives avec certains calculs.

Pour `été`, soit `C3 A9 74 C3 A9` : 5 octets, dont 2 de suite, donc 3 points de code.

## Dans matcha
- `utf8CharacterCount` existe déjà, et `cpp-validation` le remplace par `utf8::length` et `utf8::isValid`.
- `lowercaseAscii` existe parce que `std::tolower` agit octet par octet et abîmerait les caractères accentués.
- Toute chaîne reçue doit être validée avant d'arriver en base.
- Une limite affichée à l'utilisateur doit être comptée en points de code.

## Erreurs fréquentes
- Tronquer avec `substr(0, n)` : on peut couper un caractère.
- Utiliser [`std::regex`](std-regex.md) sur des accents, voir std::regex.
- Comparer des textes visuellement identiques mais différents : `é` peut être codé U+00E9 ou `e` + U+0301. C'est la [normalisation](../web/validation-entrees.md) Unicode, à prendre en compte pour la recherche.
- Croire que `std::string` connaît UTF-8 : elle contient seulement des octets.

## À retenir
Une `std::string` est un sac d'octets. Le sens UTF-8 vient de toi : valide à l'entrée, compte en points de code, et ne coupe pas au hasard.

## Exercice
Combien d'octets et de points de code contient `€uro` ? Quels sont les premiers octets ?

## Corrigé
`€` prend 3 octets (`E2 82 AC`), et `uro` en prend 3. Total : 6 octets pour 4 points de code. Le premier octet `E2` commence par `1110`, ce qui annonce une séquence de 3 octets.

## Voir aussi

- [std::regex](std-regex.md)
- [Politique de mot de passe (NIST 800-63B)](../auth/politique-mot-de-passe.md)
- [Valider les entrées](../web/validation-entrees.md)
