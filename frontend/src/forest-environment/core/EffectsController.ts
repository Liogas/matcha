import type { WorldModel } from '../world/WorldModel'
import { damp } from './math'
import type { SharedUniforms } from './materialEffects'
import type { ForestController, FrameContext } from './types'

/** Met à jour les uniformes communs (temps, vent, neige) lus par tous les matériaux. */
export class EffectsController implements ForestController {
  private readonly shared: SharedUniforms
  private readonly world: WorldModel

  constructor(
    shared: SharedUniforms,
    world: WorldModel,
  ) {
    this.shared = shared
    this.world = world
  }

  update(frame: FrameContext) {
    this.shared.uTime.value = frame.elapsed
    this.shared.uWind.value = frame.delta > 0
      ? damp(this.shared.uWind.value, this.world.wind, 0.5, frame.delta)
      : this.world.wind * frame.motion
    this.shared.uSnow.value = this.world.phenology.snowCover
  }

  dispose() {}
}
