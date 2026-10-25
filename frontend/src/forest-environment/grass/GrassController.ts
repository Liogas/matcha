import type { ForestController } from '../core/types'
import type { WorldModel } from '../world/WorldModel'
import type { GrassModel } from './GrassModel'
import type { GrassView } from './GrassView'

export class GrassController implements ForestController {
  private lastBloom = -1

  private readonly model: GrassModel
  private readonly view: GrassView
  private readonly world: WorldModel

  constructor(
    model: GrassModel,
    view: GrassView,
    world: WorldModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
  }

  update() {
    const { model, world } = this
    // Les brins sont un peu plus clairs que le sol vu de loin
    model.color.copy(world.phenology.grassColor).multiplyScalar(1.12)
    model.bloom = world.phenology.flowers * (1 - world.phenology.snowCover)
    const bloomChanged = Math.abs(model.bloom - this.lastBloom) > 0.005
    if (bloomChanged) this.lastBloom = model.bloom
    this.view.sync(model, bloomChanged)
  }

  dispose() {
    this.view.dispose()
  }
}
