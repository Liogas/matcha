import { Color } from 'three'
import { smoothstep } from '../core/math'
import type { ForestController } from '../core/types'
import { foliageColor } from '../tree/foliagePalette'
import type { WorldModel } from '../world/WorldModel'
import type { WoodsModel } from './WoodsModel'
import type { WoodsView } from './WoodsView'

const BIRCH = new Color('#d9b23a')
const MAPLE = new Color('#a8321e')

export class WoodsController implements ForestController {
  private lastStage = -1
  private lastFoliage = -1

  private readonly model: WoodsModel
  private readonly view: WoodsView
  private readonly world: WorldModel

  constructor(
    model: WoodsModel,
    view: WoodsView,
    world: WorldModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
  }

  update() {
    const { model, world } = this
    const { foliage, foliageStage } = world.phenology
    // Le feuillage évolue sur des jours : repeindre à chaque image serait du gaspillage
    if (Math.abs(foliage - this.lastFoliage) < 0.004 && Math.abs(foliageStage - this.lastStage) < 0.004)
      return
    model.foliage = foliage
    model.foliageStage = foliageStage
    for (const tree of model.trees) {
      if (tree.species === 'pine') {
        tree.color.setScalar(tree.shade)
        continue
      }
      tree.crownScale = smoothstep(tree.threshold - 0.15, tree.threshold + 0.05, foliage)
      foliageColor(foliageStage + tree.lateness, tree.color).multiplyScalar(tree.shade * 0.92)
      // En automne, chaque essence prend sa teinte : bouleaux jaunes, érables rouges
      const autumn = smoothstep(1.4, 2.6, foliageStage + tree.lateness)
      tree.color.lerp(tree.hue < 0 ? BIRCH : MAPLE, Math.abs(tree.hue) * autumn * 0.45)
    }
    this.lastFoliage = foliage
    this.lastStage = foliageStage
    this.view.sync(model)
  }

  dispose() {
    this.view.dispose()
  }
}
