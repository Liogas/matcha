import { Color, Matrix4, Vector3 } from 'three'
import { OBSERVER } from '../core/composition'
import { DEG, colorKeys, sampleColor, smoothstep } from '../core/math'
import type { ForestController, FrameContext } from '../core/types'
import type { WorldModel } from '../world/WorldModel'
import type { SkyModel } from './SkyModel'
import type { SkyView } from './SkyView'

/** Couleurs du ciel selon la hauteur du soleil, en degrés. */
const ZENITH = colorKeys([
  [-18, '#060a1a'], [-10, '#0c1330'], [-5, '#1c2955'], [0, '#34497f'], [6, '#5379b3'], [20, '#4a82c4'], [60, '#3b73ba'],
])
const HORIZON = colorKeys([
  [-18, '#0d1429'], [-10, '#18223f'], [-5, '#47466e'], [-1.5, '#b9706b'], [2, '#eda06c'], [8, '#efc89c'], [20, '#c6d9e8'], [60, '#b4cfe6'],
])
const SUN_COLOR = colorKeys([[-2, '#ff6a2a'], [4, '#ffb36b'], [15, '#ffe3b8'], [40, '#fff6e6']])

const MOONLIT_ZENITH = new Color('#1b2850')
const OVERCAST = new Color('#9aa3ab')
const WINTER_TINT = new Color('#c7d3dd')

function luminance(color: Color) {
  return color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722
}
const OVERCAST_LUMINANCE = luminance(OVERCAST)

/** Axe du pôle céleste nord : vers le nord (+z), relevé de la latitude. */
const POLE_AXIS = new Vector3(0, Math.sin(OBSERVER.latitude * DEG), Math.cos(OBSERVER.latitude * DEG)).normalize()

export class SkyController implements ForestController {
  private readonly rotation = new Matrix4()
  private readonly grey = new Color()
  private readonly tint = new Color()

  private readonly model: SkyModel
  private readonly view: SkyView
  private readonly world: WorldModel

  constructor(
    model: SkyModel,
    view: SkyView,
    world: WorldModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
  }

  update(frame: FrameContext) {
    const { model, world } = this
    const { cloudiness, season, snowCover } = world.phenology
    const altitude = world.sunAltitude

    sampleColor(ZENITH, altitude, model.zenith)
    sampleColor(HORIZON, altitude, model.horizon)
    sampleColor(SUN_COLOR, altitude, model.sunColor)

    // Clair de lune : la nuit est moins noire quand la lune est haute et pleine
    const moonUp = smoothstep(-2, 10, world.moonAltitude)
    const moonlight = moonUp * world.moonIllumination * (1 - world.daylight)
    model.zenith.lerp(MOONLIT_ZENITH, moonlight * 0.45)

    // Ciel couvert : couleurs plus grises, sans changer la luminosité
    const overcast = Math.max(cloudiness - 0.35, 0) * 0.9
    this.desaturate(model.zenith, overcast)
    this.desaturate(model.horizon, overcast * 0.8)
    if (season === 'winter' || snowCover > 0.2)
      model.horizon.lerp(this.tint.copy(WINTER_TINT).multiplyScalar(0.15 + world.daylight * 0.85), 0.25 * world.daylight)

    model.glow.copy(model.sunColor)
    model.glowStrength = world.twilight * (1 - overcast * 0.6)
    model.sunVisibility = smoothstep(-1.5, 1, altitude) * (1 - overcast * 0.85)
    model.moonVisibility = smoothstep(-3, 2, world.moonAltitude) * (1 - overcast * 0.7)
    model.starVisibility = (1 - smoothstep(-14, -5, altitude)) * (1 - cloudiness * 0.75) * (1 - moonlight * 0.35)
    model.sunDirection.copy(world.sunDirection)
    model.moonDirection.copy(world.moonDirection)

    // La voûte tourne autour du pôle au rythme du temps sidéral
    this.rotation.makeRotationAxis(POLE_AXIS, -world.siderealAngle)
    model.starRotation.setFromMatrix4(this.rotation)

    // Brume des lointains : un horizon un peu plus sombre et plus neutre
    model.haze.copy(model.horizon).lerp(model.zenith, 0.25)

    this.view.sync(model, world, frame)
  }

  dispose() {
    this.view.dispose()
  }

  private desaturate(color: Color, amount: number) {
    if (amount <= 0) return
    this.grey.copy(OVERCAST).multiplyScalar(luminance(color) / OVERCAST_LUMINANCE)
    color.lerp(this.grey, Math.min(amount, 1))
  }
}
