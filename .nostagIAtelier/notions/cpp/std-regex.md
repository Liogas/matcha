# std::regex
> Pratique pour de petites vérifications ASCII, mais coûteux à construire, lent, et inadapté à [UTF-8](utf-8.md).

## Mise en situation
`ProfileRoutes.cpp` valide le genre ainsi :

```cpp
if (!gender.empty() && !std::regex_match(gender, std::regex(R"(^[A-Za-z_]+$)")))
```

Et la ville utilise une plage de lettres accentuées. Il y a trois défauts : la regex est reconstruite à chaque requête, la plage ne fait pas ce qu'elle semble faire, et une regex peut être exploitée pour ralentir le serveur.

## La théorie
### Comment fonctionne une regex
Une regex est compilée en automate. `std::regex` utilise généralement un algorithme à retour arrière : il essaie un chemin et revient en arrière en cas d'échec.

La construction est lente, de l'ordre de la microseconde à la milliseconde, car elle compile l'expression. La bonne pratique est donc de la construire une seule fois :

```cpp
static const std::regex kGender(R"(^[A-Za-z_]+$)");
```

Une variable locale `static` est initialisée une seule fois, de manière thread-safe depuis C++11. L'utilisation en lecture depuis plusieurs threads est sûre.

Le préfixe `R"( )"` est une chaîne brute, qui évite de doubler les antislashs.

### regex_match et regex_search
- `regex_match` exige que toute la chaîne corresponde.
- `regex_search` cherche n'importe où.

Une validation avec `regex_search` sans `^` et `$` laisse passer `abc<script>`.

### Le problème d'UTF-8
`std::regex` sur `std::string` voit des octets. Prenons `[À-Ö]` :

- `À` s'écrit `C3 80` ;
- `Ö` s'écrit `C3 96`.

Le moteur lit la classe comme : l'octet `C3`, la plage `80-C3`, puis `96`. Elle accepte donc des octets au hasard, et pas du tout les lettres voulues. Le comportement dépend en plus de la locale. `.` ne correspond pas non plus à un caractère entier.

Il faut décoder d'abord, voir [UTF-8](utf-8.md), et tester des catégories, ou limiter la regex à l'ASCII.

### Le ReDoS
Avec le retour arrière, certains motifs explosent :

```text
Motif :  ^(a+)+$
Entrée : aaaaaaaaaaaaaaaaaaaaaaaaaaaaa!
```

Le moteur essaie toutes les façons de découper les `a`, soit un nombre exponentiel. Une seule requête peut bloquer un thread pendant des minutes : c'est le déni de service par regex, ou ReDoS. Avec `std::regex`, une longue entrée peut aussi faire déborder la pile, car l'implémentation est récursive.

## Exemple déroulé : valider une ville sans regex
```cpp
core::Result<std::string> CityValidator::validate(std::string_view input) {
    if (input.size() > 200 || !utf8::isValid(input))
        return core::fail(core::ErrorCode::Invalid, "invalid city");
    if (utf8::length(input) < 1 || utf8::length(input) > 80)
        return core::fail(core::ErrorCode::Invalid, "invalid city length");
    for (unsigned char c : input)
        if (c < 0x20)
            return core::fail(core::ErrorCode::Invalid, "control character");
    return std::string(input);
}
```

La longueur est vérifiée en premier, ce qui borne le travail. La règle est explicite et testable.

## Quand utiliser une regex
Pour un motif ASCII simple, sur une entrée courte déjà bornée : un nom d'utilisateur `^[a-zA-Z0-9_]{3,20}$` par exemple. Pour le reste, une boucle simple est plus claire et plus rapide.

## Erreurs fréquentes
- Construire la regex dans la fonction.
- Vouloir valider un email parfaitement : on vérifie une syntaxe raisonnable, et le lien envoyé fait la vraie vérification.
- Oublier que `$` peut accepter un saut de ligne final selon les moteurs.
- Valider avant de borner la longueur.

## À retenir
Borne d'abord, décode ensuite. Une regex `static const`, ASCII, sur une entrée courte, c'est très bien. Au-delà, une boucle explicite est préférable.

## Exercice
Pourquoi `^[A-Za-z]+$` refuse-t-il `Élodie`, et que proposes-tu pour un prénom ?

## Corrigé
`É` est hors de la plage ASCII, et ses deux octets ne correspondent pas. Pour un prénom, on valide l'[UTF-8](utf-8.md), on borne la longueur en points de code, on refuse les caractères de contrôle et on autorise lettres, espace, apostrophe et tiret. Un test d'appartenance à une classe Unicode demande un décodage.

## Voir aussi

- [UTF-8 : octets et caractères](utf-8.md)
- [Valider les entrées](../web/validation-entrees.md)
