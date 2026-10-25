import {
  ACESFilmicToneMapping,
  Object3D,
  PCFShadowMap,
  PerspectiveCamera,
  Raycaster,
  Scene,
  SRGBColorSpace,
  Vector2,
  Vector3,
  WebGLRenderer,
} from 'three'
import { CAMERA } from './composition'
import { damp } from './math'
import type { ForestController, ForestQuality, FrameContext } from './types'

/** Un décor figé n'a besoin d'être redessiné que pour suivre la course du soleil. */
const FROZEN_FRAME_INTERVAL = 2

interface Pickable {
  object: Object3D
  onPick: () => void
}

/**
 * Infrastructure three.js : rendu, caméra, boucle d'animation, redimensionnement.
 * Le moteur ne connaît aucun élément du décor : il appelle les contrôleurs
 * dans l'ordre où ils ont été enregistrés.
 */
export class ForestEngine {
  readonly scene = new Scene()
  readonly camera = new PerspectiveCamera(CAMERA.fov, 1, 0.1, 900)
  readonly renderer: WebGLRenderer
  readonly pointer = new Vector2()

  private readonly controllers: ForestController[] = []
  private readonly pickables: Pickable[] = []
  private readonly raycaster = new Raycaster()
  private readonly cameraOffset = new Vector2()
  private readonly lookTarget = new Vector3(...CAMERA.target)
  private readonly frame: FrameContext
  private container: HTMLElement | null = null
  private resizeObserver: ResizeObserver | null = null
  private animationId = 0
  private lastTime = 0
  private sinceRender = Infinity
  private motion = 1
  private rightInset = 0

  readonly quality: ForestQuality

