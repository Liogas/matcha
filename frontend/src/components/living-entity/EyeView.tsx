import { useEffect, useMemo, useRef } from 'react';
import { CanvasTexture, Color, DoubleSide, RepeatWrapping, SRGBColorSpace } from 'three';
import type { Group, Mesh } from 'three';
import { useFrame } from '@react-three/fiber';
import type { EyeStyle } from './LivingEntityAppearance.ts';
import type { EntityPose } from './LivingEntityModel.ts';

interface EyeViewProps {
	eyeColor: string;
	eyeStyle: EyeStyle;
	/** Taille de la pupille par rapport à l'iris (0.2 = contractée, 0.5 = dilatée). */
	pupilSize?: number;
	/** Couleur des paupières (la « peau » du cœur). */
	lidColor: string;
	/** Pose partagée : orientation du regard et ouverture des paupières. */
	pose: EntityPose;
}

/* ------------------------------------------------------------------ */
/* Géométrie (unités = celles de la scène)                             */
/* ------------------------------------------------------------------ */
const EYEBALL_RADIUS = 0.13;

// Iris : calotte sphérique posée sur le globe
const IRIS_RADIUS = 0.062;
const IRIS_SPHERE_R = EYEBALL_RADIUS + 0.0005;
const IRIS_THETA = Math.asin(IRIS_RADIUS / IRIS_SPHERE_R);

// Cornée : calotte plus bombée qui dépasse du globe (vraie anatomie)
const CORNEA_BASE_R = 0.068;
const CORNEA_SPHERE_R = 0.085;
const CORNEA_THETA = Math.asin(CORNEA_BASE_R / CORNEA_SPHERE_R);
const CORNEA_Z =
	Math.sqrt(EYEBALL_RADIUS ** 2 - CORNEA_BASE_R ** 2) -
	Math.sqrt(CORNEA_SPHERE_R ** 2 - CORNEA_BASE_R ** 2);

/* ------------------------------------------------------------------ */
/* Utilitaires de textures procédurales                                */
/* ------------------------------------------------------------------ */
function mulberry32(seed: number) {
	return () => {
		seed |= 0;
		seed = (seed + 0x6d2b79f5) | 0;
		let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
		t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
		return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
	};
}

const css = (c: Color) => `#${c.getHexString()}`;

/**
 * Texture d'iris en coordonnées polaires :
 *   x = angle autour de l'iris, y = distance au centre (haut = pupille, bas = bord).
 * Les fibres radiales sont donc de simples lignes verticales.
 */
function createIrisTexture(eyeColor: string, pupil: number): CanvasTexture {
	const W = 1024;
	const H = 512;
	const canvas = document.createElement('canvas');
	canvas.width = W;
	canvas.height = H;
	const ctx = canvas.getContext('2d')!;
	const rand = mulberry32(1337);

	const base = new Color(eyeColor);
	const light = base.clone().lerp(new Color('#ffffff'), 0.35);
	const warm = base.clone().lerp(new Color('#c8a050'), 0.45); // collerette
	const dark = base.clone().multiplyScalar(0.35);
	const veryDark = base.clone().multiplyScalar(0.08);

	// 1. Dégradé radial de base
	const grad = ctx.createLinearGradient(0, 0, 0, H);
	grad.addColorStop(0, '#000000');
	grad.addColorStop(pupil, '#000000');
	grad.addColorStop(pupil + 0.015, css(dark));
	grad.addColorStop(pupil + 0.06, css(warm));
	grad.addColorStop(pupil + 0.22, css(light));
	grad.addColorStop(0.62, css(base));
	grad.addColorStop(0.86, css(dark));
	grad.addColorStop(1, css(veryDark));
	ctx.fillStyle = grad;
	ctx.fillRect(0, 0, W, H);

	const y0 = (pupil + 0.02) * H;

	// 2. Fibres radiales (claires puis sombres)
	for (let pass = 0; pass < 2; pass++) {
		const count = pass === 0 ? 1600 : 1100;
		for (let i = 0; i < count; i++) {
			const x = rand() * W;
			const ya = y0 + rand() * H * 0.15;
			const yb = H * (0.65 + rand() * 0.33);
			const sway = (rand() - 0.5) * 10;
			ctx.beginPath();
			ctx.moveTo(x, ya);
			ctx.quadraticCurveTo(x + sway, (ya + yb) / 2, x + sway * 0.4, yb);
			ctx.lineWidth = 0.6 + rand() * 2.2;
			ctx.strokeStyle = pass === 0
				? `rgba(255,255,255,${0.04 + rand() * 0.14})`
				: `rgba(0,0,0,${0.05 + rand() * 0.18})`;
			ctx.stroke();
		}
	}

	// 3. Cryptes (petites zones sombres autour de la collerette)
	for (let i = 0; i < 70; i++) {
		const x = rand() * W;
		const y = (pupil + 0.07 + rand() * 0.2) * H;
		ctx.beginPath();
		ctx.ellipse(x, y, 3 + rand() * 9, 6 + rand() * 14, 0, 0, Math.PI * 2);
		ctx.fillStyle = `rgba(0,0,0,${0.08 + rand() * 0.14})`;
		ctx.fill();
	}

	// 4. Anneau limbique (bord sombre et flou de l'iris)
	const limbal = ctx.createLinearGradient(0, H * 0.78, 0, H);
	limbal.addColorStop(0, 'rgba(0,0,0,0)');
	limbal.addColorStop(0.65, 'rgba(0,0,0,0.45)');
	limbal.addColorStop(1, 'rgba(0,0,0,0.9)');
	ctx.fillStyle = limbal;
	ctx.fillRect(0, H * 0.78, W, H * 0.22);

	// 5. Liseré sombre autour de la pupille
	const ruff = ctx.createLinearGradient(0, (pupil - 0.01) * H, 0, (pupil + 0.07) * H);
	ruff.addColorStop(0, 'rgba(0,0,0,1)');
	ruff.addColorStop(1, 'rgba(0,0,0,0)');
	ctx.fillStyle = ruff;
	ctx.fillRect(0, (pupil - 0.01) * H, W, 0.08 * H);

	const tex = new CanvasTexture(canvas);
	tex.colorSpace = SRGBColorSpace;
	tex.wrapS = RepeatWrapping;
	tex.anisotropy = 8;
	tex.needsUpdate = true;
	return tex;
}

