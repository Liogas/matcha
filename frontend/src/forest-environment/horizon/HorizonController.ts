import { Color } from 'three'
import { lerp, smoothstep } from '../core/math'
import type { ForestController } from '../core/types'
import type { SkyModel } from '../sky/SkyModel'
import type { WorldModel } from '../world/WorldModel'
import type { HorizonModel } from './HorizonModel'
import type { HorizonView } from './HorizonView'

const AUTUMN_FOREST = new Color('#6b4f2a')
const WINTER_FOREST = new Color('#4a4a44')
const SPRING_FOREST = new Color('#4d6b3a')

export class HorizonController implements ForestController {
  private readonly tint = new Color()

  private readonly model: HorizonModel
  private readonly view: HorizonView
  private readonly world: WorldModel
  private readonly sky: SkyModel

  constructor(
    model: HorizonModel,
    view: HorizonView,
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
    const { foliageStage, foliage, snowCover, mist, cloudiness } = world.phenology

    // Les forêts lointaines suivent la saison, atténuée : beaucoup de résineux
    const autumn = smoothstep(1.6, 3.2, foliageStage) * foliage
    const bare = 1 - foliage
    const fresh = smoothstep(0.6, 0, foliageStage) * foliage
    const moonlight = smoothstep(0, 20, world.moonAltitude) * world.moonIllumination
    const light = lerp(0.22 + moonlight * 0.15, 1, world.daylight)

    model.layers.forEach((layer, index) => {
      this.tint.copy(layer.baseColor)
      if (index < 2) {
        this.tint.lerp(AUTUMN_FOREST, autumn * 0.4)
        this.tint.lerp(WINTER_FOREST, bare * 0.35)
        this.tint.lerp(SPRING_FOREST, fresh * 0.3)
      }
      layer.color.copy(this.tint).multiplyScalar(light)
      // Au coucher du soleil, les crêtes prennent une pointe de la lumière chaude
      layer.color.lerp(sky.glow, world.twilight * 0.08 * (index + 1))
    })

    model.haze.copy(sky.haze)
    model.snow = Math.max(snowCover, world.phenology.season === 'winter' ? 0.7 : 0, smoothstep(2.5, 4, foliageStage) * 0.4)
    model.snowLine = lerp(78, 52, snowCover)
    model.mist = Math.min(1, 0.25 + mist * 0.35 + (world.rising ? world.twilight * mist * 0.5 : 0) + cloudiness * 0.15)

    this.view.sync(model)
  }

  dispose() {
    this.view.dispose()
  }
}
