# Matcha — frontend

Matcha est une application de rencontre. Le frontend est une application
React + TypeScript (Vite) posée sur un décor 3D vivant : une clairière en
forêt, avec un grand chêne, la tanière d'un louveteau et une rivière. Le décor
suit l'heure réelle (jour, crépuscule, nuit étoilée) et la date (printemps,
été, automne, hiver).

## Ce que l'on peut faire

| Page | Adresse | Contenu |
| --- | --- | --- |
| Connexion, inscription | `/login`, `/register` | Création de compte, connexion, mot de passe oublié, vérification de l'e-mail. |
| Profil | `/profil` | Progression du profil, informations personnelles, jusqu'à 5 photos, aperçu public, déconnexion de tous les appareils. |
| Découvrir | `/decouvrir` | Grille des profils visibles, filtres (tous, pas encore likés, « ils vous aiment »), fiche détaillée avec like. |
| Matchs | `/matchs` | Matchs, likes reçus en attente, likes envoyés. |
| Messages | `/messages` | Conversations ouvertes par chaque match, messagerie en temps réel (WebSocket). |
| Informations | `/infos/:page` | Conditions générales, confidentialité, mentions légales, contact (contenu à rédiger). |

Dans toute l'application :

- **En-tête** posé sur le ciel : navigation, compteurs (likes en attente,
  messages non lus), centre de notifications, déconnexion.
- **Pied de page** sur l'herbe : état du ciel (saison, moment de la journée,
  phase de lune), liens d'information, bouton pour figer ou animer le décor.
- **Toasts** en bas à gauche pour les likes, matchs et messages reçus pendant la visite.
- **Thème** : papier clair le jour, sous-bois sombre la nuit, accent de couleur de
  la saison. Il change tout seul avec le ciel.

Les adresses `/verify-email`, `/reset-password` et `/resend-verification`
figurent dans les e-mails envoyés par le backend : ne pas les renommer.

## Le décor vivant

Tout est généré par le code : aucun modèle 3D ni aucune texture n'est téléchargé.

- **Ciel** : dégradé calculé selon la vraie hauteur du soleil (formules
  astronomiques, latitude du centre de la France), soleil, lune dont la phase
  découle de la position réelle du soleil, étoiles et voie lactée qui tournent
  avec le temps sidéral, nuages qui dérivent avec le vent.
- **Lointains** : trois crêtes peintes en aplats (rendu 2.5D) avec perspective
  atmosphérique et brume dans les vallées.
- **Saisons** : bourgeons puis feuillage plein, jaunissement et chute des
  feuilles bouquet par bouquet, neige au sol et sur les branches, glace au bord
  de la rivière, fleurs au printemps, lucioles les nuits d'été, pollen dans la
  lumière, oiseaux qui traversent le ciel.
- **Louveteau** : il se promène, renifle, s'assoit, s'allonge ; il rentre dormir
  devant la tanière à la nuit tombée et hurle parfois à la pleine lune. Il
  réagit aux événements de l'application (saute de joie à un match, remue la
  queue à un like, dresse les oreilles à un message, baisse la tête à une
  erreur), détourne poliment le regard pendant la saisie d'un mot de passe, et
  se laisse caresser d'un clic.
- **Accessibilité** : le décor est masqué aux lecteurs d'écran, il respecte
  `prefers-reduced-motion` et peut être figé depuis le pied de page (le choix
  est mémorisé). Sans WebGL, un fond dégradé le remplace.

### Essayer une autre heure ou une autre saison

Ajouter des paramètres à l'URL :

```
/login?foret-date=2027-01-14&foret-heure=22:30
/login?foret-vitesse=600            ← le temps passe 600 fois plus vite
```

## Organisation du code

```
src/
├── app/                  Racine : fournisseurs (contextes), routes, gardes d'accès
├── layout/               Squelette : en-tête, navigation, panneau de page, pied de page
├── features/             Une fonctionnalité par dossier, avec ses styles
│   ├── auth/             Connexion, inscription, mots de passe, vérification
│   ├── profile/          Page Profil et ses sections
│   ├── discover/         Page Découvrir
│   ├── matches/          Page Matchs
│   ├── messages/         Page Messages (assemble le ui-kit de messagerie)
│   ├── notifications/    Cloche et toasts (adaptateurs vers le ui-kit)
│   └── legal/            Pages d'information
├── shared/               Composants partagés par plusieurs fonctionnalités
│   ├── profile/          Portrait, vignette, fiche, bouton de like, formatage
│   └── ui/               Feedback (messages de formulaire)
├── ui-kit/               Composants réutilisables dans d'autres projets
│   ├── notifications/    Centre de notifications, toasts
│   └── messaging/        Liste de conversations, fil, saisie, état de connexion
├── forest-environment/   Le décor 3D (MVC, voir plus bas)
├── context/              État d'authentification et état social (likes, messages…)
├── api/                  Appels HTTP et WebSocket vers le backend
└── styles/               Styles globaux (voir plus bas)
```

