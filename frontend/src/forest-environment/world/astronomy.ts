/**
 * Positions approchées du soleil et de la lune (formules de V. Agafonkin / suncalc,
 * d'après J. Meeus). La précision, de l'ordre du degré, suffit largement à un décor.
 */
const RAD = Math.PI / 180
const DAY_MS = 86_400_000
const J1970 = 2440588
const J2000 = 2451545
const OBLIQUITY = RAD * 23.4397

export interface SkyPosition {
  /** Hauteur au-dessus de l'horizon, en radians. */
  altitude: number
  /** Azimut compté depuis le nord, vers l'est, en radians. */
  azimuth: number
}

export interface MoonState extends SkyPosition {
  /** 0 = nouvelle lune, 0.25 = premier quartier, 0.5 = pleine lune, 0.75 = dernier quartier. */
  phase: number
  /** Fraction éclairée du disque, de 0 à 1. */
  illumination: number
}

function daysSinceJ2000(date: Date) {
  return date.getTime() / DAY_MS - 0.5 + J1970 - J2000
}

function rightAscension(longitude: number, latitude: number) {
  return Math.atan2(
    Math.sin(longitude) * Math.cos(OBLIQUITY) - Math.tan(latitude) * Math.sin(OBLIQUITY),
    Math.cos(longitude),
  )
}

function declination(longitude: number, latitude: number) {
  return Math.asin(
    Math.sin(latitude) * Math.cos(OBLIQUITY) + Math.cos(latitude) * Math.sin(OBLIQUITY) * Math.sin(longitude),
  )
}

function siderealTime(days: number, westLongitude: number) {
  return RAD * (280.16 + 360.9856235 * days) - westLongitude
}

function toHorizon(hourAngle: number, latitude: number, dec: number): SkyPosition {
  const altitude = Math.asin(
    Math.sin(latitude) * Math.sin(dec) + Math.cos(latitude) * Math.cos(dec) * Math.cos(hourAngle),
  )
  // suncalc compte l'azimut depuis le sud : on le ramène depuis le nord
  const fromSouth = Math.atan2(
    Math.sin(hourAngle),
    Math.cos(hourAngle) * Math.sin(latitude) - Math.tan(dec) * Math.cos(latitude),
  )
  return { altitude, azimuth: fromSouth + Math.PI }
}

function sunCoordinates(days: number) {
  const meanAnomaly = RAD * (357.5291 + 0.98560028 * days)
  const center = RAD * (1.9148 * Math.sin(meanAnomaly) + 0.02 * Math.sin(2 * meanAnomaly) + 0.0003 * Math.sin(3 * meanAnomaly))
  const eclipticLongitude = meanAnomaly + center + RAD * 102.9372 + Math.PI
  return {
    dec: declination(eclipticLongitude, 0),
    ra: rightAscension(eclipticLongitude, 0),
  }
}

function moonCoordinates(days: number) {
  const meanLongitude = RAD * (218.316 + 13.176396 * days)
  const meanAnomaly = RAD * (134.963 + 13.064993 * days)
  const meanDistance = RAD * (93.272 + 13.22935 * days)
  const longitude = meanLongitude + RAD * 6.289 * Math.sin(meanAnomaly)
  const latitude = RAD * 5.128 * Math.sin(meanDistance)
  return {
    ra: rightAscension(longitude, latitude),
    dec: declination(longitude, latitude),
    distance: 385001 - 20905 * Math.cos(meanAnomaly),
  }
}

export function sunPosition(date: Date, latitudeDeg: number, longitudeDeg: number): SkyPosition {
  const days = daysSinceJ2000(date)
  const latitude = RAD * latitudeDeg
  const sun = sunCoordinates(days)
  return toHorizon(siderealTime(days, RAD * -longitudeDeg) - sun.ra, latitude, sun.dec)
}

export function moonState(date: Date, latitudeDeg: number, longitudeDeg: number): MoonState {
  const days = daysSinceJ2000(date)
  const latitude = RAD * latitudeDeg
  const moon = moonCoordinates(days)
  const sun = sunCoordinates(days)
  const position = toHorizon(siderealTime(days, RAD * -longitudeDeg) - moon.ra, latitude, moon.dec)

  const sunDistance = 149598000
  const elongation = Math.acos(
    Math.sin(sun.dec) * Math.sin(moon.dec) + Math.cos(sun.dec) * Math.cos(moon.dec) * Math.cos(sun.ra - moon.ra),
  )
  const inclination = Math.atan2(sunDistance * Math.sin(elongation), moon.distance - sunDistance * Math.cos(elongation))
  const angle = Math.atan2(
    Math.cos(sun.dec) * Math.sin(sun.ra - moon.ra),
    Math.sin(sun.dec) * Math.cos(moon.dec) - Math.cos(sun.dec) * Math.sin(moon.dec) * Math.cos(sun.ra - moon.ra),
  )
  return {
    ...position,
    illumination: (1 + Math.cos(inclination)) / 2,
    phase: 0.5 + (0.5 * inclination * (angle < 0 ? -1 : 1)) / Math.PI,
  }
}

/** Temps sidéral local en radians : sert à faire tourner la voûte étoilée. */
export function localSiderealAngle(date: Date, longitudeDeg: number) {
  return siderealTime(daysSinceJ2000(date), RAD * -longitudeDeg)
}
