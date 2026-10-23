import { useEffect, useMemo, useRef } from 'react';
import { Color, ShapeGeometry, Vector3 } from 'three';
import type { Mesh, MeshBasicMaterial } from 'three';
import { useFrame } from '@react-three/fiber';
import { createHeartShape } from './heartShape.ts';
import type { EntityPose } from './LivingEntityModel.ts';

interface HeartBurstProps {
	pose: EntityPose;
	colors: string[];
}

interface Particle {
	age:		number;
	life:		number;
	velocity:	Vector3;
	spin:		number;
	size:		number;
}

const POOL_SIZE = 24;
const GRAVITY = -1.6;
const SLOTS = Array.from({ length: POOL_SIZE }, (_, index) => index);

function createPool(): Particle[]
{
	return SLOTS.map(() => ({
		age: 1,
		life: 1,
		velocity: new Vector3(),
		spin: 0,
		size: 0,
	}));
}

/** Petits cœurs qui jaillissent quand pose.burstCount change (like, match…). */
export default function HeartBurst({ pose, colors }: HeartBurstProps) {
	const meshes = useRef<Array<Mesh | null>>([]);
	const particles = useRef<Particle[]>([]);
	const lastBurst = useRef(pose.burstCount);

	const geometry = useMemo(() => {
		const shape = new ShapeGeometry(createHeartShape(), 12);
		shape.center();
		return shape;
	}, []);
	const palette = useMemo(() => colors.map(color => new Color(color)), [colors]);

	useEffect(() => () => geometry.dispose(), [geometry]);

	const spawn = (count: number) => {
		let spawned = 0;
		particles.current.forEach((particle, index) => {
			const mesh = meshes.current[index];
			if (!mesh || spawned >= count || particle.age < particle.life)
				return;

			const angle = Math.random() * Math.PI * 2;
			const speed = 1.2 + Math.random() * 1.6;
			particle.age = 0;
			particle.life = 1.1 + Math.random() * 0.8;
			particle.velocity.set(
				Math.cos(angle) * speed,
				Math.abs(Math.sin(angle)) * speed + 0.8,
				0.4 + Math.random() * 0.6,
			);
			particle.spin = (Math.random() - 0.5) * 6;
			particle.size = 0.07 + Math.random() * 0.09;

			mesh.position.set(Math.cos(angle) * 0.4, Math.sin(angle) * 0.4, 0.3);
			mesh.rotation.z = (Math.random() - 0.5) * 0.8;
			(mesh.material as MeshBasicMaterial).color.copy(palette[spawned % palette.length]);
			mesh.visible = true;
			spawned += 1;
		});
	};

	useFrame((_, delta) => {
		const dt = Math.min(delta, 0.1);

		if (particles.current.length === 0)
			particles.current = createPool();

		if (pose.burstCount !== lastBurst.current) {
			lastBurst.current = pose.burstCount;
			spawn(pose.burstSize);
		}

		particles.current.forEach((particle, index) => {
			const mesh = meshes.current[index];
			if (!mesh)
				return;

			particle.age += dt;
			const p = particle.age / particle.life;
			if (p >= 1) {
				mesh.visible = false;
				return;
			}

			particle.velocity.y += GRAVITY * dt;
			mesh.position.addScaledVector(particle.velocity, dt);
			mesh.rotation.z += particle.spin * dt;
			// Apparition rapide, disparition douce
			mesh.scale.setScalar(particle.size * Math.min(1, p * 8) * (1 - p * 0.4));
			(mesh.material as MeshBasicMaterial).opacity = 1 - p ** 2;
		});
	});

	return (
		<group>
			{SLOTS.map((index) => (
				<mesh
					key={index}
					ref={mesh => { meshes.current[index] = mesh; }}
					geometry={geometry}
					visible={false}
				>
					<meshBasicMaterial transparent depthWrite={false} toneMapped={false} />
				</mesh>
			))}
		</group>
	);
}
