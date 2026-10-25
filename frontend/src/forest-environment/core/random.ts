/**
 * Générateur pseudo-aléatoire déterministe (mulberry32) :
 * le décor est identique à chaque visite, seul le temps le fait évoluer.
 */
export class Random {
  private state: number

  constructor(seed: number) {
    this.state = seed >>> 0
  }

  next() {
    this.state = (this.state + 0x6d2b79f5) >>> 0
    let t = this.state
    t = Math.imul(t ^ (t >>> 15), t | 1)
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61)
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296
  }

  range(min: number, max: number) {
    return min + (max - min) * this.next()
  }

  pick<T>(items: readonly T[]) {
    return items[Math.floor(this.next() * items.length)]
  }

  chance(probability: number) {
    return this.next() < probability
  }
}

/** Hachage entier → [0, 1[, utile pour un tirage stable par jour ou par position. */
export function hash01(value: number) {
  let h = Math.imul(value | 0, 0x27d4eb2d) ^ 0x165667b1
  h = Math.imul(h ^ (h >>> 15), 0x85ebca6b)
  h = Math.imul(h ^ (h >>> 13), 0xc2b2ae35)
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296
}
