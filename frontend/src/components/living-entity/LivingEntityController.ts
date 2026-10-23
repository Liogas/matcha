import { MathUtils } from 'three';
import { LivingEntityModel } from './LivingEntityModel.ts';
import type { EntityMood, EntityReaction } from './LivingEntityModel.ts';
import type { MouthStyle } from './MouthView.tsx';

export interface EntityFrame
{
	delta:		number;
	/** Position du cœur à l'écran, en coordonnées normalisées (-1..1). */
	anchorX:	number;
	anchorY:	number;
}

export interface ScreenPoint
{
	x: number;
	y: number;
}

interface ActiveReaction
{
	type:	EntityReaction;
	start:	number;
}

const REACTION_DURATION: Record<EntityReaction, number> = {
	poke:		0.8,
	love:		1.8,
	joy:		2.6,
	sad:		1.9,
	startle:	0.7,
	nod:		1.0,
	tick:		0.25,
};

const SLEEP_AFTER = 40;
const WANDER_AFTER = 4;
const MAX_REACTIONS = 12;

const now = () => performance.now() / 1000;
const clamp01 = (value: number) => MathUtils.clamp(value, 0, 1);
const pulse = (t: number, center: number, width: number) =>
	Math.exp(-((t - center) ** 2) / (2 * width ** 2));
const easeInOut = (t: number) => (t < 0.5 ? 4 * t ** 3 : 1 - (-2 * t + 2) ** 3 / 2);

/** Hauteur d'un saut : temps d'élan, puis arc. */
function hopHeight(p: number): number
{
	if (p < 0.18)
		return 0;
	return Math.sin(Math.PI * clamp01((p - 0.18) / 0.82));
}

/** Écrasement / étirement accompagnant un saut (> 0 = étiré). */
function hopSquash(p: number): number
{
	if (p < 0.18)
		return -0.12 * (p / 0.18);
	if (p < 0.35)
		return MathUtils.lerp(-0.12, 0.1, (p - 0.18) / 0.17);
	if (p < 0.85)
		return MathUtils.lerp(0.1, 0, (p - 0.35) / 0.5);
	return -0.06 * Math.sin(Math.PI * (p - 0.85) / 0.15);
}

/**
 * Cerveau du cœur : reçoit les événements de l'application (pointeur, focus,
 * réactions) et calcule à chaque frame la pose que les vues affichent.
 */
export class LivingEntityController
{
	private readonly model: LivingEntityModel;
	private readonly moodListeners = new Set<() => void>();

	private reactions: ActiveReaction[] = [];
	private pointer: ScreenPoint = { x: 0, y: 0 };
	private focus: ScreenPoint | null = null;
	private eyesHidden = false;
	private reducedMotion = false;

	private lastActivity = now();
	private lastPointerMove = -Infinity;
	private wanderTarget: ScreenPoint = { x: 0, y: 0 };
	private nextWander = 0;
	private wanderAtViewer = true;

	private nextBlink = now() + 1.5;
	private blinkStart = -Infinity;

	private beatPhase = 0;
	private bpm = 64;
	private beatAmplitude = 0.02;

	// Valeurs lissées (le reste de la pose est recalculé directement)
	private smooth = {
		offsetY: 0,
		rotationX: 0,
		rotationY: 0,
		rotationZ: 0,
		upperLid: 1,
		lowerLid: 1,
		eyeScale: 1,
		blush: 0,
	};

	constructor(model: LivingEntityModel)
	{
		this.model = model;
	}

	get mood(): EntityMood
	{
		return this.model.mood;
	}

	/* -------------------------------------------------------------- */
	/* Événements venant de l'application                              */
	/* -------------------------------------------------------------- */

	react(type: EntityReaction): void
	{
		const pose = this.model.pose;

		this.reactions.push({ type, start: now() });
		if (this.reactions.length > MAX_REACTIONS)
			this.reactions.shift();

		if (type === 'love' || type === 'joy') {
			pose.burstSize = type === 'joy' ? 22 : 8;
			pose.burstCount += 1;
		}
		if (type !== 'tick')
			this.notifyActivity();
	}

	setPointer(point: ScreenPoint): void
	{
		this.pointer = point;
		this.lastPointerMove = now();
		this.notifyActivity();
	}

	/** Point à regarder en priorité (champ de formulaire actif…). */
	setFocus(point: ScreenPoint | null): void
	{
		this.focus = point;
	}

