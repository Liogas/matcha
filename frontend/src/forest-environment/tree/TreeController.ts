import { smoothstep } from '../core/math'
import type { ForestController, FrameContext } from '../core/types'
import type { WorldModel } from '../world/WorldModel'
import { foliageColor } from './foliagePalette'
import type { TreeModel } from './TreeModel'
import type { TreeView } from './TreeView'

export class TreeController implements ForestController {
  private lastStage = -1
  private lastFoliage = -1

  private readonly model: TreeModel
  private readonly view: TreeView
  private readonly world: WorldModel

  constructor(
    model: TreeModel,
    view: TreeView,
    world: WorldModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
  }

  update(frame: FrameContext) {
    const { model, world } = this
    model.foliage = world.phenology.foliage
    model.foliageStage = world.phenology.foliageStage

    const seasonChanged = Math.abs(model.foliageStage - this.lastStage) > 0.005 || Math.abs(model.foliage - this.lastFoliage) > 0.005
    const sway = world.wind * frame.motion

    for (const cluster of model.clusters) {
      if (seasonChanged) {
        // Chaque bouquet a son propre calendrier : certains jaunissent ou tombent avant les autres
        cluster.scale = smoothstep(cluster.threshold - 0.12, cluster.threshold + 0.06, model.foliage)
        foliageColor(model.foliageStage + cluster.lateness, cluster.color).multiplyScalar(cluster.shade)
      }
      const height = Math.max(cluster.anchor.y - 3, 0) * 0.025
      const time = frame.elapsed * 0.9 + cluster.phase
      cluster.position.set(
        cluster.anchor.x + Math.sin(time) * sway * height * 2.2,
        cluster.anchor.y + Math.sin(time * 1.7) * sway * height * 0.5,
        cluster.anchor.z + Math.cos(time * 0.8) * sway * height * 1.2,
      )
    }

    if (seasonChanged) {
      this.lastStage = model.foliageStage
      this.lastFoliage = model.foliage
    }
    this.view.sync(model, seasonChanged)
  }

  dispose() {
    this.view.dispose()
  }
}
