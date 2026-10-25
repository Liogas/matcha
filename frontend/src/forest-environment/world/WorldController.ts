import type { Vector3 } from 'three'
import { OBSERVER } from '../core/composition'
import { DEG, clamp, smoothstep } from '../core/math'
import { valueNoise } from '../core/noise'
import type { ForestController, FrameContext } from '../core/types'
import { localSiderealAngle, moonState, sunPosition, type SkyPosition } from './astronomy'
import { ForestClock } from './ForestClock'
import { moonName } from './labels'
import { computePhenology } from './phenology'
import type { DayPhase, ForestSummary, WorldModel } from './WorldModel'

/** Le calendrier n'évolue pas d'une image à l'autre : inutile de le recalculer en continu. */
const CALENDAR_REFRESH = 5

function toDirection({ altitude, azimuth }: SkyPosition, target: Vector3) {
  // Azimut 0 = nord = +z (derrière la caméra), 90° = est = -x
  return target.set(
    -Math.sin(azimuth) * Math.cos(altitude),
    Math.sin(altitude),
    Math.cos(azimuth) * Math.cos(altitude),
  ).normalize()
}

/**
 * Fait avancer le monde : position des astres à l'heure réelle,
 * calendrier des saisons, vent. Ne dépend pas de WebGL, il peut donc
 * aussi alimenter l'interface quand le décor 3D est indisponible.
 */
export class WorldController implements ForestController {
  private readonly listeners = new Set<() => void>()
  private sinceCalendar = Infinity
  private windTime = 0

  private readonly model: WorldModel
  private readonly clock: ForestClock

  constructor(
    model: WorldModel,
    clock: ForestClock = new ForestClock(),
  ) {
    this.model = model
    this.clock = clock
    this.tick(0)
  }

  update(frame: FrameContext) {
    this.tick(frame.delta)
  }

  /** Avance le monde ; `delta` en secondes d'animation (0 si le décor est figé). */
  tick(delta: number) {
    const { model } = this
    model.date = this.clock.now()

    const sun = sunPosition(model.date, OBSERVER.latitude, OBSERVER.longitude)
    const moon = moonState(model.date, OBSERVER.latitude, OBSERVER.longitude)
    const sunLater = sunPosition(new Date(model.date.getTime() + 600_000), OBSERVER.latitude, OBSERVER.longitude)

    toDirection(sun, model.sunDirection)
    toDirection(moon, model.moonDirection)
    model.sunAltitude = sun.altitude / DEG
    model.moonAltitude = moon.altitude / DEG
    model.moonPhase = moon.phase
    model.moonIllumination = moon.illumination
    model.siderealAngle = localSiderealAngle(model.date, OBSERVER.longitude)
    model.rising = sunLater.altitude > sun.altitude
    model.daylight = smoothstep(-7, 6, model.sunAltitude)
    model.twilight = 1 - smoothstep(0, 1, Math.abs(model.sunAltitude - 1) / 11)

    this.sinceCalendar += delta
    if (this.sinceCalendar >= CALENDAR_REFRESH || delta === 0) {
      computePhenology(model.date, model.phenology)
      this.sinceCalendar = 0
    }

    // Rafales lentes, un peu plus fortes en automne et en hiver
    this.windTime += delta
    const seasonal = model.phenology.season === 'autumn' || model.phenology.season === 'winter' ? 0.12 : 0
    model.wind = clamp(0.18 + seasonal + valueNoise(this.windTime * 0.05, 3.7) * 0.55)

    this.refreshSummary()
  }

  subscribe(listener: () => void) {
    this.listeners.add(listener)
    return () => {
      this.listeners.delete(listener)
    }
  }

  getSummary(): ForestSummary {
    return this.model.summary
  }

  dispose() {
    this.listeners.clear()
  }

  private refreshSummary() {
    const { model } = this
    const phase: DayPhase = model.sunAltitude > 8
      ? 'day'
      : model.sunAltitude < -8
        ? 'night'
        : model.rising ? 'dawn' : 'dusk'
    const next: ForestSummary = {
      phase,
      isNight: model.sunAltitude < -4,
      season: model.phenology.season,
      moonName: moonName(model.moonPhase),
      snowing: model.phenology.snowfall > 0,
    }
    const current = model.summary
    if (
      next.phase === current.phase &&
      next.isNight === current.isNight &&
      next.season === current.season &&
      next.moonName === current.moonName &&
      next.snowing === current.snowing
    )
      return
    // Nouvel objet : useSyncExternalStore détecte le changement par identité
    model.summary = next
    this.listeners.forEach((listener) => listener())
  }
}
