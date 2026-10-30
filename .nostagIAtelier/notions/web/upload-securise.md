# Upload d'images sécurisé
> Un fichier envoyé est une entrée hostile : on borne, on décode, et on réencode.

## Mise en situation
Un utilisateur envoie `photo.jpg`, qui est en réalité du HTML avec du JavaScript. Un autre envoie une image de 5 Ko qui se décompresse en 30 000 × 30 000 pixels : c'est une bombe de décompression. Un troisième envoie une vraie photo de téléphone, contenant la position GPS exacte de son domicile.

## La théorie
### Ne pas croire le client
L'extension, le nom du fichier et le `Content-Type` sont des déclarations. Seul le contenu réel compte. Les premiers octets, dits magic bytes, sont un indice, mais un fichier peut être valide en plusieurs formats à la fois, c'est un polyglotte.

### La stratégie de matcha
1. **Borner** : [Nginx](../infra/nginx.md) limite le corps à 10 Mo, et `ImageProcessor` vérifie `_maximumInputSize`.
2. **Décoder** : libvips ouvre l'image, et un faux fichier échoue.
3. **Borner les dimensions** : au-delà de `_maximumDimension`, on réduit.
4. **Réencoder** en WebP : on écrit de nouveaux pixels. Tout ce qui était caché dans le fichier disparaît.
5. **Nommer côté serveur** : la clé de stockage est générée, et n'utilise jamais le nom reçu.

Le réencodage est la mesure la plus forte : on ne vérifie pas que le fichier est sain, on en fabrique un nouveau.

### Le traversement de chemin
Un nom comme `../../etc/passwd` pourrait écrire n'importe où. Une clé générée par le serveur rend ce risque impossible.

### Les métadonnées EXIF
Une photo de téléphone peut contenir latitude, longitude, heure et modèle. Sur un site de rencontre, ces données sont dangereuses. Il faut vérifier que la sortie WebP ne les contient plus, par exemple avec `exiftool`. libvips dispose d'une option de suppression des métadonnées. Attention : l'orientation EXIF sert à afficher la photo droite, il faut l'appliquer avant de la retirer.

### Servir les fichiers
- Avec le type exact : `image/webp`.
- Avec `X-Content-Type-Options: nosniff`, pour que le navigateur ne devine pas un autre type.

## Exemple déroulé
```text
Entrée : page.html renommée photo.jpg, 1 Ko
1. Taille correcte
2. libvips : format inconnu, échec
3. Réponse 400
Rien n'est écrit sur le disque.
```

```text
Entrée : vraie photo 4000x3000 avec GPS
1. Taille correcte
2. Décodage réussi
3. Redimensionnement
4. Écriture WebP
5. Métadonnées absentes de la sortie (à vérifier)
```

## Erreurs fréquentes
- Se fier au `Content-Type`.
- Décoder toute l'image avant de vérifier ses dimensions.
- Servir le fichier original.

## À retenir
On ne garde jamais le fichier reçu : on en extrait les pixels, et on écrit le nôtre.

## Exercice
Pourquoi une clé générée par le serveur vaut-elle mieux que le nom de fichier, même nettoyé ?

## Corrigé
Le nettoyage d'un chemin est une liste noire, facile à contourner selon les systèmes et les encodages. Une clé générée exclut toute donnée du client du chemin : le problème disparaît au lieu d'être filtré.

## Voir aussi

- [Valider les entrées](validation-entrees.md)
- [RGPD : consentement et minimisation](../metier/rgpd.md)
