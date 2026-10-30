# La formule de Haversine
> Calcule la distance la plus courte entre deux points d'une sphère, à partir de leurs latitudes et longitudes.

## Mise en situation
Il faut afficher « à 3 km » et trier par proximité. La tentation est d'utiliser Pythagore sur les degrés. Mais les degrés de longitude n'ont pas la même taille partout : environ 111 km à l'équateur, 71 km à Paris, et 0 au pôle. Le résultat serait faux, et de plus en plus faux en s'éloignant de l'équateur.

## La théorie
### Les coordonnées
La latitude va de -90 à 90, la longitude de -180 à 180. Ce sont des angles depuis le centre de la Terre.

### La distance sur un grand cercle
Le plus court chemin à la surface d'une sphère est un arc de grand cercle. Sa longueur vaut le rayon multiplié par l'angle au centre, en radians. Tout le problème consiste à trouver cet angle.

### La formule
```text
h = sin²(Δlat/2) + cos(lat1) × cos(lat2) × sin²(Δlon/2)
d = 2 × R × asin(√h)
```

avec R = 6371 km. Cette forme, à base de haversine (`sin²(x/2)`), reste précise pour de petites distances, contrairement à la loi des cosinus, où `acos` proche de 1 perd sa précision.

Le `cos(lat)` corrige la taille des degrés de longitude.

## Exemple déroulé
```cpp
double haversineKm(GeoPoint a, GeoPoint b) {
    constexpr double kEarthRadiusKm = 6371.0;
    const double dLat = radians(b.lat - a.lat);
    const double dLon = radians(b.lon - a.lon);
    const double sLat = std::sin(dLat / 2);
    const double sLon = std::sin(dLon / 2);
    const double h = sLat * sLat
        + std::cos(radians(a.lat)) * std::cos(radians(b.lat)) * sLon * sLon;
    return 2 * kEarthRadiusKm * std::asin(std::sqrt(h));
}
```

Paris (48,8566 ; 2,3522) et Lyon (45,7640 ; 4,8357) :

```text
Δlat = 3,0926°  soit 0,05398 rad
Δlon = 2,4835°  soit 0,04334 rad
h ≈ 0,000728 + 0,4588 × 0,000469 ≈ 0,000943
d ≈ 2 × 6371 × 0,0307 ≈ 391 km
```

C'est le bon ordre de grandeur, ce qu'un test vérifie.

### La précision
La Terre est légèrement aplatie, donc l'erreur peut atteindre 0,5 %. C'est largement suffisant ici.

### Les tests
Une fonction pure se teste facilement : même point (0 km), Paris-Lyon (environ 392), pôle Nord-pôle Sud (environ 20 015), et de part et d'autre de l'[antiméridien](bounding-box.md) (179 et -179 : environ 222 km à l'équateur, pas 39 000).

## En SQL
Calculer cette formule sur toute la table est coûteux. On réduit d'abord le nombre de candidats avec une [bounding box](bounding-box.md), à l'aide d'un [index](../sql/index.md). PostGIS existe, mais une lib maison suffit, et elle se présente bien.

## Erreurs fréquentes
- Oublier de convertir en radians.
- Inverser latitude et longitude.
- Utiliser `float` : la précision est insuffisante.

## À retenir
Une distance terrestre est un angle multiplié par un rayon. Haversine calcule cet angle de façon stable.

## Exercice
Pourquoi le résultat pour (0, 179) et (0, -179) reste-t-il correct, sans cas particulier ?

## Corrigé
Δlon vaut 358°, soit presque un tour complet. `sin(179°)` donne une petite valeur, comme `sin(1°)`, car le sinus est périodique. La formule retrouve donc les 2° réels, soit environ 222 km.

## Voir aussi

- [Bounding box et antiméridien](bounding-box.md)
- [RGPD : consentement et minimisation](rgpd.md)
