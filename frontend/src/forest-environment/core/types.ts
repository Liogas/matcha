import type { PerspectiveCamera, Vector2 } from 'three'

/**
 * Ce que chaque contrôleur reçoit à chaque image.
 * `delta` est déjà multiplié par `motion` : un décor figé reçoit 0.
 */
export interface FrameContext {
  /** Secondes d'animation depuis l'image précédente. */
  delta: number
  /** Temps d'animation cumulé, en secondes. */
  elapsed: number
  /** 1 = décor animé, 0 = décor figé (mouvement réduit). */
  motion: number
  /** Pointeur en coordonnées normalisées (-1..1), y vers le haut. */
  pointer: Vector2
  camera: PerspectiveCamera
}

/** Un contrôleur : le seul point d'entrée du moteur vers un élément du décor. */
export interface ForestController {
  update(frame: FrameContext): void
  dispose(): void
}

/** Niveau de détail choisi au démarrage selon l'appareil. */
export type ForestQuality = 'low' | 'high'
