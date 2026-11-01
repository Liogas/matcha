# Valider les entrées
> Tout ce qui vient du client est suspect : on vérifie à l'entrée, et on normalise.

## Mise en situation
`ProfileRoutes.cpp` accepte n'importe quelle chaîne non vide comme orientation. Le genre accepte tout `[A-Za-z_]+`. La biographie n'a pas de limite. Avec un `curl`, on envoie 10 Mo de biographie ou l'orientation `licorne`. Le sujet précise que la validation sera largement évaluée.

### Trois lignes de défense, trois rôles
Suis la biographie de 10 Mo envoyée avec `curl`. Chaque couche attrape ce qu'elle peut, et aucune ne remplace les autres.

```tableau
titre: La route des données, avec une attaque
colonnes: curl | Nginx | Route | Service | PostgreSQL
étape: Un `curl` contourne complètement le front : le formulaire et ses vérifications n'existent pas pour lui.
! curl -> Nginx: PATCH bio = 10 Mo
étape: Nginx coupe tout ce qui dépasse sa limite de taille, avant même de réveiller le backend.
Nginx -> curl: 413
étape: Une biographie de 800 caractères passe la taille, mais pas la règle métier.
curl -> Route: PATCH bio = 800 car.
Route -> curl: 400 biography too long
étape: Une valeur valide devient une valeur **typée**. Le service ne reçoit plus du texte libre.
curl -> Route: PATCH bio = 120 car.
+ Route -> Service: Biography
étape: La base garde le dernier mot avec ses contraintes, même si un bug a tout laissé passer.
Service -> PostgreSQL: UPDATE
```

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

## Exemple pas à pas : l'ordre des vérifications compte
Sur la biographie, on vérifie d'abord la taille en octets, ce qui ne coûte rien. On valide ensuite l'UTF-8, puis on compte les caractères. Dans l'autre ordre, un texte de 10 Mo serait parcouru entièrement avant d'être refusé.

```tableau
titre: validateBiography sur trois entrées
colonnes: Entrée | Taille | UTF-8 | Longueur
étape: 10 Mo : refus immédiat, sans lire le contenu.
! Taille: refus
étape: Texte de 200 octets avec une séquence invalide : la taille passe, pas l'encodage.
Taille: ok
! UTF-8: refus
étape: 600 caractères en UTF-8 valide : seule la longueur en caractères peut le voir.
Taille: ok
UTF-8: ok
! Longueur: refus
étape: 120 caractères valides : le texte passe toutes les étapes, du moins cher au plus cher.
Taille: ok
UTF-8: ok
+ Longueur: ok
```

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

<!-- enrichie -->
