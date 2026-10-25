import { Color, Matrix3, Vector3 } from 'three'

/** Couleurs et visibilités du ciel à l'instant présent. */
export class SkyModel {
  readonly zenith = new Color('#4f86c6')
  readonly horizon = new Color('#c9dceb')
  /** Halo chaud autour du soleil quand il est bas. */
  readonly glow = new Color('#ff9a5a')
  glowStrength = 0
  readonly sunColor = new Color('#fff4e0')
  sunVisibility = 1
  moonVisibility = 0
  starVisibility = 0
  readonly sunDirection = new Vector3(0, 1, 0)
  readonly moonDirection = new Vector3(0, -1, 0)
  /** Rotation de la voûte étoilée (repère céleste → scène). */
  readonly starRotation = new Matrix3()
  /** Couleur de brume pour le sol et les lointains : dérivée de l'horizon. */
  readonly haze = new Color('#b8c8d4')
}
