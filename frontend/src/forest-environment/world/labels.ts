import type { Season } from './phenology'
import type { DayPhase } from './WorldModel'

export const SEASON_LABELS: Record<Season, string> = {
  spring: 'Printemps',
  summer: 'Été',
  autumn: 'Automne',
  winter: 'Hiver',
}

export const PHASE_LABELS: Record<DayPhase, string> = {
  dawn: 'Aube',
  day: 'Jour',
  dusk: 'Crépuscule',
  night: 'Nuit',
}

export function moonName(phase: number) {
  if (phase < 0.03 || phase > 0.97) return 'Nouvelle lune'
  if (phase < 0.22) return 'Premier croissant'
  if (phase < 0.28) return 'Premier quartier'
  if (phase < 0.47) return 'Lune gibbeuse croissante'
  if (phase < 0.53) return 'Pleine lune'
  if (phase < 0.72) return 'Lune gibbeuse décroissante'
  if (phase < 0.78) return 'Dernier quartier'
  return 'Dernier croissant'
}
