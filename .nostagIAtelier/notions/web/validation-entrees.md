# Valider les entrées
> Tout ce qui vient du client est suspect : on vérifie à l'entrée, et on normalise.

## Mise en situation
`ProfileRoutes.cpp` accepte n'importe quelle chaîne non vide comme orientation. Le genre accepte tout `[A-Za-z_]+`. La biographie n'a pas de limite. Avec un `curl`, on envoie 10 Mo de biographie ou l'orientation `licorne`. Le sujet précise que la validation sera largement évaluée.

## La théorie
### Où valider
À la frontière du système, là où les données entrent. Une fois validées, les couches internes peuvent leur faire confiance. Le front valide pour le confort, le serveur pour la sécurité, la base pour la vérité.

### Liste blanche ou liste noire
Une liste noire énumère ce qui est interdit, et elle oublie toujours quelque chose. Une liste blanche décrit ce qui est accepté. Tout le reste est refusé.

### Les vérifications
- **Type** : le champ est-il une chaîne ?
- **Longueur** : en [points de code](../cpp/utf-8.md).
- **Format** : motif ou [énumération](../auth/enumeration-temps-constant.md).
- **Plage** : âge, position, limite.
- **Cohérence** : `age_min <= age_max`.

### Parser, pas seulement valider
Une bonne validation produit une valeur typée :

```cpp
core::Result<Orientation> parseOrientation(std::string_view text);
```

Le reste du code manipule une `enum class`, où une valeur invalide n'existe pas. La validation ne peut plus être oubliée plus loin.

### Normaliser
Mettre l'email en minuscules, retirer les espaces, retirer `#` d'un tag. On stocke la forme normalisée, et on valide après normalisation.

## Exemple déroulé : une biographie
```cpp
core::Result<std::string> validateBiography(std::string_view input) {
    if (input.size() > 4 * 500 || !utf8::isValid(input))
        return core::fail(core::ErrorCode::Invalid, "invalid biography");
    if (utf8::length(input) > 500)
        return core::fail(core::ErrorCode::Invalid, "biography too long");
    return std::string(input);
}
```

Un [point de code](../cpp/utf-8.md) prend au plus 4 octets, donc plus de 2000 octets dépassent forcément la limite : le premier test borne le travail. [Nginx](../infra/nginx.md) a déjà limité le corps à 10 Mo.

### Pour l'âge
Le sujet n'en parle pas, mais un site de rencontre exige une majorité. La date de naissance ne doit pas être dans le futur, et l'âge minimal doit être fixé. C'est une décision ouverte du projet.

## Erreurs fréquentes
- Valider, puis utiliser une autre valeur.
- Afficher l'entrée brute dans un message d'erreur.
- Confondre validation et [échappement](xss-echappement.md) : la XSS reste à traiter à l'affichage.
- Oublier un champ optionnel.

## À retenir
Valide à la frontière, transforme en types forts, et garde une dernière barrière dans la base.

## Exercice
Pourquoi le test `input.size() > 2000` arrive-t-il avant `utf8::length` ?

## Corrigé
C'est un test en [temps constant](../auth/enumeration-temps-constant.md), qui rejette tout de suite les entrées énormes, sans parcourir des mégaoctets. La borne est sûre, car 500 [points de code](../cpp/utf-8.md) tiennent au plus sur 2000 octets.

## Voir aussi

- [UTF-8 : octets et caractères](../cpp/utf-8.md)
- [std::regex](../cpp/std-regex.md)
- [Les contraintes d'intégrité](../sql/contraintes.md)
- [XSS et échappement](xss-echappement.md)
