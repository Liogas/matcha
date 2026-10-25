import {
  BufferAttribute,
  BufferGeometry,
  Color,
  Group,
  Mesh,
  ShaderMaterial,
} from 'three'
import { fbm } from '../core/noise'
import { hash01 } from '../core/random'
import type { HorizonModel, RidgeLayer } from './HorizonModel'

const HALF_WIDTH = 720
const BOTTOM = -30

const VERTEX = /* glsl */ `
  varying float vHeight;
  void main() {
    vHeight = position.y;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`

const FRAGMENT = /* glsl */ `
  uniform vec3 uColor;
  uniform vec3 uHaze;
  uniform float uHazeAmount;
  uniform float uSnow;
  uniform float uSnowLine;
  uniform float uMist;
  uniform float uMistTop;
  varying float vHeight;
  void main() {
    vec3 color = uColor;
    float snow = smoothstep(uSnowLine, uSnowLine + 8.0, vHeight) * uSnow;
    color = mix(color, uColor * 0.4 + vec3(0.55, 0.58, 0.62) * (0.5 + 0.5 * length(uColor)), snow);
    color = mix(color, uHaze, uHazeAmount);
    // Brume des vallées : le pied de chaque crête se fond dans le ciel
    float valley = 1.0 - smoothstep(0.0, uMistTop, vHeight);
    color = mix(color, uHaze, valley * uMist);
    gl_FragColor = vec4(color, 1.0);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
  }
`

/** Ligne de crête : grandes ondulations, puis dentelure de cimes si la crête est boisée. */
function ridgeHeight(layer: RidgeLayer, x: number, seed: number) {
  const broad = fbm(x * layer.frequency, seed, 5, seed)
  let height = layer.baseHeight + Math.pow(broad, 1.6) * layer.amplitude * 1.8
  if (layer.treeTips > 0) {
    const slot = Math.floor(x / 1.7)
    const local = x / 1.7 - slot
    const tip = 1 - Math.abs(local - 0.5) * 2
    height += tip * layer.treeTips * (0.55 + hash01(slot + seed * 97) * 0.9)
  }
  return height
}

function buildRidge(layer: RidgeLayer, seed: number) {
  const step = layer.treeTips > 0 ? 0.85 : 4
  const columns = Math.ceil((HALF_WIDTH * 2) / step) + 1
  const positions = new Float32Array(columns * 2 * 3)
  const indices: number[] = []
  for (let column = 0; column < columns; column += 1) {
    const x = -HALF_WIDTH + column * step
    positions.set([x, ridgeHeight(layer, x, seed), 0, x, BOTTOM, 0], column * 6)
    if (column > 0) {
      const top = column * 2
      indices.push(top - 2, top - 1, top, top, top - 1, top + 1)
    }
  }
  const geometry = new BufferGeometry()
  geometry.setAttribute('position', new BufferAttribute(positions, 3))
  geometry.setIndex(indices)
  return geometry
}

export class HorizonView {
  readonly root = new Group()
  private readonly meshes: Mesh<BufferGeometry, ShaderMaterial>[] = []

  constructor(model: HorizonModel) {
    model.layers.forEach((layer, index) => {
      const material = new ShaderMaterial({
        vertexShader: VERTEX,
        fragmentShader: FRAGMENT,
        uniforms: {
          uColor: { value: new Color() },
          uHaze: { value: new Color() },
          uHazeAmount: { value: layer.haze },
          uSnow: { value: 0 },
          uSnowLine: { value: 70 },
          uMist: { value: 0 },
          uMistTop: { value: layer.baseHeight * 0.9 },
        },
      })
      const mesh = new Mesh(buildRidge(layer, 11 + index * 23), material)
      mesh.position.z = layer.z
      // Du plus loin au plus proche, juste après le ciel et les nuages
      mesh.renderOrder = -7 + index * 0.1
      this.meshes.push(mesh)
      this.root.add(mesh)
    })
  }

  sync(model: HorizonModel) {
    model.layers.forEach((layer, index) => {
      const uniforms = this.meshes[index].material.uniforms
      uniforms.uColor.value.copy(layer.color)
      uniforms.uHaze.value.copy(model.haze)
      // Seules les montagnes du fond portent de la neige durable
      uniforms.uSnow.value = index === model.layers.length - 1 ? model.snow : 0
      uniforms.uSnowLine.value = model.snowLine
      uniforms.uMist.value = model.mist
    })
  }

  dispose() {
    this.meshes.forEach((mesh) => {
      mesh.geometry.dispose()
      mesh.material.dispose()
    })
  }
}
