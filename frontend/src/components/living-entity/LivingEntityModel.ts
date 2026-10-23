import type { MouthStyle } from './MouthView.tsx';

/** Humeur de fond : dure tant que rien ne la change. */
export type EntityMood = 'awake' | 'sleepy';

/** Réactions ponctuelles : elles se superposent à l'humeur puis s'estompent. */
export type EntityReaction =
	| 'poke'	// on l'a touché
	| 'love'	// like envoyé
	| 'joy'		// match
	| 'sad'		// erreur
	| 'startle'	// surprise (message reçu, réveil)
	| 'nod'		// succès, validation
	| 'tick';	// frappe au clavier

/**
 * Pose calculée à chaque frame par le contrôleur et lue par les vues.
 * L'objet est muté sur place : les vues gardent la même référence.
 */
export interface EntityPose
{
	offsetY:	number;
	rotationX:	number;
	rotationY:	number;
	rotationZ:	number;
	scaleX:		number;
	scaleY:		number;
	scaleZ:		number;

	eyeYaw:		number;
	eyePitch:	number;
	eyeScale:	number;
	/** Ouverture des paupières : 0 = fermée, 1 = ouverte. */
	upperLid:	number;
	lowerLid:	number;

	/** Contraction du battement de cœur (0 = repos). */
	beat:		number;
	/** Rougissement (0..1). */
	blush:		number;
	mouth:		MouthStyle;

	/** Incrémenté à chaque gerbe de petits cœurs. */
	burstCount:	number;
	burstSize:	number;
}

export class LivingEntityModel
{
	readonly pose: EntityPose = {
		offsetY: 0,
		rotationX: 0,
		rotationY: 0,
		rotationZ: 0,
		scaleX: 1,
		scaleY: 1,
		scaleZ: 1,
		eyeYaw: 0,
		eyePitch: 0,
		eyeScale: 1,
		upperLid: 1,
		lowerLid: 1,
		beat: 0,
		blush: 0,
		mouth: 'smile',
		burstCount: 0,
		burstSize: 0,
	};

	private _mood: EntityMood = 'awake';

	get mood(): EntityMood
	{
		return this._mood;
	}

	setMood(mood: EntityMood): void
	{
		this._mood = mood;
	}
}
