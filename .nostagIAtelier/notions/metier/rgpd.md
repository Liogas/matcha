# RGPD : consentement et minimisation
> On collecte le strict nécessaire, avec un consentement explicite, et on permet de le retirer.

## Mise en situation
Le sujet demande une position GPS au quartier, avec consentement. Sur un site de rencontre, une position exacte révèle un domicile. Une fuite serait un vrai risque physique pour les personnes, pas seulement un problème juridique.

## La théorie
### Les principes utiles
Le Règlement général sur la protection des données fixe notamment :

- **Licéité et consentement** : une base légale, ici un consentement libre, spécifique et éclairé.
- **Minimisation** : seulement ce qui est nécessaire.
- **Limitation de la conservation**.
- **Droits des personnes** : accès, rectification, suppression, retrait du consentement.

### Les données sensibles
L'orientation sexuelle fait partie des catégories particulières de données, protégées plus strictement. Dans matcha, elle est au cœur du service. Elle ne doit jamais aller dans les logs.

### Le consentement dans le code
Le navigateur demande via `navigator.geolocation`, mais cette permission technique ne suffit pas à tracer le choix. `location_consent_at` conserve la date. La [contrainte](../sql/contraintes.md) `matcha_profiles_location_consent` relie `location_source` et cette date. Refuser ne doit pas bloquer l'application : la ville saisie à la main prend le relais.

### La minimisation par arrondi
On arrondit à 2 décimales, soit environ 1 km, avant le stockage :

```cpp
GeoPoint roundToPrecision(GeoPoint p, double km);
```

Une donnée non stockée ne peut pas fuir. C'est une protection plus forte qu'un arrondi à l'affichage.

## Exemple déroulé : la triangulation
Même avec une position arrondie, afficher une distance précise est risqué. Un attaquant se place à trois endroits, lit trois distances, et trace trois cercles : leur intersection donne la position. Des applications de rencontre ont réellement eu cette faille.

Les parades :

- position stockée déjà imprécise ;
- distance affichée arrondie (« moins de 5 km ») ;
- limitation des appels.

## Retirer son consentement
Un appel met `location_source` à `manual`, efface les coordonnées et `location_consent_at`. Il faut alors une ville, sinon le matching ne fonctionne plus.

## Erreurs fréquentes
- Stocker la position exacte, avec l'idée de l'arrondir à l'affichage.
- Garder les métadonnées GPS des photos, voir l'[upload](../web/upload-securise.md).
- Journaliser des coordonnées.

## À retenir
La meilleure protection d'une donnée sensible est de ne pas la conserver.

## Exercice
Pourquoi arrondir à l'écriture plutôt qu'à l'affichage ?

## Corrigé
Si la position exacte est en base, une fuite ou un bug d'API la révèle. Arrondie à l'écriture, elle n'existe plus nulle part.

## Voir aussi

- [Le géocodage](geocodage.md)
- [Upload d'images sécurisé](../web/upload-securise.md)
- [La formule de Haversine](haversine.md)
