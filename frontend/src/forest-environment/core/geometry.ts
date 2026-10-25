import {
  BufferAttribute,
  BufferGeometry,
  Color,
  CylinderGeometry,
  Quaternion,
  Vector3,
} from 'three'
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js'
import { hash01 } from './random'

const UP = new Vector3(0, 1, 0)

/**
 * Déplace chaque sommet selon un hachage de sa position :
 * deux sommets confondus bougent ensemble, la surface reste fermée.
 */
export function jitter(geometry: BufferGeometry, amount: number, seed = 0) {
  const position = geometry.getAttribute('position') as BufferAttribute
  for (let index = 0; index < position.count; index += 1) {
    const x = position.getX(index)
    const y = position.getY(index)
    const z = position.getZ(index)
    const key = Math.round(x * 1000) * 73856093 ^ Math.round(y * 1000) * 19349663 ^ Math.round(z * 1000) * 83492791
    position.setXYZ(
      index,
      x + (hash01(key + seed) - 0.5) * amount,
      y + (hash01(key + seed + 1) - 0.5) * amount,
      z + (hash01(key + seed + 2) - 0.5) * amount,
    )
  }
  position.needsUpdate = true
  return geometry
}

/** Facettes nettes : chaque triangle a sa propre normale (rendu low-poly). */
export function facet(geometry: BufferGeometry) {
  const flat = geometry.index ? geometry.toNonIndexed() : geometry
  if (flat !== geometry) geometry.dispose()
  flat.deleteAttribute('uv')
  flat.computeVertexNormals()
  return flat
}

/** Ajoute (ou remplace) une couleur de sommet uniforme. */
export function paint(geometry: BufferGeometry, color: Color | string, variation = 0, seed = 0) {
  const base = color instanceof Color ? color : new Color(color)
  const count = geometry.getAttribute('position').count
  const colors = new Float32Array(count * 3)
  const tint = new Color()
  // Une variation par triangle donne l'aspect « taillé » sans texture
  for (let index = 0; index < count; index += 3) {
    const shade = 1 + (hash01(index + seed) - 0.5) * variation
    tint.copy(base).multiplyScalar(shade)
    for (let corner = 0; corner < 3 && index + corner < count; corner += 1)
      tint.toArray(colors, (index + corner) * 3)
  }
  geometry.setAttribute('color', new BufferAttribute(colors, 3))
  return geometry
}

export function merge(geometries: BufferGeometry[]) {
  const merged = mergeGeometries(geometries, false)
  geometries.forEach((geometry) => geometry.dispose())
  if (!merged)
    throw new Error('forest-environment : géométries incompatibles à la fusion.')
  return merged
}

/** Cylindre effilé tendu entre deux points (branches, racines, pattes). */
export function cylinderBetween(start: Vector3, end: Vector3, radiusStart: number, radiusEnd: number, radialSegments = 6) {
  const direction = new Vector3().subVectors(end, start)
  const length = direction.length()
  const geometry = new CylinderGeometry(radiusEnd, radiusStart, length, radialSegments, 1, true)
  geometry.applyQuaternion(new Quaternion().setFromUnitVectors(UP, direction.normalize()))
  geometry.translate((start.x + end.x) / 2, (start.y + end.y) / 2, (start.z + end.z) / 2)
  return geometry
}
