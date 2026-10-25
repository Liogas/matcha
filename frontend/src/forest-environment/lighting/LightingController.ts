import { Color } from 'three'
import { DEG, lerp, smoothstep } from '../core/math'
import type { ForestController } from '../core/types'
import type { SkyModel } from '../sky/SkyModel'
import type { WorldModel } from '../world/WorldModel'
import type { LightingModel } from './LightingModel'
import type { LightingView } from './LightingView'

const MOON_COLOR = new Color('#a9bcff')
const NIGHT_SKY = new Color('#3f5590')
const DAY_GROUND = new Color('#5e5638')
const NIGHT_GROUND = new Color('#1c2536')
/** En dessous, les ombres s'étirent à l'infini : on relève l'astre pour les garder lisibles. */
const MIN_SHADOW_ELEVATION = 9 * DEG

export class LightingController implements ForestController {
  private readonly model: LightingModel
  private readonly view: LightingView
  private readonly world: WorldModel
  private readonly sky: SkyModel

  constructor(
    model: LightingModel,
    view: LightingView,
    world: WorldModel,
    sky: SkyModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
    this.sky = sky
  }

  update() {
    const { model, world, sky } = this
    const { cloudiness, mist, snowCover } = world.phenology
    const overcast = Math.max(cloudiness - 0.35, 0)

    const sunPower = smoothstep(-2, 12, world.sunAltitude) * (1 - overcast * 0.7)
    const moonPower = smoothstep(-2, 12, world.moonAltitude) * world.moonIllumination * (1 - world.daylight) * (1 - overcast * 0.6)
    const bySun = sunPower * 3 >= moonPower * 0.8

    const source = bySun ? world.sunDirection : world.moonDirection
    model.keyDirection.copy(source)
    const elevation = Math.asin(Math.min(Math.max(model.keyDirection.y, -1), 1))
    if (elevation < MIN_SHADOW_ELEVATION) {
      const horizontal = Math.hypot(model.keyDirection.x, model.keyDirection.z) || 1
      model.keyDirection.x *= Math.cos(MIN_SHADOW_ELEVATION) / horizontal
      model.keyDirection.z *= Math.cos(MIN_SHADOW_ELEVATION) / horizontal
      model.keyDirection.y = Math.sin(MIN_SHADOW_ELEVATION)
    }
    model.keyColor.copy(bySun ? sky.sunColor : MOON_COLOR)
    model.keyIntensity = bySun ? sunPower * 3 : moonPower * 0.8

    // Lumière du ciel : bleutée et plus présente quand le ciel est couvert ou enneigé
    model.skyColor.copy(NIGHT_SKY).lerp(sky.zenith, world.daylight * 0.45).lerp(sky.horizon, world.daylight * 0.5)
    model.groundColor.copy(NIGHT_GROUND).lerp(DAY_GROUND, world.daylight)
    // La nuit reste lisible : un sous-bois bleuté, jamais un écran noir
    model.ambientIntensity = lerp(1.7 + snowCover * 0.4, 1.75 + overcast * 0.7 + snowCover * 0.25, world.daylight)

    // Brume : plus dense par temps couvert et aux heures fraîches des saisons humides
    const morningMist = world.rising ? world.twilight * mist : 0
    model.fogColor.copy(sky.haze)
    model.fogNear = lerp(30, 14, morningMist)
    model.fogFar = lerp(200, 105, Math.max(overcast * 1.2, morningMist))
    model.exposure = lerp(1.45, 1.12, world.daylight)

    this.view.sync(model)
  }

  dispose() {
    this.view.dispose()
  }
}
