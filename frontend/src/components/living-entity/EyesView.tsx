import type { Group } from 'three'
import { forwardRef } from 'react'

import type { EntityState } from './LivingEntityModel.ts'

export interface EyesViewProps {
	gazeX: number;
	gazeY: number;
	state: EntityState;
}

const EyesView = forwardRef<Group, EyesViewProps>(
	function EyesView({ gazeX, gazeY, state }, ref) {
		const eyeScale = state === 'curious' ? 1.15 :
			state === 'excited' ? 1.3 :
				1;
		return (
			<group ref={ref} position={[gazeX, gazeY, 0]}>
				<group scale={eyeScale}>
					<mesh position={[-0.25, 0.12, 0.7]}>
						<sphereGeometry args={[0.09, 16, 16]} />
						<meshStandardMaterial color="#17212b" />
					</mesh>

					<mesh position={[0.25, 0.12, 0.7]}>
						<sphereGeometry args={[0.09, 16, 16]} />
						<meshStandardMaterial color="#17212b" />
					</mesh>
				</group>
			</group>
		)
	},
)

export default EyesView
