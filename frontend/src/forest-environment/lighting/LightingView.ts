import {
  DirectionalLight,
  Fog,
  Group,
  HemisphereLight,
  type Scene,
  type WebGLRenderer,
} from 'three'
import { CLEARING } from '../core/composition'
import type { ForestQuality } from '../core/types'
import type { LightingModel } from './LightingModel'

const SHADOW_EXTENT = 24
const LIGHT_DISTANCE = 70

export class LightingView {
  readonly root = new Group()
  private readonly key = new DirectionalLight()
  private readonly hemisphere = new HemisphereLight()
  private readonly fog = new Fog('#b8c8d4', 30, 190)

  private readonly scene: Scene
  private readonly renderer: WebGLRenderer

  constructor(
    scene: Scene,
    renderer: WebGLRenderer,
    quality: ForestQuality,
  ) {
    this.scene = scene
    this.renderer = renderer
    this.key.castShadow = true
    const size = quality === 'high' ? 2048 : 1024
    this.key.shadow.mapSize.set(size, size)
    const camera = this.key.shadow.camera
    camera.left = -SHADOW_EXTENT
    camera.right = SHADOW_EXTENT
    camera.top = SHADOW_EXTENT
    camera.bottom = -SHADOW_EXTENT
    camera.near = 1
    camera.far = LIGHT_DISTANCE * 2
    this.key.shadow.bias = -0.0008
    this.key.shadow.normalBias = 0.04
    this.key.shadow.radius = 3
    this.key.target.position.set(CLEARING.x, 0, CLEARING.z)

    this.root.add(this.key, this.key.target, this.hemisphere)
    scene.fog = this.fog
  }

  sync(model: LightingModel) {
    this.key.position.copy(model.keyDirection).multiplyScalar(LIGHT_DISTANCE).add(this.key.target.position)
    this.key.color.copy(model.keyColor)
    // Jamais masquée : changer le nombre de lumières recompilerait tous les shaders
    this.key.intensity = model.keyIntensity

    this.hemisphere.color.copy(model.skyColor)
    this.hemisphere.groundColor.copy(model.groundColor)
    this.hemisphere.intensity = model.ambientIntensity

    this.fog.color.copy(model.fogColor)
    this.fog.near = model.fogNear
    this.fog.far = model.fogFar
    this.renderer.toneMappingExposure = model.exposure
  }

  dispose() {
    this.key.dispose()
    this.hemisphere.dispose()
    if (this.scene.fog === this.fog) this.scene.fog = null
  }
}
