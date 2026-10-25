import { Color } from 'three'
import { smoothstep } from '../core/math'
import type { ForestController, FrameContext } from '../core/types'
import type { WorldModel } from '../world/WorldModel'
import type { TerrainModel } from './TerrainModel'
import type { TerrainView } from './TerrainView'

const REPAINT_INTERVAL = 0.5
const FOREST_FLOOR = new Color('#4b4630')
const AUTUMN_FLOOR = new Color('#5e4426')

export class TerrainController implements ForestController {
  private readonly lastGrass = new Color()
  private lastLitter = -1
  private sinceRepaint = 0

  private readonly model: TerrainModel
  private readonly view: TerrainView
  private readonly world: WorldModel

  constructor(
    model: TerrainModel,
    view: TerrainView,
    world: WorldModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
  }

  update(frame: FrameContext) {
    const { model, world } = this
    const { grassColor, foliageStage } = world.phenology

    model.grass.copy(grassColor)
    model.litterAmount = smoothstep(2.4, 3.8, foliageStage)
    model.forestFloor.copy(FOREST_FLOOR).lerp(AUTUMN_FLOOR, model.litterAmount * 0.6)

    // Le sol ne change qu'au fil des jours : on ne repeint que si la saison a bougé
    this.sinceRepaint += frame.delta
    const changed = Math.abs(model.litterAmount - this.lastLitter) > 0.01 ||
      Math.abs(model.grass.r - this.lastGrass.r) + Math.abs(model.grass.g - this.lastGrass.g) + Math.abs(model.grass.b - this.lastGrass.b) > 0.004
    if (changed && (this.sinceRepaint >= REPAINT_INTERVAL || this.lastLitter < 0)) {
      this.view.repaint(model)
      this.lastGrass.copy(model.grass)
      this.lastLitter = model.litterAmount
      this.sinceRepaint = 0
    }
  }

  dispose() {
    this.view.dispose()
  }
}
