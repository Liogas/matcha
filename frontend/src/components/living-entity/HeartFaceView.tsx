import { forwardRef } from 'react';
import type { Group } from 'three';
import type { EntityState } from './LivingEntityModel.ts';
import type { LivingEntityAppearance } from './LivingEntityAppearance.ts';
import EyeView from './EyeView.tsx';
import MouthView from './MouthView.tsx';
import type { MouthStyle } from './MouthView.tsx';

interface HeartFaceViewProps {
	state: EntityState;
	appearance: LivingEntityAppearance;
}

// Centre de la bouche : juste au-dessus de la face avant du cœur (z ≈ 0.42)
const MOUTH_POSITION: [number, number, number] = [0, -0.15, 0.425];

const HeartFaceView = forwardRef<Group, HeartFaceViewProps>(
	function HeartFaceView(
		{ state, appearance },
		gazeRef,
	) {
		const eyeScale =
			state === 'curious'
				? 1.12
				: state === 'excited'
					? 1.08
					: 1;

		const effectiveMouthStyle: MouthStyle =
			appearance.mouthStyle ??
			(state === 'excited' ? 'open' : 'smile');

		return (
			<group>
				{/* Groupe mobile : seuls les yeux suivent le pointeur. */}
				<group ref={gazeRef}>
					<group scale={eyeScale}>
						<group position={[-0.27, 0.16, 0.39]}>
							<EyeView
								eyeColor={appearance.eyeColor}
								eyeStyle={appearance.eyeStyle}
							/>
						</group>

						<group position={[0.27, 0.16, 0.39]}>
							<EyeView
								eyeColor={appearance.eyeColor}
								eyeStyle={appearance.eyeStyle}
							/>
						</group>
					</group>
				</group>

				{/* La bouche reste fixe par rapport au cœur. */}
				<MouthView
					mouthStyle={effectiveMouthStyle}
					bodyColor={appearance.bodyColor}
					position={MOUTH_POSITION}
				/>
			</group>
		);
	},
);

export default HeartFaceView;