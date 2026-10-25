import type { Color } from 'three'
import { colorKeys, sampleColor } from '../core/math'

/** Couleurs des feuilles selon leur stade : bourgeon, été, jaune, orangé, rouille. */
const FOLIAGE = colorKeys([
  [0, '#a6c862'],
  [1, '#557f36'],
  [2, '#c7a136'],
  [3, '#cc6a28'],
  [4, '#8b4426'],
])

/** Partagé par le grand arbre, les feuillus du fond et les feuilles qui tombent. */
export function foliageColor(stage: number, target: Color) {
  return sampleColor(FOLIAGE, Math.min(Math.max(stage, 0), 4), target)
}
