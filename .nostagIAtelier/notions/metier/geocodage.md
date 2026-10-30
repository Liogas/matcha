# Le géocodage
> Transformer un nom de ville en coordonnées.

## Mise en situation
Un utilisateur refuse le GPS et saisit `Lyon`. Le sujet exige que le matching fonctionne quand même. Il faut donc une latitude et une longitude à partir de ce texte, de préférence sans dépendre d'un service externe le jour de la soutenance.

## La théorie
### Deux approches
- **Base locale** : `TableGeocoder` charge le fichier GeoNames `cities15000`, soit les villes de plus de 15 000 habitants. C'est rapide, fiable et hors ligne, mais limité aux villes connues.
- **Service externe** : `NominatimGeocoder` interroge OpenStreetMap. C'est plus complet, mais il dépend du réseau, avec une politique d'usage d'une requête par seconde, d'où un cache obligatoire.

Les deux respectent `IGeocoder`, une [interface](../cpp/interfaces-injection.md). On peut combiner : table locale d'abord, service en secours.

### La normalisation
Le fichier et l'utilisateur n'écrivent pas pareil. `Saint-Étienne`, `saint etienne` et `SAINT ETIENNE` doivent coïncider :

```text
minuscules -> sans accents -> tirets en espaces -> espaces simplifiés
```

Le retrait des accents se fait sur le texte décodé, voir [UTF-8](../cpp/utf-8.md).

### L'ambiguïté
Plusieurs villes portent le même nom : Paris existe aussi au Texas. On garde par défaut la plus peuplée, ou on demande un pays.

## Exemple déroulé
```cpp
class TableGeocoder : public IGeocoder {
public:
    explicit TableGeocoder(const std::filesystem::path &file);
    core::Result<GeoPoint> locate(std::string_view city) const override;
private:
    std::unordered_map<std::string, GeoPoint> _cities;
};
```

Au chargement, le fichier tabulé est lu une fois. Si un nom existe déjà, on garde la ville la plus peuplée. La recherche est alors immédiate.

```text
"Saint-Étienne" -> "saint etienne" -> trouvé
"Trifouillis"   -> introuvable -> NotFound -> 404 avec message clair
```

Quel message ? « Ville inconnue, essaie une grande ville proche ». La saisie doit se conclure, car elle est requise.

## La licence
GeoNames est sous licence Creative Commons : il faut citer la source dans la documentation.

## Erreurs fréquentes
- Dépendre d'un réseau en soutenance.
- Oublier le cache.
- Normaliser à l'indexation, mais pas à la recherche.

## À retenir
Une dépendance réseau est un risque. Un fichier local suffit pour un besoin simple.

## Exercice
Pourquoi le fichier doit-il être chargé au démarrage plutôt qu'à chaque recherche ?

## Corrigé
Il contient des dizaines de milliers de lignes : le relire à chaque requête serait très lent. Chargé une fois en mémoire, il offre une recherche en temps quasi constant, et ne change pas pendant l'exécution.

## Voir aussi

- [La formule de Haversine](haversine.md)
- [Interfaces et injection de dépendances](../cpp/interfaces-injection.md)
- [UTF-8 : octets et caractères](../cpp/utf-8.md)
