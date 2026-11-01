# La formule de Haversine
> Calcule la distance la plus courte entre deux points d'une sphère, à partir de leurs latitudes et longitudes.

## Mise en situation
Il faut afficher « à 3 km » et trier par proximité. La tentation est d'utiliser Pythagore sur les degrés. Mais les degrés de longitude n'ont pas la même taille partout : environ 111 km à l'équateur, 71 km à Paris, et 0 au pôle. Le résultat serait faux, et de plus en plus faux en s'éloignant de l'équateur.

### Pythagore se trompe, et de combien ?
Mesure l'erreur sur un vrai exemple. Paris et Vienne sont à peu près à la même latitude, mais éloignées en longitude.

```tableau
titre: Paris - Vienne, deux calculs
étape: Coordonnées : Paris (48,86 ; 2,35), Vienne (48,21 ; 16,37). Les longitudes diffèrent de 14,02°.
= Δlat = 0,65°    Δlon = 14,02°
étape: Pythagore compte chaque degré pour 111 km, comme à l'équateur.
! = √(72² + 1556²) ≈ 1558 km
étape: À cette latitude, un degré de longitude ne fait que 111 × cos(48,5°), soit 73,5 km.
= Δlon réel ≈ 14,02 × 73,5 ≈ 1030 km
étape: Haversine tient compte de cette réduction, et donne environ 1034 km.
+ = distance réelle ≈ 1034 km
étape: L'erreur dépasse 500 km : c'est une distance de plus de 50 %, pas un détail d'arrondi.
! = écart ≈ 524 km
```

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

## Exemple pas à pas : les pièges de l'implémentation
Trois erreurs reviennent presque toujours. Chacune se détecte avec un test simple.

```cpp
// 1. Oublier les radians : std::sin attend des radians
std::sin(48.8);             // faux
std::sin(radians(48.8));    // juste

// 2. Inverser latitude et longitude : aucune erreur de compilation avec deux double
haversineKm({2.35, 48.86}, ...);        // faux
haversineKm({.lat = 48.86, .lon = 2.35}, ...); // lisible et sûr

// 3. Ignorer les points identiques : h = 0, résultat 0, sans division à risque
```

Un test utile tient en trois cas : la distance d'un point à lui-même vaut 0, Paris-Lyon est proche de 392 km, et le calcul est symétrique (`d(a, b) == d(b, a)`).

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

<!-- enrichie -->
