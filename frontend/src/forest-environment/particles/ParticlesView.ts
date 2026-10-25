import {
  AdditiveBlending,
  BufferAttribute,
  BufferGeometry,
  Color,
  DoubleSide,
  Euler,
  Group,
  InstancedMesh,
  Matrix4,
  MeshLambertMaterial,
  NormalBlending,
  Points,
  Quaternion,
  ShaderMaterial,
  Vector3,
  type Blending,
} from 'three'
import type { ParticlesModel, PointCloud } from './ParticlesModel'

const POINT_VERTEX = /* glsl */ `
  attribute float aAlpha;
  uniform float uSize;
  uniform float uPixelRatio;
  varying float vAlpha;
  void main() {
    vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
    gl_Position = projectionMatrix * mvPosition;
    gl_PointSize = uSize * uPixelRatio * (60.0 / -mvPosition.z);
    vAlpha = aAlpha;
  }
`

const POINT_FRAGMENT = /* glsl */ `
  uniform vec3 uColor;
  uniform float uOpacity;
  uniform float uSoftness;
  varying float vAlpha;
  void main() {
    float distance = length(gl_PointCoord - 0.5);
    float disc = 1.0 - smoothstep(0.5 - uSoftness, 0.5, distance);
    gl_FragColor = vec4(uColor, disc * vAlpha * uOpacity);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
  }
`

interface CloudLayer {
  cloud: PointCloud
  points: Points<BufferGeometry, ShaderMaterial>
  opacity: number
}

/** Feuille : un losange légèrement plié, vu des deux côtés. */
function leafGeometry() {
  const geometry = new BufferGeometry()
  const positions = new Float32Array([
    0, 0, -0.07, 0.045, 0.006, 0, 0, 0, 0.08,
    0, 0, -0.07, 0, 0, 0.08, -0.045, 0.006, 0,
  ])
  geometry.setAttribute('position', new BufferAttribute(positions, 3))
  geometry.computeVertexNormals()
  return geometry
}

export class ParticlesView {
  readonly root = new Group()
  private readonly layers: CloudLayer[] = []
  private readonly leaves: InstancedMesh<BufferGeometry, MeshLambertMaterial>
  private readonly matrix = new Matrix4()
  private readonly quaternion = new Quaternion()
  private readonly euler = new Euler()
  private readonly scale = new Vector3()

  constructor(model: ParticlesModel, pixelRatio: number) {
    this.layers.push(
      this.cloud(model.snow, '#f4f7fb', 1.1, 0.45, NormalBlending, 0.9, pixelRatio),
      this.cloud(model.fireflies, '#d8f27a', 1.5, 0.5, AdditiveBlending, 1, pixelRatio),
      this.cloud(model.motes, '#fff1cf', 0.55, 0.5, AdditiveBlending, 0.55, pixelRatio),
    )

    this.leaves = new InstancedMesh(leafGeometry(), new MeshLambertMaterial({ side: DoubleSide }), model.leaves.length)
    this.leaves.castShadow = false
    this.leaves.frustumCulled = false
    model.leaves.forEach((_, index) => this.leaves.setColorAt(index, new Color('#c46a2a')))
    this.root.add(this.leaves)
  }

  sync(model: ParticlesModel) {
    for (const layer of this.layers) {
      const visible = layer.cloud.intensity > 0.01
      layer.points.visible = visible
      if (!visible) continue
      const geometry = layer.points.geometry
      geometry.getAttribute('position').needsUpdate = true
      geometry.getAttribute('aAlpha').needsUpdate = true
      layer.points.material.uniforms.uOpacity.value = layer.cloud.intensity * layer.opacity
    }

    let visibleLeaves = 0
    model.leaves.forEach((leaf, index) => {
      let size = 0
      if (leaf.active) {
        // Au sol, la feuille rapetisse doucement avant de disparaître
        size = leaf.grounded > 0 ? Math.max(1 - Math.max(leaf.grounded - 6, 0) / 3, 0) : 1
        visibleLeaves += 1
      }
      this.euler.set(leaf.rotation.x, leaf.rotation.y, leaf.rotation.z)
      this.quaternion.setFromEuler(this.euler)
      this.scale.setScalar(Math.max(size * 1.6, 0.0001))
      this.matrix.compose(leaf.position, this.quaternion, this.scale)
      this.leaves.setMatrixAt(index, this.matrix)
      if (leaf.active) this.leaves.setColorAt(index, leaf.color)
    })
    this.leaves.visible = visibleLeaves > 0
    this.leaves.instanceMatrix.needsUpdate = true
    if (this.leaves.instanceColor) this.leaves.instanceColor.needsUpdate = true
  }

  dispose() {
    this.layers.forEach(({ points }) => {
      points.geometry.dispose()
      points.material.dispose()
    })
    this.leaves.geometry.dispose()
    this.leaves.material.dispose()
    this.leaves.dispose()
  }

  private cloud(cloud: PointCloud, color: string, size: number, softness: number, blending: Blending, opacity: number, pixelRatio: number): CloudLayer {
    const geometry = new BufferGeometry()
    geometry.setAttribute('position', new BufferAttribute(cloud.positions, 3))
    geometry.setAttribute('aAlpha', new BufferAttribute(cloud.alphas, 1))
    const material = new ShaderMaterial({
      vertexShader: POINT_VERTEX,
      fragmentShader: POINT_FRAGMENT,
      transparent: true,
      depthWrite: false,
      blending,
      uniforms: {
        uColor: { value: new Color(color) },
        uSize: { value: size },
        uPixelRatio: { value: pixelRatio },
        uOpacity: { value: 0 },
        uSoftness: { value: softness },
      },
    })
    const points = new Points(geometry, material)
    points.frustumCulled = false
    this.root.add(points)
    return { cloud, points, opacity }
  }
}
