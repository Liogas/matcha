import type { Group } from 'three'
import { useRef, useState } from 'react'
import { useFrame } from '@react-three/fiber'
import { LivingEntityModel } from './LivingEntityModel.ts'
import { LivingEntityController } from './LivingEntityController.ts'
import type { EntityState } from './LivingEntityModel.ts'
import HeartBodyView from './HeartBodyView.tsx'
import HeartFaceView from './HeartFaceView.tsx'
import {
	DEFAULT_APPEARANCE,
} from './LivingEntityAppearance.ts';
import type {
	LivingEntityAppearance,
} from './LivingEntityAppearance.ts';

export default function LivingEntityView() {
	const [entityState, setEntityState] = useState<EntityState>('idle');
	const [appearance, setAppearance] = useState<LivingEntityAppearance>(DEFAULT_APPEARANCE);

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
		const controller = controllerRef.current;
		if (!controller)
			return;
		controller.toggleState();
		setEntityState(modelRef.current!.state.state);
	};

	return (
		<group ref={bodyRef} onClick={handleClick}>
			<HeartBodyView appearance={appearance} />
			<HeartFaceView
				ref={eyesRef}
				state={entityState}
				appearance={appearance}
			/>
		</group>
	)
}