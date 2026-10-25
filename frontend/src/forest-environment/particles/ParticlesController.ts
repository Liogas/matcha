import { damp, smoothstep } from '../core/math'
import { valueNoise } from '../core/noise'
import { Random } from '../core/random'
import type { ForestController, FrameContext } from '../core/types'
import type { TerrainModel } from '../terrain/TerrainModel'
import { foliageColor } from '../tree/foliagePalette'
import type { TreeModel } from '../tree/TreeModel'
import type { WorldModel } from '../world/WorldModel'
import { PARTICLE_BOX, type FallingLeaf, type ParticlesModel } from './ParticlesModel'
import type { ParticlesView } from './ParticlesView'

const LEAF_LIFETIME_ON_GROUND = 9
const FIREFLY_ZONE = { minX: -15, maxX: 7, minZ: -13, maxZ: 10 }

export class ParticlesController implements ForestController {
  private readonly random = new Random(123)

  private readonly model: ParticlesModel
  private readonly view: ParticlesView
  private readonly world: WorldModel
  private readonly tree: TreeModel
  private readonly terrain: TerrainModel

  constructor(
    model: ParticlesModel,
    view: ParticlesView,
    world: WorldModel,
    tree: TreeModel,
    terrain: TerrainModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
    this.tree = tree
    this.terrain = terrain
    // Les lucioles restent près du sol, dans la clairière et le long de l'eau
    const { fireflies } = model
    for (let index = 0; index < fireflies.count; index += 1)
      this.placeFirefly(index)
  }

  update(frame: FrameContext) {
    const { model, world } = this
    const { phenology } = world
    const delta = frame.delta
    const night = 1 - world.daylight

    // Intensités : elles suivent la saison et l'heure, en douceur
    const ease = (current: number, target: number) => (delta > 0 ? damp(current, target, 0.6, delta) : target)
    model.snow.intensity = ease(model.snow.intensity, phenology.snowfall)
    model.fireflies.intensity = ease(model.fireflies.intensity, phenology.fireflies * smoothstep(0.5, 0.95, night))
    model.motes.intensity = ease(model.motes.intensity, Math.max(phenology.pollen, phenology.leafFall * 0.3) * world.daylight * (1 - phenology.cloudiness * 0.6))

    if (delta > 0) {
      this.updateLeaves(delta, frame.elapsed)
      this.updateSnow(delta, frame.elapsed)
      this.updateFireflies(delta, frame.elapsed)
      this.updateMotes(delta, frame.elapsed)
    }
    this.view.sync(model)
  }

  dispose() {
    this.view.dispose()
  }

  private updateLeaves(delta: number, time: number) {
    const { model, world, random } = this
    const wind = world.wind
    model.leafBudget += world.phenology.leafFall * (1.2 + wind * 3) * delta
    const sources = model.leafBudget >= 1 ? this.tree.leafyClusters : []
    while (model.leafBudget >= 1) {
      model.leafBudget -= 1
      const leaf = model.leaves.find((item) => !item.active)
      if (!leaf || sources.length === 0) continue
      this.spawnLeaf(leaf, random.pick(sources).position, random.next())
    }

    for (const leaf of model.leaves) {
      if (!leaf.active) continue
      if (leaf.grounded > 0) {
        leaf.grounded += delta
        if (leaf.grounded > LEAF_LIFETIME_ON_GROUND) leaf.active = false
        continue
      }
      // Chute en feuille morte : elle se balance d'un côté à l'autre en descendant
      const sway = Math.sin(time * 2.1 + leaf.phase)
      leaf.position.x += (leaf.velocity.x + wind * 0.9 + sway * 0.55) * delta
      leaf.position.y += (leaf.velocity.y - Math.abs(sway) * 0.25) * delta
      leaf.position.z += (leaf.velocity.z + Math.cos(time * 1.7 + leaf.phase) * 0.3) * delta
      leaf.rotation.x += leaf.spin.x * delta
      leaf.rotation.y += leaf.spin.y * delta
      leaf.rotation.z = sway * 0.9
      const ground = this.terrain.heightAt(leaf.position.x, leaf.position.z) + 0.03
      if (leaf.position.y <= ground) {
        leaf.position.y = ground
        leaf.rotation.set(-Math.PI / 2, leaf.rotation.y, 0)
        leaf.grounded = 0.0001
      }
    }
  }

