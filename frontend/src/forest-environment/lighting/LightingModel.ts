import { Color, Vector3 } from 'three'

/** L'éclairage de la scène : un astre principal, la lumière du ciel, la brume. */
export class LightingModel {
  /** Direction vers l'astre qui éclaire (soleil le jour, lune la nuit). */
  readonly keyDirection = new Vector3(0, 1, 0)
  readonly keyColor = new Color('#fff4e0')
  keyIntensity = 2.4

  readonly skyColor = new Color('#a8c4e0')
  readonly groundColor = new Color('#5b5136')
  ambientIntensity = 1

  readonly fogColor = new Color('#b8c8d4')
  fogNear = 30
  fogFar = 190
  exposure = 1
}
