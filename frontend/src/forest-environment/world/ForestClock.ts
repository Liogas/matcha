/**
 * Source du temps du décor. Par défaut : l'heure réelle du navigateur.
 * Pour essayer une autre heure ou une autre saison, ajouter à l'URL :
 *   ?foret-date=2027-01-12&foret-heure=22:30&foret-vitesse=600
 * (`foret-vitesse` : nombre de secondes du décor par seconde réelle).
 */
export class ForestClock {
  private readonly origin: number
  private readonly startedAt = Date.now()
  private readonly speed: number

  constructor(search = typeof window === 'undefined' ? '' : window.location.search) {
    const params = new URLSearchParams(search)
    const date = params.get('foret-date')
    const time = params.get('foret-heure')
    const speed = Number(params.get('foret-vitesse'))
    this.speed = Number.isFinite(speed) && speed > 0 ? speed : 1

    const origin = new Date()
    if (date && /^\d{4}-\d{2}-\d{2}$/.test(date)) {
      const [year, month, dayOfMonth] = date.split('-').map(Number)
      origin.setFullYear(year, month - 1, dayOfMonth)
    }
    if (time && /^\d{1,2}:\d{2}$/.test(time)) {
      const [hours, minutes] = time.split(':').map(Number)
      origin.setHours(hours, minutes, 0, 0)
    }
    this.origin = origin.getTime()
  }

  get isSimulated() {
    return this.speed !== 1 || Math.abs(this.origin - this.startedAt) > 60_000
  }

  now() {
    return new Date(this.origin + (Date.now() - this.startedAt) * this.speed)
  }
}
