import { Color } from 'three'
import { Random } from '../core/random'

export interface Cloud {
  x: number
  y: number
  z: number
  width: number
  /** Le nuage n'apparaît que si la couverture du ciel dépasse ce seuil. */
  threshold: number
  speed: number
  variant: number
  opacity: number
}

export const CLOUD_SPAN = 320
export const CLOUD_VARIANTS = 4

export class CloudsModel {
  readonly clouds: Cloud[] = []
  readonly lit = new Color('#ffffff')
  readonly shade = new Color('#b4bfcc')
  /** Côté éclairé par le soleil : -1 à gauche, 1 à droite. */
  sunSide = 0

  constructor(count: number) {
    const random = new Random(42)
    for (let index = 0; index < count; index += 1) {
      const z = random.range(-330, -210)
      this.clouds.push({
        x: random.range(-CLOUD_SPAN, CLOUD_SPAN),
        y: random.range(38, 125),
        z,
        width: random.range(70, 150),
        threshold: index / count,
        speed: random.range(0.6, 1.4),
        variant: index % CLOUD_VARIANTS,
        opacity: 0,
      })
    }
  }
}
