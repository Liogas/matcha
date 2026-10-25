/**
 * Mise en scène du décor : où se trouvent la caméra, l'arbre, la tanière, la rivière.
 * Repère : y vers le haut, la caméra regarde vers -z, c'est-à-dire le sud.
 * Le soleil se lève donc à gauche (est) et se couche à droite (ouest).
 */

export const CAMERA = {
  fov: 46,
  position: [0.6, 2.5, 14.5] as [number, number, number],
  target: [-1.6, 3.7, -6] as [number, number, number],
  /** Amplitude de la parallaxe au pointeur, en mètres (x, y). */
  parallax: [0.7, 0.3] as [number, number],
}

/** Latitude et longitude utilisées pour le soleil et la lune (centre de la France). */
export const OBSERVER = { latitude: 46.6, longitude: 2.4 }

export const CLEARING = { x: -2, z: -2, radius: 15 }

export const TREE = { x: -7, z: -5 }

export const DEN = {
  x: -4.4,
  z: -3.2,
  /** Orientation de l'entrée (radians autour de y, 0 = vers la caméra). */
  facing: 0.55,
}

/** Zone où le louveteau se promène. */
export const PLAY_AREA = { x: -2.4, z: 1.4, radius: 3.2 }

/** Tracé de la rivière, de l'amont (au fond) vers l'aval (devant la caméra). */
export const RIVER_PATH: [number, number][] = [
  [38, -150],
  [26, -96],
  [17, -58],
  [10, -30],
  [5, -15],
  [2.6, -5],
  [3.8, 3.5],
  [1.8, 11],
  [2.4, 24],
]

export const RIVER_WIDTH = { upstream: 2.2, downstream: 3.4 }

/** Niveau de l'eau : la rivière descend des collines jusqu'à la clairière. */
export const RIVER_LEVEL = { upstream: 4.2, downstream: -0.35 }
