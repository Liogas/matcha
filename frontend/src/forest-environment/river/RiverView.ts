import {
  BufferAttribute,
  BufferGeometry,
  CanvasTexture,
  Color,
  Group,
  IcosahedronGeometry,
  Mesh,
  MeshLambertMaterial,
  ShaderMaterial,
  Sprite,
  SpriteMaterial,
  UniformsLib,
  UniformsUtils,
  Vector3,
  type Texture,
} from 'three'
import { facet, jitter, paint } from '../core/geometry'
import { GLSL_NOISE } from '../core/glsl'
import { applyEffects, type SharedUniforms } from '../core/materialEffects'
import { Random } from '../core/random'
import type { FrameContext } from '../core/types'
import type { TerrainModel } from '../terrain/TerrainModel'
import type { RiverModel } from './RiverModel'

const ACROSS = 6
const MIST_SPRITES = 9

const VERTEX = /* glsl */ `
  varying vec2 vUv;
  varying vec3 vWorld;
  #include <fog_pars_vertex>
  void main() {
    vUv = uv;
    vec4 world = modelMatrix * vec4(position, 1.0);
    vWorld = world.xyz;
    vec4 mvPosition = viewMatrix * world;
    gl_Position = projectionMatrix * mvPosition;
    #include <fog_vertex>
  }
`

const FRAGMENT = /* glsl */ `
  uniform vec3 uDeep;
  uniform vec3 uShallow;
  uniform vec3 uReflection;
  uniform vec3 uFoam;
  uniform vec3 uGlintColor;
  uniform vec3 uGlintDirection;
  uniform float uGlintStrength;
  uniform float uIce;
  uniform float uFlow;
  varying vec2 vUv;
  varying vec3 vWorld;
  #include <fog_pars_fragment>
  ${GLSL_NOISE}
  void main() {
    float edge = abs(vUv.x * 2.0 - 1.0);
    // Deux couches de courant à des vitesses différentes : l'eau ne glisse pas d'un bloc
    vec2 flowA = vec2(vUv.x * 2.2, vUv.y * 0.55 - uFlow);
    vec2 flowB = vec2(vUv.x * 4.1 + 3.0, vUv.y * 1.1 - uFlow * 1.7);
    float ripple = fbm2(flowA) * 0.6 + fbm2(flowB) * 0.4;

    vec3 color = mix(uDeep, uShallow, smoothstep(0.25, 1.0, edge) * 0.8 + ripple * 0.15);

    vec3 toCamera = normalize(cameraPosition - vWorld);
    vec3 normal = normalize(vec3((ripple - 0.5) * 0.5, 1.0, (fbm2(flowB + 7.0) - 0.5) * 0.5));
    float fresnel = pow(1.0 - max(dot(toCamera, normal), 0.0), 3.0);
    color = mix(color, uReflection, 0.12 + fresnel * 0.45);

    vec3 reflected = reflect(-toCamera, normal);
    float glint = pow(max(dot(reflected, uGlintDirection), 0.0), 90.0);
    color += uGlintColor * glint * uGlintStrength * 0.9;

    // Écume le long des berges, étirée par le courant
    float foam = smoothstep(0.78, 1.0, edge + (ripple - 0.5) * 0.35);
    foam *= 0.55 + 0.45 * noise2(vec2(vUv.x * 9.0, vUv.y * 2.4 - uFlow * 2.0));
    color = mix(color, uFoam, foam * 0.55);

    // Glace : elle gagne depuis les bords quand il gèle
    float ice = smoothstep(1.0 - uIce * 1.3, 1.08 - uIce * 1.3, edge + (noise2(vUv * vec2(3.0, 0.8)) - 0.5) * 0.3);
    color = mix(color, uFoam * vec3(0.92, 0.97, 1.02) + uReflection * 0.15, ice * 0.9);

    gl_FragColor = vec4(color, 0.94);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
    #include <fog_fragment>
  }
`

function buildRibbon(model: RiverModel) {
  const { points } = model.path
  const count = points.length
  const positions = new Float32Array(count * (ACROSS + 1) * 3)
  const uvs = new Float32Array(count * (ACROSS + 1) * 2)
  const indices: number[] = []
  const tangent = new Vector3()
  let along = 0

  for (let index = 0; index < count; index += 1) {
    const point = points[index]
    const previous = points[Math.max(index - 1, 0)]
    const next = points[Math.min(index + 1, count - 1)]
    tangent.subVectors(next, previous).setY(0).normalize()
    if (index > 0) along += point.distanceTo(previous)
    const t = index / (count - 1)
    const width = model.path.widthAt(t) * 1.06
    const level = model.path.levelAt(t)
    for (let step = 0; step <= ACROSS; step += 1) {
      const u = step / ACROSS
      const offset = (u - 0.5) * width
      const vertex = index * (ACROSS + 1) + step
      positions.set([point.x - tangent.z * offset, level, point.z + tangent.x * offset], vertex * 3)
      uvs.set([u, along / 4], vertex * 2)
      if (index > 0 && step < ACROSS) {
        const a = vertex - (ACROSS + 1)
        indices.push(a, a + 1, vertex, vertex, a + 1, vertex + 1)
      }
    }
  }
  const geometry = new BufferGeometry()
  geometry.setAttribute('position', new BufferAttribute(positions, 3))
  geometry.setAttribute('uv', new BufferAttribute(uvs, 2))
  geometry.setIndex(indices)
  return geometry
}

