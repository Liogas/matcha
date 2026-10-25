import { Color, Vector3 } from 'three'
import type { RiverPath } from './RiverPath'

/** État de l'eau : couleurs, reflets, glace et brume. */
export class RiverModel {
  readonly deep = new Color('#1d3b3f')
  readonly shallow = new Color('#4f7a6c')
  readonly reflection = new Color('#9fbad0')
  readonly foam = new Color('#dfe8e6')
  readonly glintColor = new Color('#fff4e0')
  readonly glintDirection = new Vector3(0, 1, 0)
  glintStrength = 1
  /** Glace sur les bords (0..1). */
  ice = 0
  /** Décalage du motif de courant, avancé à chaque image. */
  flow = 0
  /** Brume au ras de l'eau (0..1). */
  mist = 0
  readonly mistColor = new Color('#c8d4dc')

  readonly path: RiverPath

  constructor(path: RiverPath) {
    this.path = path
  }
}
