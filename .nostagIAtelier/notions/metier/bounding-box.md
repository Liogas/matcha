# Bounding box et antiméridien
> Un rectangle de coordonnées élimine vite les points trop loin, avant le calcul précis.

## Mise en situation
Les profils à moins de 50 km : calculer [Haversine](haversine.md) pour tout le monde serait coûteux, et aucun [index](../sql/index.md) ne peut aider. En revanche, un index sait répondre à `latitude BETWEEN a AND b`. Il faut donc traduire un cercle en plages.

## La théorie
### Du cercle au rectangle
Un degré de latitude fait toujours environ 111 km. Un degré de longitude fait `111 × cos(latitude)` km.

```text
dLat = rayon / 111
dLon = rayon / (111 × cos(latitude))
```

Le rectangle contient le cercle. Il contient aussi ses coins, qui sont trop loin : c'est un pré-filtre, puis [Haversine](haversine.md) affine.

### Le filtre en deux temps
```sql
WHERE latitude BETWEEN $1 AND $2
  AND longitude BETWEEN $3 AND $4
  AND distance_km(latitude, longitude, $5, $6) <= $7
```

La première partie utilise un [index](../sql/index.md), la seconde élimine les faux positifs.

### L'antiméridien
La longitude saute de 180 à -180. Un rayon autour de la longitude 179 donne un maximum de 181, qui n'existe pas. Il faut le ramener à -179 :

```text
minLon = 178, maxLon = -179
```

Alors `BETWEEN 178 AND -179` est toujours faux. On coupe en deux plages :

```sql
(longitude >= 178 OR longitude <= -179)
```

### Les pôles
Près d'un pôle, `cos(lat)` tend vers 0, et `dLon` explose. Si le cercle contient le pôle, on prend toutes les longitudes. Ce n'est pas un cas réel pour matcha, mais un bon test.

## Exemple déroulé
Autour de Paris, rayon 50 km :

```text
dLat = 50 / 111 = 0,45°
dLon = 50 / (111 × 0,657) = 0,685°
Lat : 48,41 à 49,31
Lon : 1,67 à 3,04
```

Sur 500 profils, le rectangle peut en garder 40. [Haversine](haversine.md) ne s'applique alors qu'à ces 40.

## Dans cpp-geo
```cpp
struct BoundingBox { double minLat, maxLat, minLon, maxLon; };
BoundingBox boundingBox(GeoPoint center, double radiusKm);
```

Une fonction pure, sans PostgreSQL. Le [QueryBuilder](../sql/query-builder.md) en fait les conditions.

## Erreurs fréquentes
- Oublier le `cos(latitude)`.
- Croire que le rectangle suffit.
- Ignorer le passage de l'antiméridien.

## À retenir
Le rectangle est rapide et approximatif, [Haversine](haversine.md) est précis et coûteux : on les combine.

## Exercice
Pourquoi le rectangle ne perd-il jamais un point du cercle ?

## Corrigé
Il est construit pour contenir tout le cercle : aucun point à moins de R km ne peut avoir une latitude ou une longitude hors de ces plages. Il peut seulement inclure des points de trop, retirés ensuite. Il y a des faux positifs, mais aucun faux négatif.

## Voir aussi

- [La formule de Haversine](haversine.md)
- [Les index](../sql/index.md)
