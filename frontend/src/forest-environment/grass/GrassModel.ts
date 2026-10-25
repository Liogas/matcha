import { Color } from 'three'
import { CLEARING, DEN, TREE } from '../core/composition'
import { fbm } from '../core/noise'
import { Random } from '../core/random'
import type { ForestQuality } from '../core/types'
import type { TerrainModel } from '../terrain/TerrainModel'

export interface GrassBlade {
  x: number
  y: number
  z: number
  rotation: number
  width: number
  height: number
  shade: number
}

export interface Flower {
  x: number
  y: number
  z: number
  size: number
  threshold: number
  color: Color
}

const FLOWER_COLORS = ['#f2efe4', '#f2efe4', '#e6c34a', '#b6a2d4', '#e0a3b4']

/** L'herbe de la clairière et ses fleurs, en touffes irrégulières. */
export class GrassModel {
  readonly blades: GrassBlade[] = []
  readonly flowers: Flower[] = []
  readonly color = new Color('#6b9440')
  /** Floraison (0..1). */
  bloom = 0

  constructor(terrain: TerrainModel, quality: ForestQuality) {
    const random = new Random(77)
    const bladeTarget = quality === 'high' ? 11000 : 4000
    const flowerTarget = quality === 'high' ? 420 : 160
    const radius = CLEARING.radius + 7

    const accept = (x: number, z: number) => {
      const { distance, t } = terrain.river.nearest(x, z)
      if (distance < terrain.river.widthAt(t) / 2 + 0.5) return false
      if (Math.hypot(x - TREE.x, z - TREE.z) < 1.4) return false
      // Devant l'entrée de la tanière, la terre est tassée
      const entranceX = DEN.x + Math.sin(DEN.facing) * 1.4
      const entranceZ = DEN.z + Math.cos(DEN.facing) * 1.4
      if (Math.hypot(x - entranceX, z - entranceZ) < 1.6) return false
      return true
    }

    let attempts = 0
    while (this.blades.length < bladeTarget && attempts < bladeTarget * 6) {
      attempts += 1
      // Échantillonnage biaisé vers l'avant-plan, là où les brins se voient
      const x = CLEARING.x + random.range(-radius, radius) * 1.15
      const z = random.range(CLEARING.z - radius, 13)
      if (Math.hypot(x - CLEARING.x, (z - CLEARING.z) * 0.85) > radius) continue
      const clump = fbm(x * 0.22, z * 0.22, 3, 12)
      if (clump < 0.36 || !accept(x, z)) continue
      const near = z > 6 ? 0.85 : 1
      this.blades.push({
        x,
        y: terrain.heightAt(x, z) - 0.03,
        z,
        rotation: random.range(0, Math.PI * 2),
        width: random.range(0.8, 1.4),
        height: random.range(0.22, 0.6) * (0.7 + clump * 0.6) * near,
        shade: random.range(0.78, 1.15),
      })
    }

    attempts = 0
    while (this.flowers.length < flowerTarget && attempts < flowerTarget * 20) {
      attempts += 1
      const x = CLEARING.x + random.range(-radius, radius)
      const z = random.range(CLEARING.z - radius * 0.8, 11)
      if (fbm(x * 0.15 + 40, z * 0.15, 2, 3) < 0.52 || !accept(x, z)) continue
      this.flowers.push({
        x,
        y: terrain.heightAt(x, z) + random.range(0.22, 0.42),
        z,
        size: random.range(0.05, 0.085),
        threshold: random.range(0, 0.9),
        color: new Color(random.pick(FLOWER_COLORS)),
      })
    }
  }
}
