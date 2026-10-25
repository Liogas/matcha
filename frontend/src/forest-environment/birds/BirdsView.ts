import {
  BufferAttribute,
  BufferGeometry,
  DoubleSide,
  InstancedMesh,
  Matrix4,
  MeshBasicMaterial,
  Quaternion,
  Vector3,
} from 'three'
import type { BirdsModel } from './BirdsModel'

/** Un oiseau lointain : deux ailes en chevron. Inverser l'échelle verticale le fait battre des ailes. */
function chevronGeometry() {
  const geometry = new BufferGeometry()
  geometry.setAttribute('position', new BufferAttribute(new Float32Array([
    0, 0, 0.12, -0.75, 0.32, -0.05, 0, 0.04, -0.12,
    0, 0, 0.12, 0, 0.04, -0.12, 0.75, 0.32, -0.05,
  ]), 3))
  return geometry
}

export class BirdsView {
  readonly root: InstancedMesh<BufferGeometry, MeshBasicMaterial>
  private readonly matrix = new Matrix4()
  private readonly position = new Vector3()
  private readonly rotation = new Quaternion()
  private readonly scale = new Vector3()

  constructor(model: BirdsModel) {
    this.root = new InstancedMesh(chevronGeometry(), new MeshBasicMaterial({ side: DoubleSide, fog: false }), model.birds.length)
    this.root.frustumCulled = false
  }

  sync(model: BirdsModel) {
    let active = 0
    model.birds.forEach((bird, index) => {
      const flap = bird.glide > 0 ? 0.35 : Math.sin(bird.flap)
      this.position.set(bird.x, bird.y, bird.z)
      this.scale.set(bird.active ? 1.6 : 0.0001, flap * 1.6, 1.6)
      this.rotation.set(0, bird.direction > 0 ? 0.70710678 : -0.70710678, 0, 0.70710678)
      this.root.setMatrixAt(index, this.matrix.compose(this.position, this.rotation, this.scale))
      if (bird.active) active += 1
    })
    this.root.visible = active > 0
    this.root.instanceMatrix.needsUpdate = true
    this.root.material.color.copy(model.color)
  }

  dispose() {
    this.root.geometry.dispose()
    this.root.material.dispose()
    this.root.dispose()
  }
}
