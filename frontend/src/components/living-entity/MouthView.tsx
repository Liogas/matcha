import { useLayoutEffect, useMemo, useRef } from 'react';
import { BufferGeometry, Color, ExtrudeGeometry, MathUtils, Shape, ShapeGeometry } from 'three';
import type { Mesh } from 'three';
import { useFrame } from '@react-three/fiber';

export type MouthStyle = 'smile' | 'open' | 'smirk' | 'sad' | 'surprised' | 'flat';

interface MouthViewProps {
	mouthStyle: MouthStyle;
	/** Couleur du corps : les lèvres en sont dérivées (plus foncées et saturées). */
	bodyColor: string;
	position?: [number, number, number];
}

/* ------------------------------------------------------------------ */
/* Presets                                                             */
/* ------------------------------------------------------------------ */
/**
 * La bouche est définie par deux courbes qui partagent les mêmes commissures :
 *  - la courbe du haut (bord de la lèvre supérieure),
 *  - la courbe du bas (bord de la lèvre inférieure).
 * "sag" = de combien le milieu de la courbe descend par rapport aux commissures.
 * Bouche fermée : les deux courbes sont quasi confondues (ça trace le trait de la bouche).
 * Bouche ouverte : l'écart entre les deux laisse apparaître l'intérieur, les dents et la langue.
 */
interface MouthParams {
	halfWidth: number;
	cornerLeft: number;
	cornerRight: number;
	upperSag: number;
	lowerSag: number;
	offsetX: number;
}

const PRESETS: Record<MouthStyle, MouthParams> = {
	smile: { halfWidth: 0.075, cornerLeft: 0, cornerRight: 0, upperSag: 0.025, lowerSag: 0.029, offsetX: 0 },
	open: { halfWidth: 0.08, cornerLeft: 0.012, cornerRight: 0.012, upperSag: 0.012, lowerSag: 0.1, offsetX: 0 },
	smirk: { halfWidth: 0.06, cornerLeft: -0.004, cornerRight: 0.018, upperSag: 0.014, lowerSag: 0.019, offsetX: 0.025 },
	// Sag négatif : le milieu remonte au-dessus des commissures
	sad: { halfWidth: 0.06, cornerLeft: -0.012, cornerRight: -0.012, upperSag: -0.02, lowerSag: -0.015, offsetX: 0 },
	surprised: { halfWidth: 0.034, cornerLeft: 0, cornerRight: 0, upperSag: -0.022, lowerSag: 0.026, offsetX: 0 },
	flat: { halfWidth: 0.05, cornerLeft: 0, cornerRight: 0, upperSag: 0.004, lowerSag: 0.008, offsetX: 0 },
};

const PARAM_KEYS = Object.keys(PRESETS.smile) as Array<keyof MouthParams>;

/* ------------------------------------------------------------------ */
/* Géométrie                                                           */
/* ------------------------------------------------------------------ */
type Point = [number, number];

const SAMPLES = 28;

const UPPER_LIP_THICKNESS = 0.013;
const LOWER_LIP_THICKNESS = 0.017;
const MIN_THICKNESS = 0.0012;

/** Courbe de Bézier quadratique entre les deux commissures. */
function sampleCurve(p: MouthParams, sag: number): Point[] {
	const xl = p.offsetX - p.halfWidth;
	const xr = p.offsetX + p.halfWidth;
	const mid = (p.cornerLeft + p.cornerRight) / 2;
	const cx = p.offsetX;
	const cy = mid - 2 * sag; // le milieu de la courbe tombe exactement à (mid - sag)

	const points: Point[] = [];
	for (let i = 0; i <= SAMPLES; i++) {
		const t = i / SAMPLES;
		const u = 1 - t;
		points.push([
			u * u * xl + 2 * u * t * cx + t * t * xr,
			u * u * p.cornerLeft + 2 * u * t * cy + t * t * p.cornerRight,
		]);
	}
	return points;
}

/** Épaisseur qui s'effile aux commissures. */
const taper = (base: number) => (t: number) =>
	Math.max(MIN_THICKNESS, base * Math.sin(Math.PI * t) ** 0.75);

/** Ruban le long d'une courbe, vers le haut (+1) ou vers le bas (-1). */
function ribbon(base: Point[], thickness: (t: number) => number, direction: 1 | -1): Shape {
	const shape = new Shape();
	shape.moveTo(base[0][0], base[0][1]);
	for (let i = 1; i < base.length; i++)
		shape.lineTo(base[i][0], base[i][1]);

	for (let i = base.length - 1; i >= 0; i--) {
		const h = thickness(i / SAMPLES) * direction;
		shape.lineTo(base[i][0], base[i][1] + h);
	}
	shape.closePath();
	return shape;
}

/** Surface comprise entre deux courbes qui partagent leurs extrémités. */
function between(upper: Point[], lower: Point[]): Shape {
	const shape = new Shape();
	shape.moveTo(upper[0][0], upper[0][1]);
	for (let i = 1; i < upper.length; i++)
		shape.lineTo(upper[i][0], upper[i][1]);
	for (let i = lower.length - 2; i >= 1; i--)
		shape.lineTo(lower[i][0], lower[i][1]);
	shape.closePath();
	return shape;
}

