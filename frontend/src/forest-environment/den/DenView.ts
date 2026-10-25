import {
  BackSide,
  BufferAttribute,
  BufferGeometry,
  CircleGeometry,
  Color,
  CylinderGeometry,
  Group,
  IcosahedronGeometry,
  Mesh,
  MeshBasicMaterial,
  MeshLambertMaterial,
  Quaternion,
  SphereGeometry,
  Vector3,
} from 'three'
import { facet, jitter, merge, paint } from '../core/geometry'
import { applyEffects, type SharedUniforms } from '../core/materialEffects'
import type { DenModel } from './DenModel'

const STONE = new Color('#8b867b')
const STONE_DARK = new Color('#716d65')
const MOSS = new Color('#5a6b39')
const WOOD = new Color('#5a4633')
const UP = new Vector3(0, 1, 0)

/** Mousse sur les faces tournées vers le ciel. */
function addMoss(geometry: BufferGeometry, amount: number) {
  const normals = geometry.getAttribute('normal') as BufferAttribute
  const colors = geometry.getAttribute('color') as BufferAttribute
  const color = new Color()
  for (let index = 0; index < colors.count; index += 1) {
    const up = normals.getY(index)
    if (up < 0.45) continue
    color.fromBufferAttribute(colors, index).lerp(MOSS, Math.min((up - 0.45) * 2, 1) * amount)
    colors.setXYZ(index, color.r, color.g, color.b)
  }
  return geometry
}

export class DenView {
  readonly root = new Group()
  private readonly geometries: BufferGeometry[] = []
  private readonly materials: (MeshLambertMaterial | MeshBasicMaterial)[] = []

  constructor(model: DenModel, shared: SharedUniforms) {
    const stoneMaterial = applyEffects(new MeshLambertMaterial({ vertexColors: true, flatShading: true }), shared, { snow: true, snowSlope: 0.55 })
    this.materials.push(stoneMaterial)

    // Tous les blocs dans une seule géométrie : un seul appel de rendu
    const stones = model.boulders.map((boulder, index) => {
      let geometry = jitter(new IcosahedronGeometry(boulder.size, 1), boulder.size * 0.32, boulder.seed)
      geometry.scale(boulder.scale.x, boulder.scale.y, boulder.scale.z)
      geometry.rotateY(boulder.rotation)
      geometry.translate(boulder.position.x, boulder.position.y, boulder.position.z)
      geometry = paint(facet(geometry), index % 3 ? STONE : STONE_DARK, 0.2, boulder.seed)
      return addMoss(geometry, 0.75)
    })
    const rocks = new Mesh(merge(stones), stoneMaterial)
    rocks.castShadow = true
    rocks.receiveShadow = true

    // Entrée : un disque sombre et une cavité, pour qu'on devine la profondeur
    const darkness = new MeshBasicMaterial({ color: '#0d0a08' })
    const cavityMaterial = new MeshBasicMaterial({ color: '#16110d', side: BackSide })
    this.materials.push(darkness, cavityMaterial)
    const mouthGeometry = new CircleGeometry(0.62, 14, 0, Math.PI)
    const mouth = new Mesh(mouthGeometry, darkness)
    mouth.position.copy(model.entrance)
    mouth.rotation.y = model.facing
    mouth.scale.set(1.1, 1.25, 1)
    const cavityGeometry = new SphereGeometry(0.9, 12, 8, 0, Math.PI * 2, 0, Math.PI / 2)
    const cavity = new Mesh(cavityGeometry, cavityMaterial)
    cavity.position.copy(model.entrance).add(new Vector3(-Math.sin(model.facing) * 0.55, -0.05, -Math.cos(model.facing) * 0.55))
    cavity.scale.set(1, 1, 1.2)
    cavity.rotation.y = model.facing

    const woodMaterial = applyEffects(new MeshLambertMaterial({ vertexColors: true, flatShading: true }), shared, { snow: true, snowSlope: 0.5 })
    this.materials.push(woodMaterial)
    const logs = model.logs.map((log, index) => {
      const direction = new Vector3().subVectors(log.end, log.start)
      const geometry = new CylinderGeometry(log.radius, log.radius * 1.08, direction.length(), log.radius > 0.2 ? 8 : 5)
      geometry.applyQuaternion(new Quaternion().setFromUnitVectors(UP, direction.normalize()))
      geometry.translate((log.start.x + log.end.x) / 2, (log.start.y + log.end.y) / 2, (log.start.z + log.end.z) / 2)
      return addMoss(paint(facet(jitter(geometry, log.radius * 0.15, index)), WOOD, 0.2, index), 0.6)
    })
    const wood = new Mesh(merge(logs), woodMaterial)
    wood.castShadow = true
    wood.receiveShadow = true

    this.geometries.push(rocks.geometry, mouthGeometry, cavityGeometry, wood.geometry)
    this.root.add(rocks, mouth, cavity, wood)
  }

  dispose() {
    this.geometries.forEach((geometry) => geometry.dispose())
    this.materials.forEach((material) => material.dispose())
  }
}
