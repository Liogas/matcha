import {
  BufferGeometry,
  Color,
  Group,
  IcosahedronGeometry,
  InstancedMesh,
  Matrix4,
  Mesh,
  MeshLambertMaterial,
  Quaternion,
  Vector3,
} from 'three'
import { cylinderBetween, facet, jitter, merge, paint } from '../core/geometry'
import { applyEffects, type SharedUniforms } from '../core/materialEffects'
import { hash01 } from '../core/random'
import type { BranchSegment, TreeModel } from './TreeModel'

const BARK = new Color('#6e5a47')
const BARK_DARK = new Color('#56463a')
const MOSS = new Color('#55623a')
const Y_AXIS = new Vector3(0, 1, 0)

/** Écorce : un cylindre par segment, plus foncé et moussu près du sol. */
function barkGeometry(segments: BranchSegment[], seed: number) {
  const pieces = segments.map((segment, index) => {
    const radial = segment.radiusStart > 0.3 ? 8 : segment.radiusStart > 0.08 ? 6 : 4
    const geometry = facet(cylinderBetween(segment.start, segment.end, segment.radiusStart, segment.radiusEnd, radial))
    const color = BARK.clone().lerp(BARK_DARK, hash01(index + seed) * 0.6)
    if (segment.start.y < 2.2) color.lerp(MOSS, 0.35)
    return paint(geometry, color, 0.18, index + seed)
  })
  // Nœuds aux jonctions épaisses : pas de fente visible entre deux segments
  segments.forEach((segment, index) => {
    if (segment.radiusEnd < 0.16) return
    const knot = facet(new IcosahedronGeometry(segment.radiusEnd * 1.04, 0))
    knot.translate(segment.end.x, segment.end.y, segment.end.z)
    pieces.push(paint(knot, BARK, 0.15, index + seed + 500))
  })
  return merge(pieces)
}

export class TreeView {
  readonly root = new Group()
  private readonly bark: Mesh<BufferGeometry, MeshLambertMaterial>
  private readonly foliage: InstancedMesh<BufferGeometry, MeshLambertMaterial>
  private readonly matrix = new Matrix4()
  private readonly rotation = new Quaternion()
  private readonly scale = new Vector3()

  constructor(model: TreeModel, shared: SharedUniforms) {
    const barkMaterial = applyEffects(new MeshLambertMaterial({ vertexColors: true, flatShading: true }), shared, { snow: true, snowSlope: 0.5 })
    this.bark = new Mesh(barkGeometry([...model.branches, ...model.roots], 3), barkMaterial)
    this.bark.castShadow = true
    this.bark.receiveShadow = true

    const leaf = facet(jitter(new IcosahedronGeometry(1, 1), 0.35, 9))
    const foliageMaterial = applyEffects(new MeshLambertMaterial({ flatShading: true }), shared, { snow: true, snowSlope: 0.7 })
    this.foliage = new InstancedMesh(leaf, foliageMaterial, model.clusters.length)
    this.foliage.castShadow = true
    this.foliage.receiveShadow = true
    model.clusters.forEach((cluster, index) => {
      cluster.color.set('#557f36')
      this.foliage.setColorAt(index, cluster.color)
    })

    this.root.add(this.bark, this.foliage)
  }

  sync(model: TreeModel, recolor: boolean) {
    model.clusters.forEach((cluster, index) => {
      const size = Math.max(cluster.size * cluster.scale, 0.0001)
      this.rotation.setFromAxisAngle(Y_AXIS, cluster.phase)
      this.scale.set(size * 1.15, size * 0.85, size)
      this.matrix.compose(cluster.position, this.rotation, this.scale)
      this.foliage.setMatrixAt(index, this.matrix)
      if (recolor) this.foliage.setColorAt(index, cluster.color)
    })
    this.foliage.instanceMatrix.needsUpdate = true
    if (recolor && this.foliage.instanceColor) this.foliage.instanceColor.needsUpdate = true
    // Les bouquets bougent : la sphère englobante doit suivre pour le tri et le picking
    this.foliage.computeBoundingSphere()
  }

  dispose() {
    this.bark.geometry.dispose()
    this.bark.material.dispose()
    this.foliage.geometry.dispose()
    this.foliage.material.dispose()
    this.foliage.dispose()
  }
}
