# Politique de mot de passe (NIST 800-63B)
> La longueur et une liste de mots interdits protègent mieux que des règles de composition.

## Mise en situation
Le sujet refuse les mots anglais courants. Le code n'en refuse que 7, en dur dans `AuthRouteSupport.cpp`. Un formulaire classique impose une majuscule, un chiffre et un symbole, et `Password1!` passe, alors qu'il figure dans toutes les listes d'attaque.

## La théorie
### Comment les mots de passe sont attaqués
Une attaque ne teste pas au hasard. Elle essaie d'abord les mots de passe connus issus des fuites, puis des mots du dictionnaire avec des transformations courantes : majuscule initiale, chiffres à la fin, `a` remplacé par `@`.

### Ce que recommande le NIST
Le document SP 800-63B demande :

- au moins 8 caractères imposés, et au moins 64 acceptés ;
- une vérification contre une liste de mots compromis, courants ou liés au service ;
- aucune règle de composition imposée ;
- aucune expiration périodique sans raison.

Les règles de composition poussent vers des motifs prévisibles, et l'expiration vers `Printemps2026!`, puis `Ete2026!`.

### L'entropie
L'entropie mesure le nombre d'essais nécessaires, en bits. Une phrase de 4 mots choisis au hasard parmi 7776 donne environ 51 bits. `Tr0ub4dor&3` semble fort, mais ses transformations sont prévisibles. La longueur l'emporte. `zxcvbn` estime cette difficulté réelle, en repérant mots, dates et suites clavier.

### Le k-anonymat
L'API de HaveIBeenPwned permet de vérifier un mot de passe sans le révéler : on calcule son SHA-1, on envoie les 5 premiers caractères, et le service renvoie tous les suffixes correspondants. La comparaison se fait localement.

## Exemple déroulé
```cpp
class IPasswordPolicy {
public:
    virtual ~IPasswordPolicy() = default;
    virtual core::Result<void> check(std::string_view password) const = 0;
};
```

`DictionaryPasswordPolicy` charge `backend/data/` une fois au démarrage dans un `std::unordered_set`, soit une recherche en temps quasi constant. Pour `P4ssw0rd123` :

1. minuscules : `p4ssw0rd123` ;
2. retrait des chiffres de fin : `p4ssw0rd` ;
3. inversion du leet speak (`4` en `a`, `0` en `o`) : `password` ;
4. présent dans la liste : refusé avec `Invalid`.

La règle est injectée, ce qui remplace les doublons `isValidPwd` et `validPassword` (audit 6.1 #4).

## Où l'appliquer
À l'inscription, au reset, et au changement de mot de passe. Un seul oubli, et la règle tombe.

## Erreurs fréquentes
- Tester avant la mise en minuscules.
- Une liste absente de l'image Docker : le fichier doit être copié à l'étape `runtime`.
- Un message qui ne dit pas quoi faire. Dis : « Ce mot de passe est trop courant ».
- Refuser de longues phrases à cause d'une limite trop basse.

## À retenir
On ne demande pas un mot de passe compliqué, mais un mot de passe absent des listes des attaquants, et assez long.

## Exercice
Pourquoi le refus d'un mot de passe doit-il être évalué côté serveur, et non seulement dans React ?

## Corrigé
Un client peut appeler l'API directement avec `curl`, sans passer par le formulaire. La validation du front améliore l'expérience, mais seule celle du serveur est une garantie.

## Voir aussi

- [Argon2id : stocker un mot de passe](argon2id.md)
- [Valider les entrées](../web/validation-entrees.md)
- [Interfaces et injection de dépendances](../cpp/interfaces-injection.md)
