import {
  BufferGeometry,
  ConeGeometry,
  Group,
  IcosahedronGeometry,
  InstancedMesh,
  Matrix4,
  MeshLambertMaterial,
  Quaternion,
  Vector3,
} from 'three'
import { cylinderBetween, facet, jitter, merge, paint } from '../core/geometry'
import { applyEffects, type SharedUniforms } from '../core/materialEffects'
import type { WoodsModel, WoodTree } from './WoodsModel'

const Y_AXIS = new Vector3(0, 1, 0)
/** Hauteur du centre du houppier d'un feuillu à l'échelle 1. */
const CROWN_HEIGHT = 4.5

/** Sapin : un fût et quatre étages de branches. */
function pineGeometry() {
  const trunk = paint(facet(cylinderBetween(new Vector3(0, 0, 0), new Vector3(0, 1.8, 0), 0.2, 0.13, 5)), '#4a3a2b', 0.1)
  const tiers = [
    [1.9, 3.2, 2.9],
    [1.5, 2.8, 4.5],
    [1.05, 2.4, 6.0],
    [0.6, 1.9, 7.2],
  ].map(([radius, height, y], index) => {
    const cone = jitter(new ConeGeometry(radius, height, 7, 1), 0.25, index * 7)
    cone.translate(0, y, 0)
    return paint(facet(cone), index % 2 ? '#2e4a30' : '#2a432b', 0.22, index * 31)
  })
  return merge([trunk, ...tiers])
}

/** Feuillu nu : visible en hiver, sous le houppier le reste de l'année. */
function broadleafFrame() {
  const base = new Vector3(0, 0, 0)
  const fork = new Vector3(0.1, 2.6, 0)
  const pieces = [paint(facet(cylinderBetween(base, fork, 0.22, 0.15, 6)), '#4f4031', 0.12)]
  const tips = [
    new Vector3(1.4, 4.8, 0.3), new Vector3(-1.1, 5, 0.7), new Vector3(0.2, 5.3, -1.2), new Vector3(-0.4, 5.6, 0.1),
  ]
  tips.forEach((tip, index) => {
    pieces.push(paint(facet(cylinderBetween(fork, tip, 0.12, 0.03, 4)), '#4f4031', 0.12, index + 10))
  })
  return merge(pieces)
}

function crownGeometry() {
  const crown = jitter(new IcosahedronGeometry(1, 1), 0.3, 21)
  crown.scale(2.3, 1.9, 2.3)
  return facet(crown)
}

export class WoodsView {
  readonly root = new Group()
  private readonly meshes: InstancedMesh<BufferGeometry, MeshLambertMaterial>[] = []
  private readonly crowns: InstancedMesh<BufferGeometry, MeshLambertMaterial>
  private readonly broadleaves: WoodTree[]

  constructor(model: WoodsModel, shared: SharedUniforms) {
    const barkMaterial = applyEffects(new MeshLambertMaterial({ vertexColors: true, flatShading: true }), shared, { wind: 'canopy', snow: true, snowSlope: 0.32 })
    // Le houppier est centré sur l'origine : le vent doit y être amplifié pour se voir
    const crownMaterial = applyEffects(new MeshLambertMaterial({ flatShading: true }), shared, { wind: 'canopy', windScale: 6, snow: true, snowSlope: 0.7 })

    const pines = this.instanced(pineGeometry(), barkMaterial, model.pines)
    this.broadleaves = model.broadleaves
    const frames = this.instanced(broadleafFrame(), barkMaterial, this.broadleaves)
    this.crowns = this.instanced(crownGeometry(), crownMaterial, this.broadleaves)
    this.meshes.push(pines, frames, this.crowns)
    this.root.add(pines, frames, this.crowns)
    this.sync(model)
  }

  sync(model: WoodsModel) {
    const matrix = new Matrix4()
    const rotation = new Quaternion()
    const scale = new Vector3()
    const position = new Vector3()
    const pines = this.meshes[0]
    model.pines.forEach((tree, index) => pines.setColorAt(index, tree.color))
    this.broadleaves.forEach((tree, index) => {
      // Le houppier se clairsème autour de son centre, il ne s'affaisse pas
      const [wide, tall] = tree.stretch
      position.set(tree.x, tree.y + CROWN_HEIGHT * tree.scale * (0.85 + tall * 0.15), tree.z)
      rotation.setFromAxisAngle(Y_AXIS, tree.rotation)
      const size = Math.max(tree.scale * tree.crownScale, 0.0001)
      scale.set(size * wide, size * tall, size * wide)
      this.crowns.setMatrixAt(index, matrix.compose(position, rotation, scale))
      this.crowns.setColorAt(index, tree.color)
    })
    this.crowns.instanceMatrix.needsUpdate = true
    for (const mesh of [pines, this.crowns])
      if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true
  }

  dispose() {
    this.meshes.forEach((mesh) => {
      mesh.geometry.dispose()
      mesh.material.dispose()
      mesh.dispose()
    })
  }

  private instanced(geometry: BufferGeometry, material: MeshLambertMaterial, trees: WoodTree[]) {
    const mesh = new InstancedMesh(geometry, material, Math.max(trees.length, 1))
    mesh.count = trees.length
    const matrix = new Matrix4()
    const rotation = new Quaternion()
    trees.forEach((tree, index) => {
      rotation.setFromAxisAngle(Y_AXIS, tree.rotation)
      matrix.compose(new Vector3(tree.x, tree.y, tree.z), rotation, new Vector3().setScalar(tree.scale))
      mesh.setMatrixAt(index, matrix)
      mesh.setColorAt(index, tree.color.setScalar(tree.shade))
    })
    mesh.castShadow = true
    mesh.receiveShadow = true
    return mesh
  }
}
