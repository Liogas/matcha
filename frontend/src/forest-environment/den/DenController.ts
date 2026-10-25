import type { ForestController } from '../core/types'
import type { DenView } from './DenView'

/**
 * La tanière ne bouge pas : la neige et la lumière lui sont appliquées
 * par les uniformes partagés. Le contrôleur gère seulement son cycle de vie.
 */
export class DenController implements ForestController {
  private readonly view: DenView

  constructor(view: DenView) {
    this.view = view
  }

  update() {}

  dispose() {
    this.view.dispose()
  }
}
