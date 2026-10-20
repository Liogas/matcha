import { Canvas, useFrame, useThree } from '@react-three/fiber'
import { useEffect, useRef } from 'react'
import * as THREE from 'three'

type AvatarMood = 'welcoming' | 'thoughtful' | 'curious' | 'playful'

interface AvatarSceneProps {
  mood: AvatarMood
  reaction: number
  onInteract: () => void
  reduceMotion: boolean
}

function Avatar({ mood, reaction, onInteract, reduceMotion }: AvatarSceneProps) {
  const root = useRef<THREE.Group>(null)
  const head = useRef<THREE.Group>(null)
  const arm = useRef<THREE.Mesh>(null)
  const reactionTime = useRef(0)
  const viewportWidth = useThree((state) => state.viewport.width)

  useEffect(() => {
    reactionTime.current = 1
  }, [mood, reaction])

  useFrame(({ clock }, delta) => {
    if (reduceMotion || !root.current || !head.current || !arm.current) return
    const time = clock.getElapsedTime()
    reactionTime.current = Math.max(0, reactionTime.current - delta * 0.7)
    const bounce = Math.sin(time * 2.1) * 0.07
    const greeting = reactionTime.current * Math.sin(time * 15) * 0.28

    root.current.position.x = Math.min(0.92, viewportWidth * 0.3)
    root.current.scale.setScalar(Math.min(1, viewportWidth / 3.4))
    root.current.position.y = bounce + reactionTime.current * 0.28
    root.current.rotation.y = Math.sin(time * 0.45) * 0.12
    head.current.rotation.z =
      Math.sin(time * 0.9) * 0.06 +
      (mood === 'thoughtful' ? -0.12 : 0) +
      greeting * 0.18
    arm.current.rotation.z =
      (mood === 'welcoming' ? -0.9 : -0.28) + greeting
  })

  return (
    <group
      ref={root}
      position={[0.92, -0.15, 0]}
      onClick={(event) => {
        event.stopPropagation()
        onInteract()
      }}
      onPointerOver={(event) => {
        event.stopPropagation()
        document.body.style.cursor = 'pointer'
      }}
      onPointerOut={() => {
        document.body.style.cursor = ''
      }}
    >
      <group ref={head} position={[0, 0.6, 0]}>
        <mesh castShadow>
          <sphereGeometry args={[0.52, 24, 20]} />
          <meshStandardMaterial color="#f4d5bd" roughness={0.72} />
        </mesh>
        <mesh position={[0, 0.42, -0.05]} scale={[1.02, 0.56, 0.95]}>
          <sphereGeometry args={[0.48, 18, 12]} />
          <meshStandardMaterial color="#293b31" roughness={0.9} />
        </mesh>
        <mesh position={[-0.18, 0.04, 0.455]}>
          <sphereGeometry args={[0.045, 12, 10]} />
          <meshStandardMaterial color="#29312b" />
        </mesh>
        <mesh position={[0.18, 0.04, 0.455]}>
          <sphereGeometry args={[0.045, 12, 10]} />
          <meshStandardMaterial color="#29312b" />
        </mesh>
        <mesh position={[0, -0.13, 0.48]}>
          <torusGeometry args={[0.07, 0.015, 8, 16, Math.PI]} />
          <meshStandardMaterial color="#bd7867" />
        </mesh>
        <mesh position={[-0.54, -0.01, 0]}>
          <sphereGeometry args={[0.12, 14, 12]} />
          <meshStandardMaterial color="#e9b9a5" />
        </mesh>
        <mesh position={[0.54, -0.01, 0]}>
          <sphereGeometry args={[0.12, 14, 12]} />
          <meshStandardMaterial color="#e9b9a5" />
        </mesh>
      </group>

      <mesh position={[0, -0.15, 0]} castShadow>
        <capsuleGeometry args={[0.32, 0.52, 4, 12]} />
        <meshStandardMaterial
          color={mood === 'welcoming' ? '#a7c99a' : '#90b895'}
          roughness={0.7}
        />
      </mesh>
      <mesh position={[-0.2, -0.7, 0.02]} rotation={[0, 0, 0.12]}>
        <capsuleGeometry args={[0.11, 0.24, 3, 8]} />
        <meshStandardMaterial color="#314b3c" />
      </mesh>
      <mesh position={[0.2, -0.7, 0.02]} rotation={[0, 0, -0.12]}>
        <capsuleGeometry args={[0.11, 0.24, 3, 8]} />
        <meshStandardMaterial color="#314b3c" />
      </mesh>
      <mesh
        ref={arm}
        position={[0.48, -0.16, 0.06]}
        rotation={[0, 0, mood === 'welcoming' ? -0.9 : -0.28]}
      >
        <capsuleGeometry args={[0.09, 0.28, 3, 8]} />
        <meshStandardMaterial color="#f4d5bd" />
      </mesh>
    </group>
  )
}

function SceneOrbits({ reduceMotion }: { reduceMotion: boolean }) {
  const ring = useRef<THREE.Mesh>(null)
  useFrame(({ clock }) => {
    if (reduceMotion || !ring.current) return
    ring.current.rotation.z = Math.sin(clock.getElapsedTime() * 0.25) * 0.12
  })

  return (
    <>
      <mesh ref={ring} position={[0.9, -0.1, -0.5]} rotation={[1.08, 0.12, 0.1]}>
        <torusGeometry args={[1.26, 0.008, 8, 96]} />
        <meshBasicMaterial color="#b7d3a5" transparent opacity={0.58} />
      </mesh>
      <mesh position={[-1.35, 1.15, -0.9]}>
        <sphereGeometry args={[0.08, 12, 10]} />
        <meshBasicMaterial color="#d9aa80" />
      </mesh>
      <mesh position={[1.92, 1.05, -0.7]}>
        <sphereGeometry args={[0.055, 12, 10]} />
        <meshBasicMaterial color="#d9aa80" />
      </mesh>
      <mesh position={[-1.65, -0.92, -0.8]}>
        <sphereGeometry args={[0.045, 12, 10]} />
        <meshBasicMaterial color="#d9aa80" />
      </mesh>
    </>
  )
}

export function AvatarScene(props: AvatarSceneProps) {
  return (
    <div className="avatar-scene" aria-hidden="true">
      <Canvas
        camera={{ position: [0, 0, 5.4], fov: 42 }}
        dpr={[1, 1.35]}
        gl={{ alpha: true, antialias: true, powerPreference: 'low-power' }}
        fallback={<div className="avatar-fallback" />}
      >
        <ambientLight intensity={1.2} />
        <directionalLight position={[-3, 4, 5]} intensity={2.1} color="#fff1d8" />
        <pointLight position={[2.5, 1.4, 2]} intensity={18} color="#b7d3a5" />
        <SceneOrbits reduceMotion={props.reduceMotion} />
        <Avatar {...props} />
      </Canvas>
    </div>
  )
}
