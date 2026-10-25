import { createContext } from 'react'
import type { ForestEnvironment } from '../ForestEnvironment'
import type { CubReaction } from '../wolf-cub/WolfCubModel'
import type { WorldController } from '../world/WorldController'

export interface ForestContextValue {
  world: WorldController
  /** `null` tant que le décor se construit, ou si WebGL est indisponible. */
  environment: ForestEnvironment | null
  /** Fait réagir le louveteau (like, match, message, erreur…). */
  react: (reaction: CubReaction) => void
  /** Part de l'écran couverte à droite par le contenu (0..1). */
  setRightInset: (fraction: number) => void
  animated: boolean
  setAnimated: (animated: boolean) => void
}

export const ForestContext = createContext<ForestContextValue | null>(null)
