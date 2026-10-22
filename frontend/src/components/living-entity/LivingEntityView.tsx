import type { Group } from 'three'
import { useEffect, useRef } from 'react'
import { useFrame } from '@react-three/fiber'
import { LivingEntityModel } from './LivingEntityModel.ts'
import { LivingEntityController } from './LivingEntityController.ts'
import EyesView from './EyesView.tsx'

export default function LivingEntityView() {
	const bodyRef = useRef<Group>(null);
	const eyesRef = useRef<Group>(null);

	const modelRef = useRef<LivingEntityModel | null>(null);
	const controllerRef = useRef<LivingEntityController | null>(null);

	if (modelRef.current === null) {
		modelRef.current = new LivingEntityModel();
		controllerRef.current = new LivingEntityController(
			modelRef.current,
		);
	}

	useFrame((frame, delta) => {
		const controller = controllerRef.current;
		const model = modelRef.current;
		const body = bodyRef.current;

		if (!controller || !model || !body) return

		controller.update({
			time: frame.clock.elapsedTime,
			delta,
			pointerX: frame.pointer.x,
			pointerY: frame.pointer.y,
		})

		const state = model.state;
		const eyes = eyesRef.current

		if (eyes) {
		eyes.position.x = state.gazeX
		eyes.position.y = state.gazeY
		}
		body.position.y = state.positionY;
		body.rotation.x = state.rotationX;
		body.rotation.y = state.rotationY;
		body.scale.setScalar(state.scale);
	})

	const handleClick = () => {
		controllerRef.current?.toggleState();
	};

	return (
		<group ref={bodyRef}>
			<mesh>
				<sphereGeometry args={[0.8, 32, 32]} />
				<meshStandardMaterial
					color="#65d9c0"
					roughness={0.35}
				/>
			</mesh>
			<EyesView
				ref={eyesRef}
				onClick={handleClick}
				gazeY={modelRef.current!.state.gazeY}
				gazeX={modelRef.current!.state.gazeX}
			/>
		</group>
	)
}