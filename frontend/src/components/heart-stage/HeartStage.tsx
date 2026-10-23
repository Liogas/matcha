import { Canvas, useFrame, useThree } from '@react-three/fiber'
import { useEffect, useMemo, useRef } from 'react'
import {
  AdditiveBlending,
  BufferAttribute,
  BufferGeometry,
  CanvasTexture,
  MathUtils,
} from 'three'
import type { Group, Points } from 'three'
import LivingEntityView from '../living-entity/LivingEntityView'
import { useHeart } from '../../context/useHeart'
import type { HeartContextValue } from '../../context/HeartContext'
import { toNormalized } from './heartLayout'
import type { HeartLayout, ViewportSize } from './heartLayout'

/** Hauteur du cœur dans ses propres unités (forme + biseau). */
const HEART_MODEL_HEIGHT = 1.9

interface HeartStageProps {
  layout: HeartLayout
  viewport: ViewportSize
}

/**
 * Déplace le cœur vers la place que lui donne la mise en page,
 * avec une légère inclinaison dans le sens du mouvement.
 */
function HeartRig({ layout, viewport, model, controller }: HeartStageProps & HeartContextValue) {
  const rig = useRef<Group>(null)
  const placed = useRef(false)
  const world = useThree((state) => state.viewport)
  const size = useThree((state) => state.size)

  const anchor = useMemo(
    () => toNormalized(layout.x, layout.y, viewport),
    [layout.x, layout.y, viewport],
  )

  useFrame((_, delta) => {
    const group = rig.current
    if (!group || size.width === 0)
      return

    const unit = world.height / size.height
    const targetX = (layout.x - size.width / 2) * unit
    const targetY = (size.height / 2 - layout.y) * unit
    const targetScale = (layout.size * unit) / HEART_MODEL_HEIGHT

    if (!placed.current) {
      group.position.set(targetX, targetY, 0)
      group.scale.setScalar(targetScale)
      placed.current = true
      return
    }

    const previousX = group.position.x
    group.position.x = MathUtils.damp(group.position.x, targetX, 4.5, delta)
    group.position.y = MathUtils.damp(group.position.y, targetY, 4.5, delta)
    group.scale.setScalar(MathUtils.damp(group.scale.x, targetScale, 4.5, delta))

    const velocityX = (group.position.x - previousX) / Math.max(delta, 1e-3)
    group.rotation.z = MathUtils.damp(group.rotation.z, MathUtils.clamp(-velocityX * 0.08, -0.35, 0.35), 8, delta)
  })

  return (
    <group ref={rig}>
      <LivingEntityView model={model} controller={controller} anchor={anchor} />
    </group>
  )
}

function createDotTexture() {
  const canvas = document.createElement('canvas')
  canvas.width = 64
  canvas.height = 64
  const ctx = canvas.getContext('2d')!
  const gradient = ctx.createRadialGradient(32, 32, 0, 32, 32, 32)
  gradient.addColorStop(0, 'rgba(255,255,255,1)')
  gradient.addColorStop(0.3, 'rgba(255,220,235,0.6)')
  gradient.addColorStop(1, 'rgba(255,220,235,0)')
  ctx.fillStyle = gradient
  ctx.fillRect(0, 0, 64, 64)
  return new CanvasTexture(canvas)
}

const SPARKLE_COUNT = 140

function createSparkleField() {
  const positions = new Float32Array(SPARKLE_COUNT * 3)
  const speeds = new Float32Array(SPARKLE_COUNT)
  for (let i = 0; i < SPARKLE_COUNT; i++) {
    positions[i * 3] = (Math.random() - 0.5) * 16
    positions[i * 3 + 1] = (Math.random() - 0.5) * 10
    positions[i * 3 + 2] = -1 - Math.random() * 6
    speeds[i] = 0.05 + Math.random() * 0.12
  }
  const geometry = new BufferGeometry()
  geometry.setAttribute('position', new BufferAttribute(positions, 3))
  return { geometry, speeds }
}

/** Poussière lumineuse qui flotte lentement derrière le cœur. */
function Sparkles() {
  const points = useRef<Points>(null)
  const texture = useMemo(() => createDotTexture(), [])
  const { geometry, speeds } = useMemo(() => createSparkleField(), [])

  useEffect(() => () => {
    geometry.dispose()
    texture.dispose()
  }, [geometry, texture])

  useFrame(({ clock }, delta) => {
    const attribute = geometry.getAttribute('position') as BufferAttribute
    const positions = attribute.array as Float32Array
    const time = clock.elapsedTime
    for (let i = 0; i < SPARKLE_COUNT; i++) {
      positions[i * 3 + 1] += speeds[i] * delta
      positions[i * 3] += Math.sin(time * 0.3 + i) * 0.002
      if (positions[i * 3 + 1] > 5)
        positions[i * 3 + 1] = -5
    }
    attribute.needsUpdate = true
    if (points.current)
      points.current.rotation.z = Math.sin(time * 0.05) * 0.05
  })

  return (
    <points ref={points} geometry={geometry}>
      <pointsMaterial
        map={texture}
        size={0.09}
        sizeAttenuation
        transparent
        opacity={0.55}
        depthWrite={false}
        blending={AdditiveBlending}
        color="#ffc4dc"
      />
    </points>
  )
}

/** Toile 3D plein écran, derrière toute l'interface. Montée une seule fois. */
export function HeartStage(props: HeartStageProps) {
  // Passé en props : le contexte React ne traverse pas forcément le Canvas
  const heart = useHeart()
  return (
    <div className="heart-stage" aria-hidden="true">
      <Canvas
        camera={{ position: [0, 0, 6], fov: 35 }}
        dpr={[1, 1.75]}
        gl={{ alpha: true, antialias: true }}
        fallback={<div className="heart-fallback">♥</div>}
      >
        <ambientLight intensity={0.9} />
        <directionalLight position={[-3, 4, 5]} intensity={2.4} color="#fff1e8" />
        <pointLight position={[3, -1, 3]} intensity={14} color="#8b70f5" />
        <pointLight position={[-2.5, 2.5, -1.5]} intensity={18} color="#ff7aa8" />
        <Sparkles />
        <HeartRig {...props} {...heart} />
      </Canvas>
    </div>
  )
}
