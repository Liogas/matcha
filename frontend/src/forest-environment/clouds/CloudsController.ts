import { Color } from 'three'
import { damp, smoothstep } from '../core/math'
import type { ForestController, FrameContext } from '../core/types'
import type { SkyModel } from '../sky/SkyModel'
import type { WorldModel } from '../world/WorldModel'
import { CLOUD_SPAN, type CloudsModel } from './CloudsModel'
import type { CloudsView } from './CloudsView'

const DAY_LIT = new Color('#fffaf1')
const DAY_SHADE = new Color('#a9b4c2')
const NIGHT_LIT = new Color('#46527a')
const NIGHT_SHADE = new Color('#121a30')

export class CloudsController implements ForestController {
  private readonly scratch = new Color()

  private readonly model: CloudsModel
  private readonly view: CloudsView
  private readonly world: WorldModel
  private readonly sky: SkyModel

  constructor(
    model: CloudsModel,
    view: CloudsView,
    world: WorldModel,
    sky: SkyModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
    this.sky = sky
  }

  update(frame: FrameContext) {
    const { model, world, sky } = this
    const { cloudiness } = world.phenology
    const night = 1 - world.daylight
    const moonlight = smoothstep(0, 15, world.moonAltitude) * world.moonIllumination

    for (const cloud of model.clouds) {
      cloud.x += cloud.speed * (0.4 + world.wind * 1.6) * frame.delta
      if (cloud.x > CLOUD_SPAN) cloud.x -= CLOUD_SPAN * 2
      // Les nuages apparaissent et se dissipent doucement quand la météo change
      const target = smoothstep(cloud.threshold, cloud.threshold + 0.18, cloudiness * 1.15 + 0.05)
      cloud.opacity = frame.delta > 0 ? damp(cloud.opacity, target, 0.2, frame.delta) : target
    }

    // Jour : blanc ; coucher : teinté par le soleil ; nuit : gris bleuté, plus clair sous la lune
    model.lit.copy(DAY_LIT).lerp(sky.glow, world.twilight * 0.75)
    model.lit.lerp(this.scratch.copy(NIGHT_LIT).multiplyScalar(0.45 + moonlight * 0.7), night)
    model.shade.copy(DAY_SHADE).lerp(sky.horizon, 0.3 + world.twilight * 0.3)
    model.shade.lerp(NIGHT_SHADE, night)
    // Ciel lourd : dessous des nuages plus sombre
    model.shade.multiplyScalar(1 - Math.max(cloudiness - 0.5, 0) * 0.6)
    model.sunSide = world.sunDirection.x

    this.view.sync(model)
  }

  dispose() {
    this.view.dispose()
  }
}
