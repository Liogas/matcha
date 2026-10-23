export type EyeStyle = 'round' | 'almond' | 'narrow';
export type MouthStyle = 'smile' | 'open' | 'smirk';
export type BodyShape = 'classic' | 'round' | 'slim';
export type AccessoryStyle = 'none' | 'halo' | 'orbit';

export interface LivingEntityAppearance {
	bodyColor:		string;
	secondaryColor:	string;
	eyeColor:		string;
	eyeStyle:		EyeStyle;
	mouthStyle:		MouthStyle | null;
	bodyShape:		BodyShape;
	accessory:		AccessoryStyle;
}

export const DEFAULT_APPEARANCE: LivingEntityAppearance = {
	bodyColor:		'#f06b91',
	secondaryColor:	'#8b70f5',
	eyeColor:		'#29946e',
	eyeStyle:		'round',
	mouthStyle:		null,
	bodyShape:		'classic',
	accessory:		'orbit',
};