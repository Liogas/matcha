import { Color } from 'three'

export interface Bird {
  x: number
  y: number
  z: number
  speed: number
  /** +1 vers la droite, -1 vers la gauche. */
  direction: number
  flap: number
  flapRate: number
  /** Glisse sans battre des ailes pendant un moment. */
  glide: number
  active: boolean
}

/** Des oiseaux qui traversent parfois le ciel, en petite formation lâche. */
export class BirdsModel {
  readonly birds: Bird[] = []
  readonly color = new Color('#2b2b30')
  /** Secondes avant le prochain vol. */
  nextFlock = 8

  constructor(capacity: number) {
    for (let index = 0; index < capacity; index += 1)
      this.birds.push({ x: 0, y: 0, z: 0, speed: 0, direction: 1, flap: 0, flapRate: 0, glide: 0, active: false })
  }
}
