import { hash01 } from './random'

function lattice(x: number, y: number, seed: number) {
  return hash01(x * 374761393 + y * 668265263 + seed * 1442695041)
}

function fade(t: number) {
  return t * t * (3 - 2 * t)
}

/** Bruit de valeur 2D lissé, dans [0, 1]. */
export function valueNoise(x: number, y: number, seed = 0) {
  const x0 = Math.floor(x)
  const y0 = Math.floor(y)
  const tx = fade(x - x0)
  const ty = fade(y - y0)
  const a = lattice(x0, y0, seed)
  const b = lattice(x0 + 1, y0, seed)
  const c = lattice(x0, y0 + 1, seed)
  const d = lattice(x0 + 1, y0 + 1, seed)
  return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty
}

/** Somme d'octaves de bruit, dans [0, 1] : reliefs, silhouettes de crêtes. */
export function fbm(x: number, y: number, octaves = 4, seed = 0) {
  let total = 0
  let amplitude = 0.5
  let frequency = 1
  let norm = 0
  for (let octave = 0; octave < octaves; octave += 1) {
    total += valueNoise(x * frequency, y * frequency, seed + octave * 17) * amplitude
    norm += amplitude
    amplitude *= 0.5
    frequency *= 2.03
  }
  return total / norm
}
