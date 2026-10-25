import { Color, Vector3 } from 'three'
import { Random } from '../core/random'
import type { ForestQuality } from '../core/types'

export interface FallingLeaf {
  readonly position: Vector3
  readonly velocity: Vector3
  readonly rotation: Vector3
  readonly spin: Vector3
  readonly color: Color
  phase: number
  /** Temps passé au sol : la feuille s'efface ensuite. */
  grounded: number
  active: boolean
}

/** Volume où tombe la neige et flottent les poussières : la partie visible de la clairière. */
export const PARTICLE_BOX = { minX: -26, maxX: 16, minY: 0, maxY: 16, minZ: -22, maxZ: 13 }

/** Points flottants (neige, lucioles, pollen) : positions et graines dans des tableaux typés. */
export class PointCloud {
  readonly positions: Float32Array
  readonly alphas: Float32Array
  readonly seeds: Float32Array
  /** Intensité globale (0..1), appliquée par la vue. */
  intensity = 0

  readonly count: number

  constructor(count: number, random: Random, spawn: (index: number, random: Random, out: Float32Array) => void) {
    this.count = count
    this.positions = new Float32Array(count * 3)
    this.alphas = new Float32Array(count).fill(1)
    this.seeds = new Float32Array(count)
    for (let index = 0; index < count; index += 1) {
      spawn(index, random, this.positions)
      this.seeds[index] = random.next()
    }
  }
}

function inBox(index: number, random: Random, out: Float32Array) {
  out[index * 3] = random.range(PARTICLE_BOX.minX, PARTICLE_BOX.maxX)
  out[index * 3 + 1] = random.range(PARTICLE_BOX.minY, PARTICLE_BOX.maxY)
  out[index * 3 + 2] = random.range(PARTICLE_BOX.minZ, PARTICLE_BOX.maxZ)
}

export class ParticlesModel {
  readonly leaves: FallingLeaf[] = []
  readonly snow: PointCloud
  readonly fireflies: PointCloud
  readonly motes: PointCloud
  /** Feuilles à faire naître (accumulateur fractionnaire). */
  leafBudget = 0

  constructor(quality: ForestQuality) {
    const random = new Random(99)
    const maxLeaves = quality === 'high' ? 110 : 50
    for (let index = 0; index < maxLeaves; index += 1) {
      this.leaves.push({
        position: new Vector3(),
        velocity: new Vector3(),
        rotation: new Vector3(),
        spin: new Vector3(),
        color: new Color(),
        phase: 0,
        grounded: 0,
        active: false,
      })
    }
    this.snow = new PointCloud(quality === 'high' ? 1600 : 650, random, inBox)
    this.fireflies = new PointCloud(quality === 'high' ? 55 : 30, random, inBox)
    this.motes = new PointCloud(quality === 'high' ? 160 : 70, random, inBox)
  }
}
