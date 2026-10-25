import { Vector3 } from 'three'
import { DEN } from '../core/composition'
import { Random } from '../core/random'
import type { TerrainModel } from '../terrain/TerrainModel'

export interface Boulder {
  position: Vector3
  scale: Vector3
  rotation: number
  size: number
  seed: number
}

export interface Log {
  start: Vector3
  end: Vector3
  radius: number
}

/**
 * La tanière : des blocs de roche en fer à cheval, adossés aux racines du chêne,
 * qui ménagent une entrée sombre tournée vers la clairière.
 */
export class DenModel {
  readonly boulders: Boulder[] = []
  readonly logs: Log[] = []
  /** Point devant l'entrée, au sol : le louveteau y dort. */
  readonly doorstep: Vector3
  readonly entrance: Vector3
  readonly facing = DEN.facing

  constructor(terrain: TerrainModel) {
    const random = new Random(31)
    const forward = new Vector3(Math.sin(DEN.facing), 0, Math.cos(DEN.facing))
    const center = new Vector3(DEN.x, terrain.heightAt(DEN.x, DEN.z), DEN.z)

    // Repère local : x vers la droite de l'entrée, z vers l'extérieur
    const side = new Vector3(Math.cos(DEN.facing), 0, -Math.sin(DEN.facing))
    const place = (lateral: number, depth: number, size: number, scale: [number, number, number], lift: number, seed: number) => {
      const x = center.x + side.x * lateral + forward.x * depth
      const z = center.z + side.z * lateral + forward.z * depth
      this.boulders.push({
        position: new Vector3(x, terrain.heightAt(x, z) + lift, z),
        scale: new Vector3(...scale),
        rotation: random.range(0, Math.PI * 2),
        size,
        seed,
      })
    }
    // Deux piliers encadrent l'entrée, d'autres blocs ferment le fond
    place(-1.15, 0.3, 0.75, [0.9, 1.25, 1.1], 0.35, 3)
    place(1.2, 0.25, 0.8, [0.95, 1.2, 1.05], 0.35, 20)
    place(-1.3, -0.9, 0.95, [1.1, 1, 1.1], 0.2, 37)
    place(1.25, -1, 1, [1.1, 0.95, 1.15], 0.2, 54)
    place(0, -1.55, 1.15, [1.3, 1, 1], 0.25, 71)
    place(-2, 0.1, 0.6, [1, 0.7, 1], 0, 88)
    place(2, -0.2, 0.55, [1, 0.75, 1], 0, 105)
    // Dalle de couverture posée sur les piliers : le toit de la tanière
    this.boulders.push({
      position: center.clone().add(new Vector3(0, 1.35, 0)).addScaledVector(forward, -0.15),
      scale: new Vector3(1.7, 0.45, 1.3),
      rotation: DEN.facing,
      size: 1.15,
      seed: 401,
    })

    this.entrance = center.clone().addScaledVector(forward, 0.1).add(new Vector3(0, 0.02, 0))
    const doorX = center.x + forward.x * 1.5
    const doorZ = center.z + forward.z * 1.5
    this.doorstep = new Vector3(doorX, terrain.heightAt(doorX, doorZ), doorZ)

    // Bois mort : un tronc couché près de la rivière, quelques branches près de l'entrée
    const logs: [number, number, number, number, number][] = [
      [-1.2, -8.2, 2.1, -6.6, 0.3],
      [-6.4, 0.6, -5.2, 1.4, 0.09],
      [-2.9, -1.4, -2.2, -1.9, 0.07],
      [-9.8, -0.5, -8.6, 0.9, 0.16],
    ]
    for (const [x1, z1, x2, z2, radius] of logs) {
      this.logs.push({
        start: new Vector3(x1, terrain.heightAt(x1, z1) + radius * 0.7, z1),
        end: new Vector3(x2, terrain.heightAt(x2, z2) + radius * 0.7, z2),
        radius,
      })
    }
  }
}
