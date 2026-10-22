// src/components/EntityScene.tsx

import { Canvas } from '@react-three/fiber'
import LivingEntityView from '../components/living-entity/LivingEntityView.tsx'

export default function EntityScene() {
  return (
    <div className="entity-scene">
      <Canvas camera={{ position: [0, 0, 3.5], fov: 50 }}>
        <ambientLight intensity={1.5} />
        <directionalLight position={[3, 4, 5]} intensity={2} />

        <LivingEntityView />
      </Canvas>
    </div>
  )
}