import { Vector3 } from 'three'

/** Réactions déclenchées par l'application (like, match, message, erreur…). */
export type CubReaction = 'joy' | 'love' | 'startle' | 'sad' | 'nod' | 'pet'

export type CubActivity = 'idle' | 'sit' | 'walk' | 'sniff' | 'lie' | 'sleep' | 'howl' | 'play'

/** Posture du louveteau : des valeurs continues que la vue traduit en articulations. */
export interface CubPose {
  sit: number
  lie: number
  /** Roulé en boule pour dormir. */
  curl: number
  headYaw: number
  headPitch: number
  headTilt: number
  /** 0 = oreilles couchées, 1 = dressées. */
  ears: number
  tailWag: number
  /** -1 = queue basse, 1 = queue haute. */
  tailLift: number
  stride: number
  hop: number
  blink: number
  mouth: number
}

export function createPose(): CubPose {
  return {
    sit: 0,
    lie: 0,
    curl: 0,
    headYaw: 0,
    headPitch: 0,
    headTilt: 0,
    ears: 0.9,
    tailWag: 0.2,
    tailLift: 0.2,
    stride: 0,
    hop: 0,
    blink: 0,
    mouth: 0,
  }
}

export const REACTION_DURATION: Record<CubReaction, number> = {
  joy: 2.3,
  love: 2.6,
  startle: 1.6,
  sad: 3.2,
  nod: 1.3,
  pet: 2.2,
}

/** État du louveteau : où il est, ce qu'il fait, comment il se tient. */
export class WolfCubModel {
  readonly position = new Vector3()
  heading = 0
  activity: CubActivity = 'idle'
  activityTime = 0
  activityDuration = 4
  readonly destination = new Vector3()

  reaction: CubReaction | null = null
  reactionTime = 0
  /** Petit frémissement d'oreille (bruit, frappe au clavier) pendant le sommeil. */
  earFlick = 0
  /** Le champ mot de passe est actif : il regarde ailleurs, par discrétion. */
  polite = false

  /** Posture affichée, qui rejoint doucement la posture visée. */
  readonly pose: CubPose = createPose()
  readonly targetPose: CubPose = createPose()

  gaitPhase = 0
  breathPhase = 0
  wagPhase = 0
  blinkTimer = 3
  glanceTimer = 4
  glanceYaw = 0
}
