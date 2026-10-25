import { Color } from 'three'
import { lerp, smoothstep } from '../core/math'
import type { ForestController, FrameContext } from '../core/types'
import type { LightingModel } from '../lighting/LightingModel'
import type { SkyModel } from '../sky/SkyModel'
import type { WorldModel } from '../world/WorldModel'
import type { RiverModel } from './RiverModel'
import type { RiverView } from './RiverView'

const DAY_DEEP = new Color('#1f4044')
const DAY_SHALLOW = new Color('#5b8676')
const NIGHT_DEEP = new Color('#060b12')
const NIGHT_SHALLOW = new Color('#14202a')

export class RiverController implements ForestController {
  private readonly model: RiverModel
  private readonly view: RiverView
  private readonly world: WorldModel
  private readonly sky: SkyModel
  private readonly lighting: LightingModel

  constructor(
    model: RiverModel,
    view: RiverView,
    world: WorldModel,
    sky: SkyModel,
    lighting: LightingModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
    this.sky = sky
    this.lighting = lighting
  }

  update(frame: FrameContext) {
    const { model, world, sky, lighting } = this
    const { mist, ice, cloudiness } = world.phenology

    model.flow += frame.delta * 0.32
    model.deep.copy(NIGHT_DEEP).lerp(DAY_DEEP, world.daylight)
    model.shallow.copy(NIGHT_SHALLOW).lerp(DAY_SHALLOW, world.daylight)
    model.reflection.copy(sky.horizon).lerp(sky.zenith, 0.35)
    model.foam.setRGB(0.85, 0.9, 0.9).multiplyScalar(lerp(0.12, 1, world.daylight))

    // Les reflets viennent de l'astre qui éclaire la scène : soleil ou lune
    model.glintDirection.copy(lighting.keyDirection)
    model.glintColor.copy(lighting.keyColor)
    model.glintStrength = Math.min(lighting.keyIntensity, 2.6) * (1 - Math.max(cloudiness - 0.5, 0))
    model.ice = ice

    // Brume matinale au ras de l'eau, un peu toute la journée en automne
    const morning = world.rising ? smoothstep(-6, 2, world.sunAltitude) * (1 - smoothstep(6, 22, world.sunAltitude)) : 0
    model.mist = Math.min(1, mist * (morning * 0.9 + 0.18) + (1 - world.daylight) * 0.12)
    model.mistColor.copy(sky.haze).lerp(sky.horizon, 0.4)

    this.view.sync(model, frame)
  }

  dispose() {
    this.view.dispose()
  }
}