/**
 * Texture de sclère (équirectangulaire) : blanc cassé, ombrage vers les bords
 * et fines veines rouges rayonnant depuis l'iris.
 */
function createScleraTexture(): CanvasTexture {
	const W = 1024;
	const H = 512;
	const canvas = document.createElement('canvas');
	canvas.width = W;
	canvas.height = H;
	const ctx = canvas.getContext('2d')!;
	const rand = mulberry32(42);

	ctx.fillStyle = '#f1ebe4';
	ctx.fillRect(0, 0, W, H);

	// Le pôle avant (+Z) de SphereGeometry se trouve à u = 0.25
	const cx = W * 0.25;
	const cy = H * 0.5;

	// Légère teinte jaunâtre / grisée par zones
	for (let i = 0; i < 40; i++) {
		const g = ctx.createRadialGradient(
			rand() * W, rand() * H, 0,
			rand() * W, rand() * H, 60 + rand() * 120,
		);
		g.addColorStop(0, `rgba(200,170,140,${0.05 + rand() * 0.06})`);
		g.addColorStop(1, 'rgba(200,170,140,0)');
		ctx.fillStyle = g;
		ctx.fillRect(0, 0, W, H);
	}

	// Veines rayonnant depuis l'iris
	for (let i = 0; i < 90; i++) {
		const angle = rand() * Math.PI * 2;
		const r = 80 + rand() * 40;
		let x = cx + Math.cos(angle) * r;
		let y = cy + Math.sin(angle) * r;
		let a = angle;
		ctx.beginPath();
		ctx.moveTo(x, y);
		const len = 40 + rand() * 120;
		for (let s = 0; s < len; s += 6) {
			a += (rand() - 0.5) * 0.5;
			x += Math.cos(a) * 6;
			y += Math.sin(a) * 6;
			ctx.lineTo(x, y);
		}
		ctx.lineWidth = 0.5 + rand() * 1.3;
		ctx.strokeStyle = `rgba(170,40,40,${0.07 + rand() * 0.16})`;
		ctx.stroke();
	}

	const tex = new CanvasTexture(canvas);
	tex.colorSpace = SRGBColorSpace;
	tex.wrapS = RepeatWrapping;
	tex.anisotropy = 8;
	tex.needsUpdate = true;
	return tex;
}


/* ------------------------------------------------------------------ */
/* Paupières                                                           */
/* ------------------------------------------------------------------ */
// Coques légèrement plus grandes que le globe, qui pivotent autour de son centre.
const LID_RADIUS = EYEBALL_RADIUS * 1.05;
// Angle d'ouverture maximal : bord de paupière relevé (haut) ou abaissé (bas).
const UPPER_LID_OPEN = 1.3;
const LOWER_LID_OPEN = 1.15;