/** Texture de brume : nappe douce et irrégulière, peinte au canevas. */
function paintMist(): Texture {
  const size = 256
  const canvas = document.createElement('canvas')
  canvas.width = size
  canvas.height = size / 2
  const context = canvas.getContext('2d')!
  const random = new Random(7)
  for (let index = 0; index < 18; index += 1) {
    const x = size * random.range(0.2, 0.8)
    const y = (size / 2) * random.range(0.4, 0.65)
    const radius = random.range(30, 60)
    const gradient = context.createRadialGradient(x, y, 0, x, y, radius)
    gradient.addColorStop(0, 'rgba(255,255,255,0.22)')
    gradient.addColorStop(1, 'rgba(255,255,255,0)')
    context.fillStyle = gradient
    context.fillRect(0, 0, size, size / 2)
  }
  return new CanvasTexture(canvas)
}

export class RiverView {
  readonly root = new Group()
  private readonly water: Mesh<BufferGeometry, ShaderMaterial>
  private readonly rocks: Mesh<BufferGeometry, MeshLambertMaterial>[] = []
  private readonly mistTexture: Texture
  private readonly mistMaterial: SpriteMaterial
  private readonly mist: { sprite: Sprite; t: number; offset: number; speed: number }[] = []

  constructor(model: RiverModel, terrain: TerrainModel, shared: SharedUniforms) {
    this.water = new Mesh(buildRibbon(model), new ShaderMaterial({
      vertexShader: VERTEX,
      fragmentShader: FRAGMENT,
      transparent: true,
      fog: true,
      uniforms: UniformsUtils.merge([
        UniformsLib.fog,
        {
          uDeep: { value: new Color() },
          uShallow: { value: new Color() },
          uReflection: { value: new Color() },
          uFoam: { value: new Color() },
          uGlintColor: { value: new Color() },
          uGlintDirection: { value: new Vector3() },
          uGlintStrength: { value: 0 },
          uIce: { value: 0 },
          uFlow: { value: 0 },
        },
      ]),
    }))
    this.water.receiveShadow = false
    this.root.add(this.water)

    // Quelques galets qui affleurent dans le courant et sur les berges
    const random = new Random(5)
    const rockMaterial = applyEffects(new MeshLambertMaterial({ vertexColors: true, flatShading: true }), shared, { snow: true })
    for (let index = 0; index < 11; index += 1) {
      const t = random.range(0.8, 0.985)
      const point = model.path.points[Math.round(t * (model.path.points.length - 1))]
      const side = random.chance(0.5) ? -1 : 1
      const width = model.path.widthAt(t)
      const across = side * width * random.range(0.25, 0.62)
      const size = random.range(0.18, 0.5)
      let geometry = jitter(new IcosahedronGeometry(size, 0), size * 0.5, index * 13)
      geometry = paint(facet(geometry), random.chance(0.4) ? '#5f6a5a' : '#73716a', 0.25, index)
      const rock = new Mesh(geometry, rockMaterial)
      const x = point.x + across
      const z = point.z + random.range(-0.6, 0.6)
      rock.position.set(x, Math.min(terrain.heightAt(x, z), model.path.levelAt(t)) + size * 0.25, z)
      rock.scale.set(1, random.range(0.45, 0.7), 1)
      rock.rotation.y = random.range(0, Math.PI * 2)
      rock.castShadow = true
      rock.receiveShadow = true
      this.rocks.push(rock)
      this.root.add(rock)
    }

    this.mistTexture = paintMist()
    this.mistMaterial = new SpriteMaterial({ map: this.mistTexture, transparent: true, depthWrite: false, fog: true, opacity: 0 })
    for (let index = 0; index < MIST_SPRITES; index += 1) {
      const sprite = new Sprite(this.mistMaterial)
      sprite.scale.set(random.range(9, 15), random.range(2.2, 3.4), 1)
      this.mist.push({ sprite, t: random.range(0.55, 0.98), offset: random.range(-1.5, 1.5), speed: random.range(0.0012, 0.0024) })
      this.root.add(sprite)
    }
  }

  sync(model: RiverModel, frame: FrameContext) {
    const uniforms = this.water.material.uniforms
    uniforms.uDeep.value.copy(model.deep)
    uniforms.uShallow.value.copy(model.shallow)
    uniforms.uReflection.value.copy(model.reflection)
    uniforms.uFoam.value.copy(model.foam)
    uniforms.uGlintColor.value.copy(model.glintColor)
    uniforms.uGlintDirection.value.copy(model.glintDirection)
    uniforms.uGlintStrength.value = model.glintStrength
    uniforms.uIce.value = model.ice
    uniforms.uFlow.value = model.flow

    // La brume dérive lentement vers l'aval
    this.mistMaterial.opacity = model.mist * 0.8
    this.mistMaterial.color.copy(model.mistColor)
    const { points } = model.path
    for (const patch of this.mist) {
      patch.t += patch.speed * frame.delta
      if (patch.t > 0.99) patch.t = 0.55
      const point = points[Math.round(patch.t * (points.length - 1))]
      patch.sprite.position.set(point.x + patch.offset, model.path.levelAt(patch.t) + 0.9, point.z)
      patch.sprite.visible = model.mist > 0.02
    }
  }

  dispose() {
    this.water.geometry.dispose()
    this.water.material.dispose()
    this.rocks.forEach((rock) => rock.geometry.dispose())
    this.rocks[0]?.material.dispose()
    this.mistTexture.dispose()
    this.mistMaterial.dispose()
  }
}
