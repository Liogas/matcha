import { useEffect, useMemo, useRef } from 'react';
import { CanvasTexture, SRGBColorSpace } from 'three';
import type { Group, MeshBasicMaterial } from 'three';
import { useFrame } from '@react-three/fiber';
import type { EntityPose } from './LivingEntityModel.ts';
import type { LivingEntityAppearance } from './LivingEntityAppearance.ts';
import EyeView from './EyeView.tsx';
import MouthView from './MouthView.tsx';
import type { MouthStyle } from './MouthView.tsx';

interface HeartFaceViewProps {
	pose: EntityPose;
	mouth: MouthStyle;
	appearance: LivingEntityAppearance;
}

// Centre de la bouche : juste au-dessus de la face avant du cœur (z ≈ 0.42)
const MOUTH_POSITION: [number, number, number] = [0, -0.15, 0.425];
const CHEEKS: Array<[number, number, number]> = [[-0.44, -0.07, 0.424], [0.44, -0.07, 0.424]];

/** Disque rose aux bords flous pour les joues. */
function createBlushTexture(): CanvasTexture {
	const size = 128;
	const canvas = document.createElement('canvas');
	canvas.width = size;
	canvas.height = size;
	const ctx = canvas.getContext('2d')!;
	const gradient = ctx.createRadialGradient(size / 2, size / 2, 0, size / 2, size / 2, size / 2);
	gradient.addColorStop(0, 'rgba(255,60,110,0.9)');
	gradient.addColorStop(0.55, 'rgba(255,60,110,0.45)');
	gradient.addColorStop(1, 'rgba(255,60,110,0)');
	ctx.fillStyle = gradient;
	ctx.fillRect(0, 0, size, size);

	const texture = new CanvasTexture(canvas);
	texture.colorSpace = SRGBColorSpace;
	return texture;
}

export default function HeartFaceView({ pose, mouth, appearance }: HeartFaceViewProps) {
	const eyesRef = useRef<Group>(null);
	const blushMaterials = useRef<Array<MeshBasicMaterial | null>>([]);
	const blushTexture = useMemo(() => createBlushTexture(), []);

	useEffect(() => () => blushTexture.dispose(), [blushTexture]);

	useFrame(() => {
		eyesRef.current?.scale.setScalar(pose.eyeScale);
		for (const material of blushMaterials.current) {
			if (material)
				material.opacity = 0.15 + pose.blush * 0.75;
		}
	});

	return (
		<group>
			<group ref={eyesRef} position={[0, 0.16, 0.39]}>
				<group position={[-0.27, 0, 0]}>
					<EyeView
						eyeColor={appearance.eyeColor}
						eyeStyle={appearance.eyeStyle}
						lidColor={appearance.bodyColor}
						pose={pose}
					/>
				</group>

				<group position={[0.27, 0, 0]}>
					<EyeView
						eyeColor={appearance.eyeColor}
						eyeStyle={appearance.eyeStyle}
						lidColor={appearance.bodyColor}
						pose={pose}
					/>
				</group>
			</group>

			{CHEEKS.map((position, index) => (
				<mesh key={index} position={position} scale={[1, 0.62, 1]}>
					<circleGeometry args={[0.12, 32]} />
					<meshBasicMaterial
						ref={material => { blushMaterials.current[index] = material; }}
						map={blushTexture}
						transparent
						opacity={0.15}
						depthWrite={false}
					/>
				</mesh>
			))}

			<MouthView
				// Le style choisi dans l'apparence remplace seulement le sourire de repos
				mouthStyle={mouth === 'smile' ? appearance.mouthStyle ?? mouth : mouth}
				bodyColor={appearance.bodyColor}
				position={MOUTH_POSITION}
			/>
		</group>
	);
}
