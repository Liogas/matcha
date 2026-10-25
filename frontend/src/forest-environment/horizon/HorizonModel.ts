import { Color } from 'three'

export interface RidgeLayer {
  z: number
  baseHeight: number
  amplitude: number
  frequency: number
  /** Dentelure de cimes de sapins sur la ligne de crête (en mètres, 0 = roche nue). */
  treeTips: number
  /** Part de brume atmosphérique : plus la crête est loin, plus elle se fond dans le ciel. */
  haze: number
  /** Couleur de base de la crête en plein jour d'été. */
  baseColor: Color
  /** Couleur calculée pour l'instant présent. */
  color: Color
}

/**
 * Les lointains : des crêtes peintes en aplats, de plus en plus pâles avec la distance,
 * à la manière d'un décor de théâtre ou d'une estampe.
 */
export class HorizonModel {
  readonly layers: RidgeLayer[] = [
    ridge(-150, 9, 9, 0.013, 2.6, 0.22, '#2c4330'),
    ridge(-235, 20, 18, 0.0085, 1.6, 0.48, '#3a5245'),
    ridge(-345, 42, 46, 0.0048, 0, 0.66, '#55667a'),
  ]

  readonly haze = new Color('#b8c8d4')
  /** Neige sur les sommets (0..1). */
  snow = 0
  /** Altitude au-dessus de laquelle la neige tient sur les montagnes. */
  snowLine = 70
  /** Brume dans les vallées, entre les crêtes (0..1). */
  mist = 0.4
}

function ridge(z: number, baseHeight: number, amplitude: number, frequency: number, treeTips: number, haze: number, color: string): RidgeLayer {
  return { z, baseHeight, amplitude, frequency, treeTips, haze, baseColor: new Color(color), color: new Color(color) }
}
