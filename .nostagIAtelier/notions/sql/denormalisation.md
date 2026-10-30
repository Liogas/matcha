# Dénormalisation et triggers
> Stocker une valeur calculée pour lire vite, au prix de devoir la tenir à jour.

## Mise en situation
Pour trier par popularité, il faudrait calculer pour chaque profil ses likes, matchs et visites, à chaque recherche. Avec 500 profils et un tri, ce calcul se ferait pour tous. On décide donc de stocker `fame_rating` dans `matcha_profiles`.

## La théorie
### Normalisation et dénormalisation
Un schéma normalisé stocke chaque fait une seule fois : on ne peut pas se contredire. La dénormalisation copie ou résume une information pour accélérer la lecture. On échange une lecture simple contre une écriture plus compliquée.

La règle est de partir du normalisé, et de dénormaliser quand une mesure le justifie.

### Tenir la valeur à jour
Trois stratégies :

- **Recalcul applicatif** : le service recalcule après chaque événement. C'est visible et testable, mais chaque chemin doit y penser.
- **Trigger** : la base recalcule toute seule.

```sql
CREATE FUNCTION on_like_change() RETURNS trigger AS $$
BEGIN
    UPDATE matcha_profiles SET like_count = like_count + 1
    WHERE account_id = NEW.liked_account_id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER profile_likes_count
AFTER INSERT ON profile_likes
FOR EACH ROW EXECUTE FUNCTION on_like_change();
```

Aucun chemin ne peut l'oublier, mais la logique est cachée, et plus difficile à tester.

- **Recalcul périodique** : un travail en arrière-plan recalcule tout. C'est simple, mais la note est en retard.

### La colonne générée
Si la valeur ne dépend que de la même ligne :

```sql
display_name TEXT GENERATED ALWAYS AS (first_name || ' ' || last_name) STORED
```

La base la calcule, et aucune incohérence n'est possible.

## Exemple déroulé : pour matcha
Le calcul de la note est une fonction pure, `FameCalculator`. Le déroulé est donc :

1. Un like arrive dans une [transaction](transactions.md).
2. Le service relit les totaux depuis les tables sources.
3. Il calcule la note.
4. Il écrit `fame_rating`.

Pourquoi relire, plutôt que `fame = fame + 2` ? Parce qu'une incrémentation dérive : un bug ou un `unlike` oublié reste pour toujours. Un recalcul depuis la source se corrige tout seul.

## Les pièges de concurrence
```sql
UPDATE t SET n = n + 1 WHERE id = $1;   -- sûr
```

Lire puis écrire en deux requêtes perd des mises à jour, voir l'[isolation](isolation.md). Deux [transactions](transactions.md) qui recalculent la note d'un même profil peuvent aussi écrire dans le désordre. Le dernier gagne, et un recalcul suivant corrige.

## Erreurs fréquentes
- Dénormaliser avant de mesurer.
- Des triggers en chaîne, impossibles à suivre.
- Une colonne dérivée sans moyen de la recalculer entièrement.

## À retenir
Une copie est un engagement : il faut savoir qui la met à jour, et comment la reconstruire.

## Exercice
Quel avantage a une commande capable de recalculer toutes les notes depuis zéro ?

## Corrigé
Elle corrige toute dérive due à un bug, et elle permet de changer la formule : on modifie `FameCalculator`, on relance, et toutes les notes sont cohérentes. La colonne reste une donnée dérivée, jamais une source de vérité.

## Voir aussi

- [La note de popularité](../metier/fame-rating.md)
- [Les vues SQL](vues-sql.md)
- [Les événements de domaine](../archi/evenements.md)
