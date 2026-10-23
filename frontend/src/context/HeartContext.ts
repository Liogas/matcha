import { createContext } from 'react'
import type { LivingEntityController } from '../components/living-entity/LivingEntityController'
import type { EntityReaction, LivingEntityModel } from '../components/living-entity/LivingEntityModel'

export interface HeartContextValue {
  model: LivingEntityModel
  controller: LivingEntityController
  /** Déclenche une réaction du cœur (like, match, erreur…). */
  react: (reaction: EntityReaction) => void
}

export const HeartContext = createContext<HeartContextValue | null>(null)
