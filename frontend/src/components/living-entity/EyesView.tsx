import type { Group } from 'three'
import { forwardRef } from 'react'

export interface EyesViewProps {
	gazeX: number
	gazeY: number
}

const EyesView = forwardRef<Group, EyesViewProps>(
	function EyesView({ gazeX, gazeY }, ref) {
		return (
			<group ref={ref} position={[gazeX, gazeY, 0]}>
				<mesh position={[-0.25, 0.12, 0.7]}>
					<sphereGeometry args={[0.09, 16, 16]} />
					<meshStandardMaterial color="#17212b" />
				</mesh>

				<mesh position={[0.25, 0.12, 0.7]}>
					<sphereGeometry args={[0.09, 16, 16]} />
					<meshStandardMaterial color="#17212b" />
				</mesh>
			</group>
		)
	},
)

export default EyesView