const LIP_EXTRUDE = {
	depth: 0.004,
	bevelEnabled: true,
	bevelThickness: 0.003,
	bevelSize: 0.0025,
	bevelSegments: 2,
	curveSegments: 1,
	steps: 1,
};

type PartName = 'cavity' | 'tongue' | 'teeth' | 'upperLip' | 'lowerLip';
type Parts = Partial<Record<PartName, Mesh | null>>;

function setGeometry(mesh: Mesh | null | undefined, geometry: BufferGeometry | null) {
	if (!mesh) {
		geometry?.dispose();
		return;
	}
	mesh.geometry.dispose();
	mesh.geometry = geometry ?? new BufferGeometry();
	mesh.visible = geometry !== null;
}

function rebuildMouth(parts: Parts, p: MouthParams) {
	const upper = sampleCurve(p, p.upperSag);
	const lower = sampleCurve(p, p.lowerSag);
	const gap = p.lowerSag - p.upperSag;

	// Intérieur de la bouche (fin trait sombre quand elle est fermée)
	setGeometry(parts.cavity, new ShapeGeometry(between(upper, lower), 1));

	// Dents du haut
	let teethHeight = 0;
	if (gap > 0.03) {
		teethHeight = Math.min(0.032, gap * 0.45);
		const teethEdge = sampleCurve(p, p.upperSag + teethHeight);
		setGeometry(parts.teeth, new ShapeGeometry(between(upper, teethEdge), 1));
	} else {
		setGeometry(parts.teeth, null);
	}

	// Langue : bosse posée contre la lèvre inférieure
	if (gap > 0.045) {
		const tongueHeight = Math.min(0.045, (gap - teethHeight) * 0.7);
		const tongue = ribbon(
			lower,
			t => Math.max(MIN_THICKNESS, tongueHeight * Math.sin(Math.PI * t) ** 0.9),
			1,
		);
		setGeometry(parts.tongue, new ShapeGeometry(tongue, 6));
	} else {
		setGeometry(parts.tongue, null);
	}

	// Lèvres
	setGeometry(
		parts.upperLip,
		new ExtrudeGeometry(ribbon(upper, taper(UPPER_LIP_THICKNESS), 1), LIP_EXTRUDE),
	);
	setGeometry(
		parts.lowerLip,
		new ExtrudeGeometry(ribbon(lower, taper(LOWER_LIP_THICKNESS), -1), LIP_EXTRUDE),
	);
}

/* ------------------------------------------------------------------ */
/* Composant                                                           */
/* ------------------------------------------------------------------ */
export default function MouthView({
	mouthStyle,
	bodyColor,
	position = [0, 0, 0],
}: MouthViewProps) {
	const parts = useRef<Parts>({});
	const current = useRef<MouthParams>({ ...PRESETS[mouthStyle] });
	const built = useRef<MouthParams>({ ...PRESETS[mouthStyle] });

	const lipColor = useMemo(
		() => new Color(bodyColor).lerp(new Color('#7a1f45'), 0.4),
		[bodyColor],
	);
	const tongueColor = useMemo(
		() => new Color(bodyColor).lerp(new Color('#e0405f'), 0.55),
		[bodyColor],
	);

	// Première construction + nettoyage
	useLayoutEffect(() => {
		rebuildMouth(parts.current, current.current);
		built.current = { ...current.current };

		const meshes = parts.current;
		return () => {
			(Object.keys(meshes) as PartName[]).forEach(name => meshes[name]?.geometry.dispose());
		};
	}, []);

	// Transition fluide entre deux expressions
	useFrame((_, delta) => {
		const target = PRESETS[mouthStyle];
		let changed = false;

		for (const key of PARAM_KEYS) {
			current.current[key] = MathUtils.damp(current.current[key], target[key], 10, delta);
			if (Math.abs(current.current[key] - built.current[key]) > 2e-4)
				changed = true;
		}

		if (changed) {
			rebuildMouth(parts.current, current.current);
			built.current = { ...current.current };
		}
	});

	return (
		<group position={position}>
			{/* Intérieur de la bouche */}
			<mesh ref={m => { parts.current.cavity = m; }}>
				<meshStandardMaterial color="#2a0a18" roughness={0.85} />
			</mesh>

			{/* Langue */}
			<mesh ref={m => { parts.current.tongue = m; }} position={[0, 0, 0.0015]}>
				<meshStandardMaterial color={tongueColor} roughness={0.6} />
			</mesh>

			{/* Dents du haut */}
			<mesh ref={m => { parts.current.teeth = m; }} position={[0, 0, 0.003]}>
				<meshStandardMaterial color="#fbf6ee" roughness={0.35} />
			</mesh>

			{/* Lèvres */}
			<mesh ref={m => { parts.current.upperLip = m; }} position={[0, 0, 0.001]}>
				<meshStandardMaterial color={lipColor} roughness={0.45} />
			</mesh>
			<mesh ref={m => { parts.current.lowerLip = m; }} position={[0, 0, 0.001]}>
				<meshStandardMaterial color={lipColor} roughness={0.45} />
			</mesh>
		</group>
	);
}