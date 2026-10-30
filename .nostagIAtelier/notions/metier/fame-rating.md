# La note de popularité
> Le sujet te laisse la définir : elle doit être cohérente, documentée et expliquée.

## Mise en situation
Chaque utilisateur a une note publique. Le sujet laisse le choix des critères, tant qu'ils sont cohérents. Une simple somme de likes favorise quelques profils, et peut être gonflée par de faux comptes.

## La théorie
### Que mesurer
Des signaux d'intérêt réel :

- likes reçus ;
- matchs, plus forts, car réciproques ;
- visites uniques récentes ;
- signalements validés, en négatif ;
- profil complet, en bonus léger.

### Pourquoi un logarithme
```text
fame = clamp(0, 100,
    10 × log2(1 + likes)
  + 15 × log2(1 + matchs)
  +  3 × log2(1 + visites_uniques_30j)
  - 20 × signalements_validés
  +  5 si profil complet)
```

`log2(1 + n)` croît vite au début, puis lentement. Le rendement est décroissant :

```text
1 like     -> 10
3 likes    -> 20
15 likes   -> 40
255 likes  -> 80
```

Les 3 premiers likes rapportent 20 points, et il en faut 240 de plus, de 15 à 255, pour en gagner 40. Un nouveau profil peut donc progresser, et personne n'écrase les autres. Le `1 +` évite `log(0)`.

`clamp` borne la note entre 0 et 100, ce qui simplifie l'affichage et le score.

## Exemple déroulé
Une fonction pure :

```cpp
struct FameStats { int likes, matches, uniqueVisits, reports; bool complete; };

int computeFame(const FameStats &s) {
    const double raw = 10 * std::log2(1.0 + s.likes)
                     + 15 * std::log2(1.0 + s.matches)
                     + 3 * std::log2(1.0 + s.uniqueVisits)
                     - 20 * s.reports
                     + (s.complete ? 5 : 0);
    return static_cast<int>(std::clamp(raw, 0.0, 100.0));
}
```

Pour 15 likes, 3 matchs, 7 visites, aucun signalement, profil complet : 40 + 30 + 9 + 5 = 84.

Elle n'a pas de dépendance, donc elle se teste en quelques lignes.

### Quand recalculer
À chaque événement : like, unlike, visite, signalement. Voir la [dénormalisation](../sql/denormalisation.md). On relit les totaux depuis les tables sources, ce qui corrige toute dérive.

### Visites sur 30 jours
Les visites anciennes sortent de la fenêtre sans aucun événement. Un recalcul périodique ou un recalcul à la lecture est nécessaire. La carte doit trancher.

## Erreurs fréquentes
- Compter un même visiteur plusieurs fois.
- Oublier le recalcul sur `unlike`.
- Laisser un utilisateur gonfler sa note avec de faux comptes : limiter ce que comptent des comptes non vérifiés.

## À retenir
Une note doit pouvoir s'expliquer en une phrase et se recalculer depuis les données.

## Exercice
Pourquoi `log2` est-il plus juste qu'une simple somme de likes ?

## Corrigé
Avec une somme, un profil à 1000 likes a 100 fois plus qu'un profil à 10, et le tri l'affiche sans cesse, ce qui lui donne encore plus de likes. Le logarithme réduit cet écart, et laisse leur chance aux autres.

## Voir aussi

- [Dénormalisation et triggers](../sql/denormalisation.md)
- [Les événements de domaine](../archi/evenements.md)
- [Le score de matching](score-matching.md)
