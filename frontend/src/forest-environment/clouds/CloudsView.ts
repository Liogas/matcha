import {
  CanvasTexture,
  Color,
  Group,
  Mesh,
  PlaneGeometry,
  ShaderMaterial,
  type Texture,
} from 'three'
import { Random } from '../core/random'
import { CLOUD_VARIANTS, type CloudsModel } from './CloudsModel'

const VERTEX = /* glsl */ `
  varying vec2 vUv;
  void main() {
    vUv = uv;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`

const FRAGMENT = /* glsl */ `
  uniform sampler2D uMap;
  uniform vec3 uLit;
  uniform vec3 uShade;
  uniform float uOpacity;
  uniform float uSunSide;
  varying vec2 vUv;
  void main() {
    vec4 texel = texture2D(uMap, vUv);
    // Le relief est peint dans le canal rouge, la forme dans l'alpha
    float light = clamp(texel.r * 0.85 + vUv.y * 0.35 + (vUv.x - 0.5) * uSunSide * 0.35, 0.0, 1.0);
    vec3 color = mix(uShade, uLit, smoothstep(0.15, 0.95, light));
    gl_FragColor = vec4(color, texel.a * uOpacity);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
  }
`

/**
 * Peint un cumulus sur un canevas : une base plate, des bourgeons arrondis
 * au-dessus. Le rouge code l'éclairement (haut clair, bas ombré).
 */
function paintCloud(seed: number): Texture {
  const width = 512
  const height = 256
  const canvas = document.createElement('canvas')
  canvas.width = width
  canvas.height = height
  const context = canvas.getContext('2d')!
  const random = new Random(seed)
  const base = height * 0.78

  const puffs = 26
  for (let index = 0; index < puffs; index += 1) {
    const t = random.next()
    const x = width * (0.12 + 0.76 * t)
    // Plus haut au centre : silhouette en dôme
    const hump = Math.sin(Math.PI * t)
    const radius = (28 + random.next() * 46) * (0.55 + hump * 0.6)
    const y = base - radius * 0.55 - hump * height * 0.2 * random.next()
    const shade = 150 + Math.round(105 * (1 - y / base))
    const gradient = context.createRadialGradient(x, y - radius * 0.3, radius * 0.1, x, y, radius)
    gradient.addColorStop(0, `rgba(${shade},${shade},${shade},0.95)`)
    gradient.addColorStop(0.6, `rgba(${shade - 30},${shade - 30},${shade - 30},0.75)`)
    gradient.addColorStop(1, 'rgba(110,110,110,0)')
    context.fillStyle = gradient
    context.beginPath()
    context.arc(x, y, radius, 0, Math.PI * 2)
    context.fill()
  }

  // Base aplatie et estompée
  context.globalCompositeOperation = 'destination-out'
  const fade = context.createLinearGradient(0, base - 12, 0, base + 18)
  fade.addColorStop(0, 'rgba(0,0,0,0)')
  fade.addColorStop(1, 'rgba(0,0,0,1)')
  context.fillStyle = fade
  context.fillRect(0, base - 12, width, height - base + 12)

  // Texture de données (éclairement + forme) : pas de conversion sRGB
  return new CanvasTexture(canvas)
}

export class CloudsView {
  readonly root = new Group()
  private readonly textures: Texture[] = []
  private readonly meshes: Mesh<PlaneGeometry, ShaderMaterial>[] = []
  private readonly geometry = new PlaneGeometry(1, 0.5)

  constructor(model: CloudsModel) {
    for (let variant = 0; variant < CLOUD_VARIANTS; variant += 1)
      this.textures.push(paintCloud(100 + variant * 31))

    for (const cloud of model.clouds) {
      const material = new ShaderMaterial({
        vertexShader: VERTEX,
        fragmentShader: FRAGMENT,
        transparent: true,
        depthWrite: false,
        uniforms: {
          uMap: { value: this.textures[cloud.variant] },
          uLit: { value: new Color() },
          uShade: { value: new Color() },
          uOpacity: { value: 0 },
          uSunSide: { value: 0 },
        },
      })
      const mesh = new Mesh(this.geometry, material)
      mesh.scale.setScalar(cloud.width)
      mesh.renderOrder = -8
      this.meshes.push(mesh)
      this.root.add(mesh)
    }
  }

  sync(model: CloudsModel) {
    model.clouds.forEach((cloud, index) => {
      const mesh = this.meshes[index]
      mesh.position.set(cloud.x, cloud.y, cloud.z)
      mesh.visible = cloud.opacity > 0.01
      const uniforms = mesh.material.uniforms
      uniforms.uLit.value.copy(model.lit)
      uniforms.uShade.value.copy(model.shade)
      uniforms.uOpacity.value = cloud.opacity * 0.92
      uniforms.uSunSide.value = model.sunSide
    })
  }

  dispose() {
    this.geometry.dispose()
    this.textures.forEach((texture) => texture.dispose())
    this.meshes.forEach((mesh) => mesh.material.dispose())
  }
}