/* ------------------------------------------------------------------ */
/* Composant                                                           */
/* ------------------------------------------------------------------ */
export default function EyeView({
	eyeColor,
	eyeStyle,
	pupilSize = 0.32,
	lidColor,
	pose,
}: EyeViewProps) {
	const eyeballRef = useRef<Group>(null);
	const upperLidRef = useRef<Mesh>(null);
	const lowerLidRef = useRef<Mesh>(null);

	const irisTexture = useMemo(() => createIrisTexture(eyeColor, pupilSize), [eyeColor, pupilSize]);
	const scleraTexture = useMemo(() => createScleraTexture(), []);
	const lidTint = useMemo(() => new Color(lidColor).multiplyScalar(0.88), [lidColor]);

	useEffect(() => () => irisTexture.dispose(), [irisTexture]);
	useEffect(() => () => scleraTexture.dispose(), [scleraTexture]);

	// C'est le globe entier qui tourne (et non l'iris qui glisse), comme un vrai œil.
	// Les paupières, elles, restent attachées au visage.
	useFrame(() => {
		const eyeball = eyeballRef.current;
		if (eyeball) {
			eyeball.rotation.y = pose.eyeYaw;
			eyeball.rotation.x = pose.eyePitch;
		}
		if (upperLidRef.current)
			upperLidRef.current.rotation.x = -UPPER_LID_OPEN * pose.upperLid;
		if (lowerLidRef.current)
			lowerLidRef.current.rotation.x = LOWER_LID_OPEN * pose.lowerLid;
	});

	const eyeScale =
		eyeStyle === 'almond'
			? [1.1, 0.82, 1] as const
			: eyeStyle === 'narrow'
				? [1.1, 0.65, 1] as const
				: [1, 1, 1] as const;

	return (
		<group scale={eyeScale}>
			<group ref={eyeballRef}>

				{/* Sclère (blanc de l'œil) */}
				<mesh>
					<sphereGeometry args={[EYEBALL_RADIUS, 64, 48]} />
					<meshStandardMaterial
						map={scleraTexture}
						roughness={0.35}
						metalness={0}
					/>
				</mesh>

				{/* Iris + pupille (calotte sphérique, texture polaire) */}
				<mesh rotation={[Math.PI / 2, 0, 0]}>
					<sphereGeometry args={[IRIS_SPHERE_R, 96, 32, 0, Math.PI * 2, 0, IRIS_THETA]} />
					<meshStandardMaterial
						map={irisTexture}
						emissive="#ffffff"
						emissiveMap={irisTexture}
						emissiveIntensity={0.18}
						roughness={0.5}
						metalness={0}
					/>
				</mesh>

				{/* Cornée : dôme transparent et brillant qui donne la profondeur */}
				<mesh position={[0, 0, CORNEA_Z]} rotation={[Math.PI / 2, 0, 0]}>
					<sphereGeometry args={[CORNEA_SPHERE_R, 64, 32, 0, Math.PI * 2, 0, CORNEA_THETA]} />
					<meshPhysicalMaterial
						color="#ffffff"
						transparent
						opacity={0.1}
						roughness={0}
						metalness={0}
						ior={1.376}
						specularIntensity={1}
						clearcoat={1}
						clearcoatRoughness={0}
						depthWrite={false}
					/>
				</mesh>

				{/* Reflet principal (fenêtre / source de lumière) */}
				<mesh position={[-0.024, 0.027, 0.1385]} scale={[1, 1, 0.35]}>
					<sphereGeometry args={[0.0105, 20, 16]} />
					<meshBasicMaterial color="#ffffff" transparent opacity={0.92} toneMapped={false} />
				</mesh>

				{/* Reflet secondaire, plus petit et plus doux */}
				<mesh position={[0.021, -0.019, 0.1405]} scale={[1, 1, 0.35]}>
					<sphereGeometry args={[0.0042, 16, 12]} />
					<meshBasicMaterial color="#ffffff" transparent opacity={0.6} toneMapped={false} />
				</mesh>
			</group>

			{/* Paupière supérieure : hémisphère du haut */}
			<mesh ref={upperLidRef}>
				<sphereGeometry args={[LID_RADIUS, 48, 16, 0, Math.PI * 2, 0, Math.PI / 2]} />
				<meshStandardMaterial color={lidTint} roughness={0.4} side={DoubleSide} />
			</mesh>

			{/* Paupière inférieure : hémisphère du bas */}
			<mesh ref={lowerLidRef}>
				<sphereGeometry args={[LID_RADIUS, 48, 16, 0, Math.PI * 2, Math.PI / 2, Math.PI / 2]} />
				<meshStandardMaterial color={lidTint} roughness={0.4} side={DoubleSide} />
			</mesh>
		</group>
	);
}
