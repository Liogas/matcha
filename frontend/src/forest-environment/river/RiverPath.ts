import { CatmullRomCurve3, Vector3 } from 'three'
import { RIVER_LEVEL, RIVER_PATH, RIVER_WIDTH } from '../core/composition'
import { lerp, smoothstep } from '../core/math'
import { fbm } from '../core/noise'

export interface RiverSample {
  /** Distance horizontale à l'axe de la rivière. */
  distance: number
  /** Position le long du cours, de 0 (amont) à 1 (aval). */
  t: number
}

const SAMPLES = 420

/**
 * Le tracé de la rivière : partagé par le terrain (qui creuse son lit)
 * et par l'eau elle-même. Le cours va toujours vers +z, ce qui permet
 * de chercher le point le plus proche sans tout parcourir.
 */
export class RiverPath {
  readonly points: Vector3[]
  private readonly zs: Float64Array

  constructor() {
    const curve = new CatmullRomCurve3(RIVER_PATH.map(([x, z]) => new Vector3(x, 0, z)), false, 'centripetal')
    this.points = curve.getSpacedPoints(SAMPLES)
    this.zs = Float64Array.from(this.points, (point) => point.z)
  }

  widthAt(t: number) {
    const base = lerp(RIVER_WIDTH.upstream, RIVER_WIDTH.downstream, smoothstep(0.2, 0.9, t))
    return base * (0.85 + fbm(t * 18, 2.5, 2, 4) * 0.3)
  }

  levelAt(t: number) {
    return lerp(RIVER_LEVEL.upstream, RIVER_LEVEL.downstream, smoothstep(0, 0.82, t))
  }

  nearest(x: number, z: number): RiverSample {
    const { points, zs } = this
    // Recherche dichotomique du point de même profondeur, puis exploration locale
    let low = 0
    let high = zs.length - 1
    while (high - low > 1) {
      const middle = (low + high) >> 1
      if (zs[middle] < z) low = middle
      else high = middle
    }
    let best = Infinity
    let bestT = low / SAMPLES
    for (const direction of [-1, 1]) {
      for (let index = direction < 0 ? low : low + 1; index >= 1 && index < points.length; index += direction) {
        const a = points[index - 1]
        const b = points[index]
        if (Math.min(Math.abs(a.z - z), Math.abs(b.z - z)) > best) break
        const abx = b.x - a.x
        const abz = b.z - a.z
        const lengthSq = abx * abx + abz * abz || 1
        const u = Math.min(Math.max(((x - a.x) * abx + (z - a.z) * abz) / lengthSq, 0), 1)
        const dx = a.x + abx * u - x
        const dz = a.z + abz * u - z
        const distance = Math.sqrt(dx * dx + dz * dz)
        if (distance < best) {
          best = distance
          bestT = (index - 1 + u) / SAMPLES
        }
      }
    }
    return { distance: best, t: bestT }
  }
}
