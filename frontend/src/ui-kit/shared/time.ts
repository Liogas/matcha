/** Accepte une Date ou une chaîne ISO (avec ou sans « T ») venue d'une API. */
export function toDate(value: Date | string) {
  return value instanceof Date ? value : new Date(value.replace(' ', 'T'))
}

/** Heure seule aujourd'hui, date et heure sinon. */
export function defaultFormatTime(date: Date, locale?: string) {
  if (Number.isNaN(date.getTime())) return ''
  const sameDay = date.toDateString() === new Date().toDateString()
  return sameDay
    ? date.toLocaleTimeString(locale, { hour: '2-digit', minute: '2-digit' })
    : date.toLocaleDateString(locale, { day: 'numeric', month: 'short', hour: '2-digit', minute: '2-digit' })
}

/** Intitulé d'un séparateur de jour : « Aujourd'hui », « Hier » ou la date. */
export function defaultFormatDay(date: Date, labels: { today: string; yesterday: string }, locale?: string) {
  const today = new Date()
  const yesterday = new Date(today)
  yesterday.setDate(today.getDate() - 1)
  if (date.toDateString() === today.toDateString()) return labels.today
  if (date.toDateString() === yesterday.toDateString()) return labels.yesterday
  return date.toLocaleDateString(locale, { weekday: 'long', day: 'numeric', month: 'long' })
}
