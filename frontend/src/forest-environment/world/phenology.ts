import { Color } from 'three'
import { clamp, colorKeys, sampleColor, sampleNumber, smoothstep, type Keyframe } from '../core/math'
import { hash01 } from '../core/random'

/**
 * Le calendrier de la nature : comment la forêt évolue au fil de l'année.
 * Toutes les valeurs sont continues : pas de bascule brutale d'une saison à l'autre.
 */
export type Season = 'spring' | 'summer' | 'autumn' | 'winter'

export interface Phenology {
  season: Season
  /** Densité du feuillage des feuillus (0 = nu, 1 = plein). */
  foliage: number
  /** Couleur des feuilles : 0 bourgeons, 1 vert d'été, 2 jaune, 3 orangé, 4 rouille. */
  foliageStage: number
  /** Couverture neigeuse au sol (0..1). */
  snowCover: number
  /** Chute de neige en cours (0..1), tirée au sort de façon stable pour chaque jour. */
  snowfall: number
  /** Glace sur les bords de la rivière (0..1). */
  ice: number
  grassColor: Color
  flowers: number
  leafFall: number
  /** Saison des lucioles (l'heure est gérée ailleurs). */
  fireflies: number
  pollen: number
  cloudiness: number
  /** Propension aux brumes matinales. */
  mist: number
  birds: number
}

const YEAR = 365.25

/** Jour de l'année (0 = 1er janvier) pour un mois 1..12. */
function day(month: number, date: number) {
  const starts = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334]
  return starts[month - 1] + date - 1
}

function keys(entries: [number, number, number][]): Keyframe<number>[] {
  return entries.map(([month, date, value]) => ({ at: day(month, date), value }))
}

const FOLIAGE = keys([
  [3, 25, 0], [4, 12, 0.35], [5, 6, 1], [10, 22, 1], [11, 8, 0.6], [11, 24, 0.15], [12, 6, 0],
])

// Reste « rouille » tout l'hiver (arbres nus), puis repart des bourgeons au printemps
const FOLIAGE_STAGE = keys([
  [3, 23, 4], [3, 25, 0], [5, 20, 1], [9, 8, 1], [10, 4, 2], [10, 26, 3], [11, 16, 4],
])

const SNOW_COVER = keys([
  [1, 6, 0.85], [2, 12, 0.7], [3, 8, 0], [12, 10, 0], [12, 28, 0.55],
])

const ICE = keys([
  [1, 15, 0.55], [2, 20, 0.3], [3, 5, 0], [12, 12, 0], [12, 30, 0.35],
])

const SNOW_PROBABILITY = keys([
  [1, 15, 0.45], [2, 15, 0.35], [3, 15, 0.08], [4, 1, 0], [11, 25, 0], [12, 20, 0.35],
])

const GRASS = colorKeys([
  [day(1, 15), '#7c7f57'],
  [day(3, 20), '#7f9a4c'],
  [day(5, 1), '#6fa046'],
  [day(7, 1), '#6b9440'],
  [day(8, 15), '#9a9a52'],
  [day(10, 1), '#8a9048'],
  [day(11, 15), '#86804e'],
])

const FLOWERS = keys([[3, 25, 0], [4, 25, 1], [6, 30, 0.8], [8, 25, 0.15], [9, 20, 0]])
const FIREFLIES = keys([[5, 25, 0], [6, 20, 1], [8, 5, 0.9], [9, 5, 0]])
const POLLEN = keys([[3, 15, 0], [4, 20, 1], [6, 25, 0.7], [8, 31, 0.1], [9, 20, 0]])
const CLOUDS = keys([[1, 15, 0.62], [4, 15, 0.45], [7, 15, 0.22], [10, 15, 0.5], [12, 1, 0.6]])
const MIST = keys([[1, 15, 0.5], [4, 15, 0.6], [7, 15, 0.2], [10, 20, 0.95], [12, 1, 0.65]])
const BIRDS = keys([[1, 15, 0.25], [4, 10, 1], [8, 20, 1], [10, 20, 0.6], [12, 1, 0.25]])

export function dayOfYear(date: Date) {
  const start = new Date(date.getFullYear(), 0, 1)
  return (date.getTime() - start.getTime()) / 86_400_000
}

export function seasonOf(doy: number): Season {
  if (doy >= day(12, 21) || doy < day(3, 20)) return 'winter'
  if (doy < day(6, 21)) return 'spring'
  if (doy < day(9, 22)) return 'summer'
  return 'autumn'
}

export function computePhenology(date: Date, target: Phenology) {
  const doy = dayOfYear(date)
  const dayIndex = date.getFullYear() * 1000 + Math.floor(doy)

  target.season = seasonOf(doy)
  target.foliage = sampleNumber(FOLIAGE, doy, YEAR)
  target.foliageStage = sampleNumber(FOLIAGE_STAGE, doy, YEAR)
  target.snowCover = sampleNumber(SNOW_COVER, doy, YEAR)
  target.ice = sampleNumber(ICE, doy, YEAR)
  sampleColor(GRASS, doy, target.grassColor, YEAR)
  target.flowers = sampleNumber(FLOWERS, doy, YEAR)
  target.fireflies = sampleNumber(FIREFLIES, doy, YEAR)
  target.pollen = sampleNumber(POLLEN, doy, YEAR)
  target.mist = sampleNumber(MIST, doy, YEAR)
  target.birds = sampleNumber(BIRDS, doy, YEAR)

  // Un peu de météo : chaque jour a son propre ciel, stable pendant toute la journée
  const weather = hash01(dayIndex)
  target.cloudiness = clamp(sampleNumber(CLOUDS, doy, YEAR) + (weather - 0.5) * 0.45)
  const snowChance = sampleNumber(SNOW_PROBABILITY, doy, YEAR)
  target.snowfall = weather < snowChance ? 0.55 + hash01(dayIndex + 7) * 0.45 : 0
  if (target.snowfall > 0)
    target.cloudiness = Math.max(target.cloudiness, 0.75)

  // Les feuilles tombent surtout quand elles ont pris leurs couleurs d'automne
  target.leafFall = smoothstep(2, 3.2, target.foliageStage) * smoothstep(0, 0.25, target.foliage)
  return target
}

export function createPhenology(): Phenology {
  return {
    season: 'autumn',
    foliage: 1,
    foliageStage: 1,
    snowCover: 0,
    snowfall: 0,
    ice: 0,
    grassColor: new Color('#6b9440'),
    flowers: 0,
    leafFall: 0,
    fireflies: 0,
    pollen: 0,
    cloudiness: 0.4,
    mist: 0.5,
    birds: 0.5,
  }
}
