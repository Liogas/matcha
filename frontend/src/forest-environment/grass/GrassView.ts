import {
  BufferAttribute,
  BufferGeometry,
  Color,
  DoubleSide,
  Group,
  InstancedMesh,
  Matrix4,
  MeshLambertMaterial,
  OctahedronGeometry,
  Quaternion,
  Vector3,
} from 'three'
import { smoothstep } from '../core/math'
import { applyEffects, type SharedUniforms } from '../core/materialEffects'
import type { GrassModel } from './GrassModel'

const Y_AXIS = new Vector3(0, 1, 0)

/**
 * Un brin : trois segments qui s'affinent et se courbent.
 * Hauteur locale de 0 à 1 (le shader de vent s'en sert) ; normales vers le ciel
 * pour un éclairage doux, sans facettes.
 */
function bladeGeometry() {
  const levels = [0, 0.35, 0.7, 1]
  const positions: number[] = []
  const colors: number[] = []
  levels.forEach((y) => {
    const half = 0.045 * (1 - y * 0.92)
    const bend = y * y * 0.18
    const shade = 0.72 + y * 0.45
    positions.push(-half, y, bend, half, y, bend)
    colors.push(shade, shade, shade, shade, shade, shade)
  })
  const indices: number[] = []
  for (let level = 0; level < levels.length - 1; level += 1) {
    const a = level * 2
    indices.push(a, a + 1, a + 2, a + 2, a + 1, a + 3)
  }
  const geometry = new BufferGeometry()
  geometry.setAttribute('position', new BufferAttribute(new Float32Array(positions), 3))
  geometry.setAttribute('color', new BufferAttribute(new Float32Array(colors), 3))
  geometry.setAttribute('normal', new BufferAttribute(new Float32Array(positions.length).map((_, index) => (index % 3 === 1 ? 1 : 0)), 3))
  geometry.setIndex(indices)
  return geometry
}

export class GrassView {
  readonly root = new Group()
  private readonly blades: InstancedMesh<BufferGeometry, MeshLambertMaterial>
  private readonly flowers: InstancedMesh<OctahedronGeometry, MeshLambertMaterial>

  constructor(model: GrassModel, shared: SharedUniforms) {
    const bladeMaterial = applyEffects(
      new MeshLambertMaterial({ vertexColors: true, side: DoubleSide }),
      shared,
      { wind: 'grass', windScale: 0.32, flattenUnderSnow: true, keepNormals: true },
    )
    this.blades = new InstancedMesh(bladeGeometry(), bladeMaterial, model.blades.length)
    const matrix = new Matrix4()
    const rotation = new Quaternion()
    const scale = new Vector3()
    const position = new Vector3()
    const shade = new Color()
    model.blades.forEach((blade, index) => {
      rotation.setFromAxisAngle(Y_AXIS, blade.rotation)
      matrix.compose(position.set(blade.x, blade.y, blade.z), rotation, scale.set(blade.width, blade.height, blade.width))
      this.blades.setMatrixAt(index, matrix)
      this.blades.setColorAt(index, shade.setScalar(blade.shade))
    })
    this.blades.receiveShadow = true

    const flowerMaterial = applyEffects(new MeshLambertMaterial({ flatShading: true }), shared, { wind: 'grass', windScale: 0.05 })
    const bloom = new OctahedronGeometry(1, 0)
    bloom.scale(1, 0.45, 1)
    this.flowers = new InstancedMesh(bloom, flowerMaterial, model.flowers.length)
    model.flowers.forEach((flower, index) => this.flowers.setColorAt(index, flower.color))

    this.root.add(this.blades, this.flowers)
  }

  sync(model: GrassModel, bloomChanged: boolean) {
    this.blades.material.color.copy(model.color)
    if (!bloomChanged) return
    const matrix = new Matrix4()
    const position = new Vector3()
    const rotation = new Quaternion()
    const scale = new Vector3()
    model.flowers.forEach((flower, index) => {
      const open = smoothstep(flower.threshold, flower.threshold + 0.1, model.bloom)
      scale.setScalar(Math.max(flower.size * open, 0.0001))
      this.flowers.setMatrixAt(index, matrix.compose(position.set(flower.x, flower.y, flower.z), rotation, scale))
    })
    this.flowers.instanceMatrix.needsUpdate = true
    this.flowers.visible = model.bloom > 0.01
  }

  dispose() {
    this.blades.geometry.dispose()
    this.blades.material.dispose()
    this.blades.dispose()
    this.flowers.geometry.dispose()
    this.flowers.material.dispose()
    this.flowers.dispose()
  }
}
