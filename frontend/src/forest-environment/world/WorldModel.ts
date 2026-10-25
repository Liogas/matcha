import { Vector3 } from 'three'
import { createPhenology, type Phenology } from './phenology'

export type DayPhase = 'night' | 'dawn' | 'day' | 'dusk'

/** Résumé lisible par l'interface HTML : il ne change que quelques fois par jour. */
export interface ForestSummary {
  phase: DayPhase
  /** Thème de l'interface : la nuit commence quand le soleil passe 4° sous l'horizon. */
  isNight: boolean
  season: Phenology['season']
  /** Nom de la phase de lune, en français. */
  moonName: string
  snowing: boolean
}

/**
 * Modèle du monde : l'état du ciel et du calendrier à l'instant présent.
 * Aucune logique de rendu ici ; tous les autres éléments du décor le lisent.
 */
export class WorldModel {
  date = new Date()

  /** Directions unitaires vers le soleil et la lune, dans le repère de la scène. */
  readonly sunDirection = new Vector3(0, 1, 0)
  readonly moonDirection = new Vector3(0, -1, 0)
  /** Hauteurs en degrés. */
  sunAltitude = 45
  moonAltitude = -10
  moonPhase = 0.5
  moonIllumination = 1
  /** Angle de rotation de la voûte étoilée. */
  siderealAngle = 0

  /** 0 la nuit, 1 en plein jour, transition douce autour du lever et du coucher. */
  daylight = 1
  /** Maximal quand le soleil frôle l'horizon : lumière dorée, ciel rosé. */
  twilight = 0
  /** Vrai le matin (le soleil monte), faux le soir. */
  rising = true

  /** Force du vent (0..1), variant lentement. */
  wind = 0.35

  readonly phenology: Phenology = createPhenology()

  summary: ForestSummary = {
    phase: 'day',
    isNight: false,
    season: 'autumn',
    moonName: '',
    snowing: false,
  }
}