  private spawnLeaf(leaf: FallingLeaf, origin: { x: number; y: number; z: number }, roll: number) {
    const random = this.random
    leaf.active = true
    leaf.grounded = 0
    leaf.phase = random.range(0, Math.PI * 2)
    leaf.position.set(origin.x + random.range(-1.2, 1.2), origin.y + random.range(-0.8, 0.4), origin.z + random.range(-1.2, 1.2))
    leaf.velocity.set(random.range(-0.15, 0.15), random.range(-0.85, -0.5), random.range(-0.1, 0.25))
    leaf.rotation.set(random.range(0, 6), random.range(0, 6), 0)
    leaf.spin.set(random.range(-3, 3), random.range(-2, 2), 0)
    foliageColor(this.world.phenology.foliageStage + (roll - 0.5) * 0.8, leaf.color).multiplyScalar(0.85 + roll * 0.25)
  }

  private updateSnow(delta: number, time: number) {
    const { snow } = this.model
    if (snow.intensity < 0.01) return
    const { positions, seeds } = snow
    const wind = this.world.wind
    for (let index = 0; index < snow.count; index += 1) {
      const i = index * 3
      const seed = seeds[index]
      positions[i] += (wind * 1.4 + Math.sin(time * 0.8 + seed * 40) * 0.25) * delta
      positions[i + 1] -= (0.55 + seed * 0.5) * delta
      positions[i + 2] += Math.cos(time * 0.6 + seed * 30) * 0.2 * delta
      if (positions[i + 1] < PARTICLE_BOX.minY || positions[i] > PARTICLE_BOX.maxX) {
        positions[i] = PARTICLE_BOX.minX + (PARTICLE_BOX.maxX - PARTICLE_BOX.minX) * this.random.next()
        positions[i + 1] = PARTICLE_BOX.maxY
        positions[i + 2] = PARTICLE_BOX.minZ + (PARTICLE_BOX.maxZ - PARTICLE_BOX.minZ) * this.random.next()
      }
    }
  }

  private updateFireflies(delta: number, time: number) {
    const { fireflies } = this.model
    if (fireflies.intensity < 0.01) return
    const { positions, seeds, alphas } = fireflies
    for (let index = 0; index < fireflies.count; index += 1) {
      const i = index * 3
      const seed = seeds[index] * 100
      // Vol erratique et lent ; chaque luciole s'allume et s'éteint à son rythme
      positions[i] += (valueNoise(time * 0.3, seed) - 0.5) * 0.9 * delta
      positions[i + 1] += (valueNoise(time * 0.25, seed + 50) - 0.5) * 0.5 * delta
      positions[i + 2] += (valueNoise(time * 0.3, seed + 99) - 0.5) * 0.9 * delta
      const ground = this.terrain.heightAt(positions[i], positions[i + 2])
      positions[i + 1] = Math.min(Math.max(positions[i + 1], ground + 0.3), ground + 2.4)
      alphas[index] = Math.pow(Math.max(Math.sin(time * (0.6 + seeds[index]) + seed), 0), 3)
      if (positions[i] < FIREFLY_ZONE.minX || positions[i] > FIREFLY_ZONE.maxX || positions[i + 2] < FIREFLY_ZONE.minZ || positions[i + 2] > FIREFLY_ZONE.maxZ)
        this.placeFirefly(index)
    }
  }

  private updateMotes(delta: number, time: number) {
    const { motes } = this.model
    if (motes.intensity < 0.01) return
    const { positions, seeds, alphas } = motes
    for (let index = 0; index < motes.count; index += 1) {
      const i = index * 3
      const seed = seeds[index] * 100
      positions[i] += (this.world.wind * 0.4 + Math.sin(time * 0.2 + seed) * 0.12) * delta
      positions[i + 1] += Math.sin(time * 0.35 + seed * 2) * 0.08 * delta
      positions[i + 2] += Math.cos(time * 0.25 + seed) * 0.1 * delta
      if (positions[i] > PARTICLE_BOX.maxX) positions[i] = PARTICLE_BOX.minX
      alphas[index] = 0.4 + 0.6 * Math.max(Math.sin(time * 0.9 + seed), 0)
    }
  }

  private placeFirefly(index: number) {
    const { positions } = this.model.fireflies
    const x = this.random.range(FIREFLY_ZONE.minX, FIREFLY_ZONE.maxX)
    const z = this.random.range(FIREFLY_ZONE.minZ, FIREFLY_ZONE.maxZ)
    positions.set([x, this.terrain.heightAt(x, z) + this.random.range(0.4, 2), z], index * 3)
  }
}
