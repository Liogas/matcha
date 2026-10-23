export type EntityState = 'idle' | 'curious' | 'excited';

export interface EntityModelState
{
	positionY:	number;

	rotationY:	number;
	rotationX:	number;

	scale:		number;
	state:		EntityState;

	gazeY:		number;
	gazeX:		number;
}

export class LivingEntityModel
{
	private _state: EntityModelState = {
		positionY: 0,
		rotationY: 0,
		rotationX: 0,
		scale: 1,
		state: 'idle',
		gazeY: 0,
		gazeX: 0
	}

	get state(): Readonly<EntityModelState>
	{
		return this._state;
	}

	setState(state: EntityState): void
	{
		this._state.state = state;
	}

	updatePositionY(posY: number): void
	{
		this._state.positionY = posY;
	}

	updateRotation(x: number, y: number): void
	{
		this._state.rotationY = y;
		this._state.rotationX = x;
	}

	updateScale(value: number): void
	{
		this._state.scale = value;
	}

	updateGaze(x: number, y: number)
	{
		this._state.gazeX = x;
		this._state.gazeY = y;
	}
}