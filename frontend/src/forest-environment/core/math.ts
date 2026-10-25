import { Color } from 'three'

export const DEG = Math.PI / 180

export function clamp(value: number, min = 0, max = 1) {
  return Math.min(max, Math.max(min, value))
}

export function lerp(a: number, b: number, t: number) {
  return a + (b - a) * t
}

export function smoothstep(edge0: number, edge1: number, value: number) {
  const t = clamp((value - edge0) / (edge1 - edge0))
  return t * t * (3 - 2 * t)
}

/**
 * Rapproche `current` de `target` indépendamment du nombre d'images par seconde.
 * `rate` : plus il est grand, plus la valeur rattrape vite sa cible.
 */
export function damp(current: number, target: number, rate: number, delta: number) {
  return lerp(current, target, 1 - Math.exp(-rate * delta))
}

/** Écart d'angle le plus court, dans [-PI, PI]. */
export function angleDelta(from: number, to: number) {
  return Math.atan2(Math.sin(to - from), Math.cos(to - from))
}

export interface Keyframe<T> {
  at: number
  value: T
}

/** Trouve les deux clés qui encadrent `at` ; `period` permet de boucler (jours de l'année). */
function bracket<T>(keys: Keyframe<T>[], at: number, period?: number) {
  const last = keys[keys.length - 1]
  const first = keys[0]
  if (at <= first.at) {
    if (!period) return { a: first, b: first, t: 0 }
    const span = first.at + period - last.at
    return { a: last, b: first, t: (at + period - last.at) / span }
  }
  if (at >= last.at) {
    if (!period) return { a: last, b: last, t: 0 }
    const span = first.at + period - last.at
    return { a: last, b: first, t: (at - last.at) / span }
  }
  for (let index = 0; index < keys.length - 1; index += 1) {
    const a = keys[index]
    const b = keys[index + 1]
    if (at <= b.at)
      return { a, b, t: (at - a.at) / (b.at - a.at) }
  }
  return { a: last, b: last, t: 0 }
}

/** Interpolation douce entre des clés numériques. */
export function sampleNumber(keys: Keyframe<number>[], at: number, period?: number) {
  const { a, b, t } = bracket(keys, at, period)
  return lerp(a.value, b.value, smoothstep(0, 1, t))
}

/** Interpolation entre des clés de couleur, écrite dans `target`. */
export function sampleColor(keys: Keyframe<Color>[], at: number, target: Color, period?: number) {
  const { a, b, t } = bracket(keys, at, period)
  return target.copy(a.value).lerp(b.value, smoothstep(0, 1, t))
}

export function colorKeys(entries: [number, string][]): Keyframe<Color>[] {
  return entries.map(([at, hex]) => ({ at, value: new Color(hex) }))
}