  constructor(quality: ForestQuality) {
    this.quality = quality
    this.renderer = new WebGLRenderer({
      antialias: quality === 'high',
      powerPreference: quality === 'high' ? 'high-performance' : 'low-power',
    })
    this.renderer.outputColorSpace = SRGBColorSpace
    this.renderer.toneMapping = ACESFilmicToneMapping
    this.renderer.toneMappingExposure = 1
    this.renderer.shadowMap.enabled = true
    this.renderer.shadowMap.type = PCFShadowMap
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, quality === 'high' ? 1.75 : 1.25))
    this.renderer.domElement.setAttribute('aria-hidden', 'true')

    this.camera.position.set(...CAMERA.position)
    this.camera.lookAt(this.lookTarget)
    this.frame = { delta: 0, elapsed: 0, motion: 1, pointer: this.pointer, camera: this.camera }
  }

  add(controller: ForestController) {
    this.controllers.push(controller)
  }

  /** Rend un objet cliquable (le louveteau, par exemple). */
  addPickable(object: Object3D, onPick: () => void) {
    this.pickables.push({ object, onPick })
  }

  mount(container: HTMLElement) {
    this.container = container
    container.appendChild(this.renderer.domElement)
    this.resizeObserver = new ResizeObserver(() => this.resize())
    this.resizeObserver.observe(container)
    this.resize()
    window.addEventListener('pointermove', this.onPointerMove, { passive: true })
    this.renderer.domElement.addEventListener('click', this.onClick)
    document.addEventListener('visibilitychange', this.onVisibilityChange)
    this.start()
  }

  unmount() {
    this.stop()
    window.removeEventListener('pointermove', this.onPointerMove)
    this.renderer.domElement.removeEventListener('click', this.onClick)
    document.removeEventListener('visibilitychange', this.onVisibilityChange)
    this.resizeObserver?.disconnect()
    this.resizeObserver = null
    this.renderer.domElement.remove()
    this.container = null
  }

  /** 1 = animé, 0 = figé. Le temps réel (jour, saison) continue d'avancer. */
  setMotion(motion: number) {
    this.motion = motion
    this.sinceRender = Infinity
  }

  /**
   * Part de l'écran masquée à droite par le contenu : la caméra décale
   * son cadrage pour garder l'arbre et la tanière dans la partie visible.
   */
  setRightInset(fraction: number) {
    this.rightInset = Math.min(Math.max(fraction, 0), 0.6)
    this.resize()
  }

  dispose() {
    this.unmount()
    this.controllers.forEach((controller) => controller.dispose())
    this.controllers.length = 0
    this.renderer.dispose()
  }

  private resize() {
    if (!this.container) return
    const width = Math.max(this.container.clientWidth, 1)
    const height = Math.max(this.container.clientHeight, 1)
    const shift = Math.round(width * this.rightInset * 0.6)
    this.renderer.setSize(width, height, false)
    // Portrait : on recule un peu pour garder l'arbre entier dans le cadre
    this.camera.fov = width / height < 0.9 ? CAMERA.fov * 1.25 : CAMERA.fov
    this.camera.aspect = (width + shift) / height
    if (shift > 0)
      this.camera.setViewOffset(width + shift, height, shift, 0, width, height)
    else
      this.camera.clearViewOffset()
    this.camera.updateProjectionMatrix()
    this.sinceRender = Infinity
  }

  private start() {
    if (this.animationId) return
    this.lastTime = performance.now()
    this.animationId = requestAnimationFrame(this.tick)
  }

  private stop() {
    cancelAnimationFrame(this.animationId)
    this.animationId = 0
  }

  private readonly tick = (time: number) => {
    this.animationId = requestAnimationFrame(this.tick)
    // Plafonné : après un onglet en arrière-plan, on ne rattrape pas des minutes d'animation
    const realDelta = Math.min((time - this.lastTime) / 1000, 0.1)
    this.lastTime = time
    this.sinceRender += realDelta

    if (this.motion === 0 && this.sinceRender < FROZEN_FRAME_INTERVAL)
      return
    const delta = this.motion === 0 ? 0 : realDelta * this.motion
    this.sinceRender = 0

    this.frame.delta = delta
    this.frame.elapsed += delta
    this.frame.motion = this.motion
    this.updateCamera(realDelta)
    for (const controller of this.controllers)
      controller.update(this.frame)
    this.renderer.render(this.scene, this.camera)
  }

  /** Légère parallaxe au pointeur : les plans du décor glissent les uns sur les autres. */
  private updateCamera(delta: number) {
    const targetX = this.motion > 0 ? this.pointer.x * CAMERA.parallax[0] : 0
    const targetY = this.motion > 0 ? this.pointer.y * CAMERA.parallax[1] : 0
    this.cameraOffset.x = damp(this.cameraOffset.x, targetX, 1.6, delta)
    this.cameraOffset.y = damp(this.cameraOffset.y, targetY, 1.6, delta)
    this.camera.position.set(
      CAMERA.position[0] + this.cameraOffset.x,
      CAMERA.position[1] + this.cameraOffset.y,
      CAMERA.position[2],
    )
    this.camera.lookAt(this.lookTarget)
  }

  private readonly onPointerMove = (event: PointerEvent) => {
    this.pointer.set(
      (event.clientX / window.innerWidth) * 2 - 1,
      -(event.clientY / window.innerHeight) * 2 + 1,
    )
    if (event.target === this.renderer.domElement)
      this.renderer.domElement.style.cursor = this.pick(event) ? 'pointer' : ''
  }

  private readonly onClick = (event: MouseEvent) => {
    this.pick(event)?.onPick()
  }

  private pick(event: MouseEvent) {
    if (this.pickables.length === 0) return null
    const rect = this.renderer.domElement.getBoundingClientRect()
    const ndc = new Vector2(
      ((event.clientX - rect.left) / rect.width) * 2 - 1,
      -((event.clientY - rect.top) / rect.height) * 2 + 1,
    )
    this.raycaster.setFromCamera(ndc, this.camera)
    return this.pickables.find(({ object }) => this.raycaster.intersectObject(object, true).length > 0) ?? null
  }

  private readonly onVisibilityChange = () => {
    if (document.hidden) this.stop()
    else if (this.container) this.start()
  }
}
