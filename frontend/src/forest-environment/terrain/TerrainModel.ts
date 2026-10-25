import { Color } from 'three'
import { CLEARING, DEN, TREE } from '../core/composition'
import { lerp, smoothstep } from '../core/math'
import { fbm } from '../core/noise'
import type { RiverPath } from '../river/RiverPath'

export const TERRAIN_BOUNDS = { minX: -130, maxX: 130, minZ: -155, maxZ: 45 }
/** Largeur des berges en pente douce de part et d'autre de l'eau. */
const BANK = 2.2
/** Largeur de la vallée que la rivière a creusée dans les collines. */
const VALLEY = 16

/** Types de sol mélangés sur chaque facette du terrain. */
export interface GroundWeights {
  grass: number
  dirt: number
  rock: number
  forest: number
  litter: number
}

/**
 * Le relief : une clairière presque plate entourée de collines,
 * la rivière qui descend du fond et la butte de la tanière.
 * Tous les éléments posés au sol interrogent ce modèle.
 */
export class TerrainModel {
  readonly grass = new Color('#6b9440')
  readonly dirt = new Color('#7d6a4c')
  readonly rock = new Color('#6e6b64')
  readonly forestFloor = new Color('#4b4630')
  readonly litter = new Color('#9a5a2a')
  /** Part de feuilles mortes sous les arbres (automne). */
  litterAmount = 0

  readonly river: RiverPath

  constructor(river: RiverPath) {
    this.river = river
  }

  heightAt(x: number, z: number) {
    const fromClearing = Math.hypot(x - CLEARING.x, z - CLEARING.z)
    let height = (fbm(x * 0.045, z * 0.045, 3, 5) - 0.5) * 1.3
    height += smoothstep(CLEARING.radius, CLEARING.radius + 38, fromClearing) * (2.5 + fbm(x * 0.018, z * 0.018, 4, 9) * 10)
    height += smoothstep(-25, -135, z) * 7
    // La butte de la tanière, adossée aux racines de l'arbre
    height += 1.1 * Math.exp(-((x - DEN.x + 0.8) ** 2 + (z - DEN.z + 1) ** 2) / 7)
    height += 0.5 * Math.exp(-((x - TREE.x) ** 2 + (z - TREE.z) ** 2) / 10)

    const { distance, t } = this.river.nearest(x, z)
    const half = this.river.widthAt(t) / 2
    const level = this.river.levelAt(t)

    if (distance < half) {
      // Lit de la rivière : plus profond au centre
      const depth = 1 - (distance / half) ** 2
      return level - 0.12 - depth * 0.6
    }
    // Vallée : les collines s'abaissent vers le niveau de l'eau
    const valley = smoothstep(half + BANK, half + BANK + VALLEY, distance)
    height = lerp(Math.min(height, level + 0.4), height, valley)
    // Berge : de l'eau jusqu'au terrain naturel, jamais sous le niveau de l'eau
    const bank = smoothstep(half, half + BANK, distance)
    return Math.max(lerp(level + 0.06, height, bank), level + 0.06 + bank * 0.08)
  }

  /** Composition du sol en un point, selon la pente et le voisinage. */
  groundAt(x: number, z: number, slope: number, target: GroundWeights) {
    const { distance, t } = this.river.nearest(x, z)
    const half = this.river.widthAt(t) / 2
    const fromClearing = Math.hypot(x - CLEARING.x, z - CLEARING.z)
    const fromDen = Math.hypot(x - (DEN.x + Math.sin(DEN.facing) * 1.6), z - (DEN.z + Math.cos(DEN.facing) * 1.6))
    const fromTree = Math.hypot(x - TREE.x, z - TREE.z)
    const speckle = fbm(x * 0.4, z * 0.4, 2, 31)

    target.rock = smoothstep(0.55, 0.8, slope)
    target.dirt = Math.max(
      1 - smoothstep(half, half + 1.6 + speckle, distance),
      1 - smoothstep(0.8, 2.6, fromDen + speckle * 1.2),
    )
    target.forest = smoothstep(CLEARING.radius + 4, CLEARING.radius + 16, fromClearing + speckle * 6)
    target.litter = (1 - smoothstep(3, 8.5, fromTree + speckle * 3)) * 0.9
    target.grass = 1
    return target
  }
}
