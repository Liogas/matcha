# RGPD : consentement et minimisation
> On collecte le strict nécessaire, avec un consentement explicite, et on permet de le retirer.

## Mise en situation
Le sujet demande une position GPS au quartier, avec consentement. Sur un site de rencontre, une position exacte révèle un domicile. Une fuite serait un vrai risque physique pour les personnes, pas seulement un problème juridique.

### Le risque, sur une scène réelle
Prends une personne qui utilise matcha et rentre chez elle chaque soir. Si le serveur enregistre sa position GPS brute à 5 mètres près, une fuite de la base révèle son adresse. Si le serveur ne garde que le quartier, la même fuite ne révèle presque rien. La protection la plus solide n'est pas de bien cacher la donnée : c'est de ne pas l'avoir.

```tableau
titre: Une position, de la demande au stockage
colonnes: Navigateur | Backend | Base
étape: Le navigateur demande la permission : c'est une autorisation technique.
Navigateur: permission accordée
étape: Le backend enregistre aussi le **choix de la personne**, avec sa date.
Navigateur -> Backend: position + consentement
Backend -> Base: location_consent_at
étape: L'arrondi se fait avant l'écriture. La position précise n'atteint jamais le disque.
+ Backend -> Base: 48,86 ; 2,35
étape: Si la personne retire son consentement, la position est effacée.
Navigateur -> Backend: retrait
+ Backend -> Base: position supprimée
```

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
