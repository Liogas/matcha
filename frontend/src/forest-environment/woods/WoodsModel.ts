import { Color } from 'three'
import { CAMERA, CLEARING, DEN, TREE } from '../core/composition'
import { fbm } from '../core/noise'
import { Random } from '../core/random'
import type { ForestQuality } from '../core/types'
import type { TerrainModel } from '../terrain/TerrainModel'

export type WoodSpecies = 'pine' | 'broadleaf'

export interface WoodTree {
  species: WoodSpecies
  x: number
  y: number
  z: number
  scale: number
  rotation: number
  /** Seuil de densité du feuillage (feuillus). */
  threshold: number
  lateness: number
  shade: number
  /** Couleur courante du houppier (feuillus) ou teinte (résineux). */
  readonly color: Color
  crownScale: number
  /** Silhouette propre à chaque houppier : plus large, plus haut… */
  readonly stretch: [number, number]
  /** Tendance de l'essence en automne : -1 vers le jaune (bouleau), 1 vers le rouge (érable). */
  readonly hue: number
}

/**
 * La forêt qui entoure la clairière. Placement sur une grille perturbée :
 * dense au fond, clairsemée à la lisière, jamais dans l'eau ni devant la caméra.
 */
export class WoodsModel {
  readonly trees: WoodTree[] = []
  foliage = 1
  foliageStage = 1

  constructor(terrain: TerrainModel, quality: ForestQuality) {
    const random = new Random(2024)
    const cell = quality === 'high' ? 3.1 : 4.4
    const cameraX = CAMERA.position[0]

    for (let gx = -95; gx <= 95; gx += cell) {
      for (let gz = -135; gz <= 34; gz += cell) {
        const x = gx + random.range(-cell * 0.45, cell * 0.45)
        const z = gz + random.range(-cell * 0.45, cell * 0.45)
        const edgeNoise = fbm(x * 0.08, z * 0.08, 2, 77)
        const fromClearing = Math.hypot(x - CLEARING.x, z - CLEARING.z)
        if (fromClearing < CLEARING.radius + 1.5 + edgeNoise * 7) continue

        // Couloir de vue : les arbres proches de la caméra ne font qu'encadrer l'image
        if (z > 0 && Math.abs(x - cameraX) < 8 + z * 0.9) continue
        if (Math.hypot(x - TREE.x, z - TREE.z) < 7 || Math.hypot(x - DEN.x, z - DEN.z) < 5) continue

        const { distance, t } = terrain.river.nearest(x, z)
        if (distance < terrain.river.widthAt(t) / 2 + 2.6) continue

        // Plus dense en s'éloignant de la clairière ; quelques trouées naturelles
        const density = Math.min(0.35 + (fromClearing - CLEARING.radius) * 0.025, 0.92) * (edgeNoise > 0.68 ? 0.3 : 1)
        if (!random.chance(density)) continue

        const coniferous = fbm(x * 0.03, z * 0.03, 2, 5) > 0.43 || z < -70
        this.trees.push({
          species: coniferous ? 'pine' : 'broadleaf',
          x,
          y: terrain.heightAt(x, z) - 0.15,
          z,
          scale: random.range(0.75, 1.3) * (fromClearing < CLEARING.radius + 8 ? 0.85 : 1),
          rotation: random.range(0, Math.PI * 2),
          threshold: random.range(0.05, 0.9),
          lateness: random.range(-0.7, 0.55),
          shade: random.range(0.78, 1.12),
          color: new Color(),
          crownScale: 1,
          stretch: [random.range(0.75, 1.25), random.range(0.8, 1.3)],
          hue: random.range(-1, 1),
        })
      }
    }
  }

  get pines() {
    return this.trees.filter((tree) => tree.species === 'pine')
  }

  get broadleaves() {
    return this.trees.filter((tree) => tree.species === 'broadleaf')
  }
}
