import type { Group } from 'three'
import { useMemo, useRef, useState } from 'react'
import { useFrame } from '@react-three/fiber'
import { LivingEntityController } from './LivingEntityController.ts'
import type { LivingEntityModel } from './LivingEntityModel.ts'
import type { MouthStyle } from './MouthView.tsx'
import HeartBodyView from './HeartBodyView.tsx'
import HeartFaceView from './HeartFaceView.tsx'
import HeartBurst from './HeartBurst.tsx'
import {
	DEFAULT_APPEARANCE,
} from './LivingEntityAppearance.ts';
import type {
	LivingEntityAppearance,
} from './LivingEntityAppearance.ts';

interface LivingEntityViewProps {
	model:			LivingEntityModel;
	controller:		LivingEntityController;
	/** Position du cœur à l'écran (coordonnées normalisées), pour orienter le regard. */
	anchor:			{ x: number; y: number };
	appearance?:	LivingEntityAppearance;
}

export default function LivingEntityView({
	model,
	controller,
	anchor,
	appearance = DEFAULT_APPEARANCE,
}: LivingEntityViewProps) {
	const bodyRef = useRef<Group>(null);
	// La bouche est reconstruite côté React : on ne déclenche un rendu que si l'expression change
	const [mouth, setMouth] = useState<MouthStyle>(model.pose.mouth);
	const mouthRef = useRef(mouth);

	const burstColors = useMemo(
		() => [appearance.bodyColor, appearance.secondaryColor, '#ffd1e0', '#ff3d7f'],
		[appearance.bodyColor, appearance.secondaryColor],
	);

	useFrame((_, delta) => {
		controller.update({ delta, anchorX: anchor.x, anchorY: anchor.y });

		const pose = model.pose;
		const body = bodyRef.current;
		if (body) {
			body.position.y = pose.offsetY;
			body.rotation.set(pose.rotationX, pose.rotationY, pose.rotationZ);
			body.scale.set(pose.scaleX, pose.scaleY, pose.scaleZ);
		}

		if (pose.mouth !== mouthRef.current) {
			mouthRef.current = pose.mouth;
			setMouth(pose.mouth);
		}
	})

	return (
		<group>
			<group ref={bodyRef}>
				<HeartBodyView appearance={appearance} pose={model.pose} />
				<HeartFaceView pose={model.pose} mouth={mouth} appearance={appearance} />
			</group>
			<HeartBurst pose={model.pose} colors={burstColors} />
		</group>
	)
}
