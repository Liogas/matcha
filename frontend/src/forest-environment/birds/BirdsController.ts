import { Color } from 'three'
import { lerp } from '../core/math'
import { Random } from '../core/random'
import type { ForestController, FrameContext } from '../core/types'
import type { SkyModel } from '../sky/SkyModel'
import type { WorldModel } from '../world/WorldModel'
import type { BirdsModel } from './BirdsModel'
import type { BirdsView } from './BirdsView'

const SPAN = 170
const SILHOUETTE = new Color('#26262b')

export class BirdsController implements ForestController {
  private readonly random = new Random(64)

  private readonly model: BirdsModel
  private readonly view: BirdsView
  private readonly world: WorldModel
  private readonly sky: SkyModel

  constructor(
    model: BirdsModel,
    view: BirdsView,
    world: WorldModel,
    sky: SkyModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
    this.sky = sky
  }

  update(frame: FrameContext) {
    const { model, world, random } = this
    const delta = frame.delta

    if (delta > 0) {
      model.nextFlock -= delta
      const activity = world.phenology.birds * world.daylight * (1 - world.phenology.snowfall)
      if (model.nextFlock <= 0) {
        if (activity > 0.2 && random.chance(activity)) this.launchFlock()
        model.nextFlock = random.range(30, 85)
      }
      for (const bird of model.birds) {
        if (!bird.active) continue
        bird.x += bird.direction * bird.speed * delta
        bird.y += Math.sin(bird.flap * 0.15) * 0.4 * delta
        bird.glide -= delta
        if (bird.glide < -random.range(2, 5)) bird.glide = random.range(0.8, 2.2)
        if (bird.glide <= 0) bird.flap += bird.flapRate * delta
        if (Math.abs(bird.x) > SPAN) bird.active = false
      }
    }

    // Silhouettes sombres, adoucies par la brume des lointains
    model.color.copy(SILHOUETTE).lerp(this.sky.haze, lerp(0.15, 0.35, world.phenology.cloudiness))
    this.view.sync(model)
  }

  dispose() {
    this.view.dispose()
  }

  private launchFlock() {
    const { random } = this
    const direction = random.chance(0.5) ? 1 : -1
    const size = Math.floor(random.range(3, 8))
    const y = random.range(24, 42)
    const z = random.range(-120, -60)
    const speed = random.range(8, 11)
    let launched = 0
    for (const bird of this.model.birds) {
      if (bird.active || launched >= size) continue
      // Formation en V approximative : chacun un peu en retrait du précédent
      const rank = Math.ceil(launched / 2)
      const side = launched % 2 ? 1 : -1
      bird.active = true
      bird.direction = direction
      bird.x = -direction * (SPAN - 5) - direction * rank * random.range(2.5, 4)
      bird.y = y - rank * random.range(0.4, 1) + side * 0.3
      bird.z = z + side * rank * random.range(1.5, 3)
      bird.speed = speed * random.range(0.97, 1.03)
      bird.flap = random.range(0, 6)
      bird.flapRate = random.range(9, 12)
      bird.glide = 0
      launched += 1
    }
  }
}
