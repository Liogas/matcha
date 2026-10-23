import { LivingEntityModel } from "./LivingEntityModel.ts";

export interface EntityFrame
{
	time:		number;
	delta:		number;
	pointerX:	number;
	pointerY:	number;
}

export class LivingEntityController
{
	private readonly model: LivingEntityModel;
	constructor(model : LivingEntityModel)
	{
		this.model = model;
	}

	update(frame: EntityFrame): void
	{
		const {time, pointerX, pointerY} = frame;
		this.model.updatePositionY(Math.sin(time * 1.5) * 0.12);
		this.model.updateRotation(
			-pointerY * 0.3,
			pointerX * 0.45
		);
		this.model.updateScale(1 + Math.sin(time * 2) * 0.035);
		this.model.updateGaze(
			Math.max(-0.05, Math.min(0.05, pointerX * 0.05)),
			Math.max(-0.08, Math.min(0.08, pointerY * 0.08))
		);
	}

	toggleState(): void
	{
		const currentState = this.model.state.state;
		const nextState = currentState === 'idle'
			? 'curious'
			: currentState === 'curious'
			? 'excited'
			: 'idle';
		console.log("State", nextState);
		this.model.setState(nextState);
	}
}