	/** Le cœur ferme les yeux et détourne la tête (saisie d'un mot de passe). */
	setEyesHidden(hidden: boolean): void
	{
		this.eyesHidden = hidden;
	}

	setReducedMotion(reduced: boolean): void
	{
		this.reducedMotion = reduced;
	}

	notifyActivity(): void
	{
		this.lastActivity = now();
		if (this.model.mood === 'sleepy') {
			this.setMood('awake');
			this.react('startle');
		}
	}

	subscribeMood(listener: () => void): () => void
	{
		this.moodListeners.add(listener);
		return () => this.moodListeners.delete(listener);
	}

	private setMood(mood: EntityMood): void
	{
		if (this.model.mood === mood)
			return;
		this.model.setMood(mood);
		this.moodListeners.forEach(listener => listener());
	}

	/* -------------------------------------------------------------- */
	/* Boucle d'animation                                              */
	/* -------------------------------------------------------------- */

	update(frame: EntityFrame): void
	{
		const t = now();
		const dt = Math.min(frame.delta, 0.1);
		const pose = this.model.pose;
		const calm = this.reducedMotion ? 0.25 : 1;

		if (!this.eyesHidden && !this.focus && t - this.lastActivity > SLEEP_AFTER)
			this.setMood('sleepy');

		this.reactions = this.reactions.filter(r => t - r.start < REACTION_DURATION[r.type]);

		/* --- Où regarder --- */
		const target = this.lookTarget(t, frame);
		const lookX = MathUtils.clamp(target.x - frame.anchorX, -1, 1);
		const lookY = MathUtils.clamp(target.y - frame.anchorY, -1, 1);

		/* --- Valeurs de base selon l'humeur --- */
		let offsetY = Math.sin(t * 1.5) * 0.05 * calm;
		let rotationX = -lookY * 0.22;
		let rotationY = lookX * 0.38;
		let rotationZ = Math.sin(t * 0.7) * 0.03 * calm;
		let eyeYaw = lookX * 0.55;
		let eyePitch = -lookY * 0.45;
		let upperLid = 1;
		let lowerLid = 1;
		let eyeScale = 1;
		let blush = 0;
		let mouth: MouthStyle = 'smile';
		let bpm = 64;
		let beatAmplitude = 0.02;

		if (this.model.mood === 'sleepy') {
			offsetY = Math.sin(t * 0.8) * 0.03 - 0.06;
			rotationX = 0.18;
			rotationY = 0;
			rotationZ = 0.14;
			eyeYaw = 0;
			eyePitch = 0.3;
			upperLid = 0.08;
			mouth = 'flat';
			bpm = 46;
		}

		if (this.eyesHidden) {
			rotationX = 0.12;
			rotationY = -0.55;
			upperLid = 0;
			lowerLid = 0;
			blush = 0.7;
			mouth = 'smirk';
		}

		/* --- Réactions (les plus récentes l'emportent pour la bouche) --- */
		let hopY = 0;
		let squash = 0;
		let spinY = 0;
		let shakeY = 0;
		let nodX = 0;

		for (const reaction of this.reactions) {
			const age = t - reaction.start;
			const p = age / REACTION_DURATION[reaction.type];
			const fade = 1 - p;

			switch (reaction.type) {
				case 'poke':
					hopY += hopHeight(p) * 0.22;
					squash += hopSquash(p);
					lowerLid = Math.min(lowerLid, 0.55);
					mouth = 'open';
					break;

				case 'love':
					bpm = Math.max(bpm, 120);
					beatAmplitude = Math.max(beatAmplitude, 0.02 + 0.08 * fade);
					blush = Math.max(blush, fade);
					lowerLid = Math.min(lowerLid, 0.5);
					mouth = 'smile';
					break;

				case 'joy': {
					const hop = (p * 2) % 1;
					hopY += hopHeight(hop) * 0.3;
					squash += hopSquash(hop);
					spinY += easeInOut(clamp01(p / 0.5)) * Math.PI * 2;
					bpm = Math.max(bpm, 150);
					beatAmplitude = Math.max(beatAmplitude, 0.03 + 0.07 * fade);
					blush = Math.max(blush, fade);
					lowerLid = Math.min(lowerLid, 0.45);
					eyeScale = Math.max(eyeScale, 1.08);
					mouth = 'open';
					break;
				}

				case 'sad': {
					const envelope = Math.sin(Math.PI * p) ** 0.5;
					offsetY -= 0.1 * envelope;
					rotationX += 0.2 * envelope;
					shakeY += Math.sin(age * 15) * 0.22 * clamp01(1 - age / 0.9);
					upperLid = Math.min(upperLid, 0.55);
					mouth = 'sad';
					break;
				}

				case 'startle':
					hopY += hopHeight(p) * 0.12;
					squash += hopSquash(p) * 0.6;
					eyeScale = Math.max(eyeScale, 1 + 0.2 * fade);
					upperLid = this.eyesHidden ? upperLid : 1;
					mouth = 'surprised';
					break;

				case 'nod':
					nodX += Math.sin(p * Math.PI * 4) * 0.16 * fade;
					lowerLid = Math.min(lowerLid, 0.55);
					mouth = 'smile';
					break;

				case 'tick':
					hopY += Math.sin(Math.PI * p) * 0.025;
					break;
			}
		}

		/* --- Lissage --- */
		const s = this.smooth;
		s.offsetY = MathUtils.damp(s.offsetY, offsetY, 6, dt);
		s.rotationX = MathUtils.damp(s.rotationX, rotationX, 4, dt);
		s.rotationY = MathUtils.damp(s.rotationY, rotationY, 4, dt);
		s.rotationZ = MathUtils.damp(s.rotationZ, rotationZ, 4, dt);
		s.upperLid = MathUtils.damp(s.upperLid, upperLid, 14, dt);
		s.lowerLid = MathUtils.damp(s.lowerLid, lowerLid, 14, dt);
		s.eyeScale = MathUtils.damp(s.eyeScale, eyeScale, 10, dt);
		s.blush = MathUtils.damp(s.blush, blush, 3, dt);
		this.bpm = MathUtils.damp(this.bpm, bpm, 2, dt);
		this.beatAmplitude = MathUtils.damp(this.beatAmplitude, beatAmplitude, 5, dt);

		/* --- Clignement --- */
		if (t > this.nextBlink && s.upperLid > 0.4) {
			this.blinkStart = t;
			this.nextBlink = t + (Math.random() < 0.15 ? 0.3 : 1.8 + Math.random() * 4.5);
		}
		const blinkAge = t - this.blinkStart;
		const blink = blinkAge < 0.16 ? Math.sin(Math.PI * blinkAge / 0.16) : 0;

		/* --- Battement --- */
		this.beatPhase = (this.beatPhase + dt * this.bpm / 60) % 1;
		const beat = pulse(this.beatPhase, 0.08, 0.035) + 0.6 * pulse(this.beatPhase, 0.26, 0.04);

		/* --- Respiration --- */
		const breath = Math.sin(t * 2) * 0.012 * calm;

		/* --- Écriture de la pose --- */
		squash *= calm;
		pose.offsetY = s.offsetY + hopY * calm;
		pose.rotationX = s.rotationX + nodX * calm;
		pose.rotationY = s.rotationY + spinY * calm + shakeY * calm;
		pose.rotationZ = s.rotationZ;
		pose.scaleY = 1 + squash + breath;
		pose.scaleX = 1 - squash * 0.5 - breath * 0.5;
		pose.scaleZ = pose.scaleX;

		pose.eyeYaw = MathUtils.damp(pose.eyeYaw, eyeYaw, 18, dt);
		pose.eyePitch = MathUtils.damp(pose.eyePitch, eyePitch, 18, dt);
		pose.eyeScale = s.eyeScale;
		pose.upperLid = s.upperLid * (1 - blink);
		pose.lowerLid = s.lowerLid * (1 - blink);

		pose.beat = beat * this.beatAmplitude * (this.reducedMotion ? 0.5 : 1);
		pose.blush = s.blush;
		pose.mouth = mouth;
	}

	/** Pointeur, champ actif, ou regard qui se promène quand rien ne bouge. */
	private lookTarget(t: number, frame: EntityFrame): ScreenPoint
	{
		if (this.focus)
			return this.focus;
		if (t - this.lastPointerMove < WANDER_AFTER)
			return this.pointer;

		if (t > this.nextWander) {
			this.nextWander = t + 1.5 + Math.random() * 2.5;
			this.wanderAtViewer = Math.random() < 0.4;
			this.wanderTarget = {
				x: frame.anchorX + (Math.random() - 0.5) * 1.2,
				y: frame.anchorY + (Math.random() - 0.4) * 0.7,
			};
		}
		return this.wanderAtViewer
			? { x: frame.anchorX, y: frame.anchorY }
			: this.wanderTarget;
	}
}