### `forest-environment/` : le décor en MVC

Chaque élément du décor a son dossier et suit le même découpage :

- **Model** : l'état, sans aucun objet three.js de rendu (couleurs du moment,
  positions, posture du louveteau…).
- **View** : les maillages et matériaux three.js ; elle lit le modèle et
  l'affiche, sans rien décider.
- **Controller** : la logique ; à chaque image, il lit le monde, met à jour le
  modèle et demande à la vue de se synchroniser.

| Dossier | Rôle |
| --- | --- |
| `world/` | Horloge, astronomie, calendrier des saisons (phénologie), vent. Ne dépend pas de WebGL. |
| `sky/` | Dôme céleste, soleil, lune, étoiles. |
| `lighting/` | Lumière du soleil ou de la lune, lumière du ciel, brouillard, exposition. |
| `clouds/` | Nuages peints, couverture selon la météo du jour. |
| `horizon/` | Crêtes lointaines en 2.5D, neige des sommets, brume des vallées. |
| `terrain/` | Relief, lit de la rivière, couleurs du sol selon la saison. |
| `river/` | Tracé partagé, eau animée, galets, brume au ras de l'eau. |
| `tree/` | Le grand chêne : charpente générée, feuillage saisonnier. |
| `woods/` | La forêt autour de la clairière (sapins et feuillus). |
| `grass/` | Herbe ondulant au vent, fleurs. |
| `den/` | La tanière : blocs de roche, entrée, bois mort. |
| `wolf-cub/` | Le louveteau : comportement (contrôleur) et squelette articulé (vue). |
| `particles/` | Feuilles qui tombent, neige, lucioles, pollen. |
| `birds/` | Vols d'oiseaux. |
| `core/` | Moteur (rendu, caméra, boucle), mise en scène, bruit, outils de géométrie et de matériaux. |
| `react/` | Liaison avec React : `ForestProvider`, `ForestCanvas`, `useForest`. |

`ForestEnvironment.ts` assemble les éléments dans l'ordre de mise à jour.
L'application n'importe que `forest-environment/index.ts` :

```tsx
const { react } = useForest()
react('joy')            // 'joy' | 'love' | 'startle' | 'sad' | 'nod' | 'pet'
```

### `ui-kit/` : composants réutilisables

Les composants de notifications et de messagerie ne dépendent ni de l'API, ni
du routeur, ni des contextes : tout passe par les props, et le thème se règle
par variables CSS `--uik-*`. Copier le dossier suffit pour les réutiliser.
Voir [`src/ui-kit/README.md`](src/ui-kit/README.md).

### Styles

Les styles utilisent les couches CSS (`@layer`), déclarées dans
`src/styles/index.css`, de la plus faible à la plus forte :

| Couche | Où | Contenu |
| --- | --- | --- |
| `reset` | `styles/base/reset.css` | Remise à zéro des navigateurs |
| `settings` | `styles/settings/` | Tokens (typo, espacements, formes) et thèmes jour, nuit et saison |
| `base` | `styles/base/` | Éléments HTML nus : typographie, formulaires |
| `layout` | `layout/*.css` | En-tête, panneau de page, pied de page |
| `ui-kit` | `ui-kit/**/*.css` | Composants réutilisables |
| `components` | `styles/components/`, `shared/**/*.css` | Boutons, pastilles, sections, fiches de profil… |
| `features` | `features/**/*.css` | Styles propres à une page |
| `utilities` | `styles/utilities/` | Classes utilitaires |

Règles : un style global vit dans `src/styles` ; un style propre à un
composant ou une page vit dans un fichier `.css` à côté de son code. Les
classes suivent la convention BEM (`bloc__element--variante`). Les composants
n'utilisent que les couleurs sémantiques (`--color-surface`,
`--color-accent`…), que le décor fait varier en écrivant `data-daylight` et
`data-season` sur `<html>`.

## Développement

Lancer toute la pile depuis la racine du dépôt :

```sh
docker compose up -d --build
```

Vite sert le frontend sur `http://localhost:5173`. Les requêtes partent vers
`http://localhost:18080` par défaut ; pour changer cette adresse, définir
`VITE_AUTH_API_URL` dans un fichier `.env` local non versionné. Pour ne
relancer que le frontend : `docker compose up -d --build frontend`.

Commandes, dans `frontend/` :

```sh
npm run dev      # serveur de développement
npm run build    # vérification TypeScript puis build de production
npm run lint     # ESLint
```
