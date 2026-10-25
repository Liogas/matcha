import { PLAY_AREA } from '../core/composition'
import { angleDelta, clamp, damp, smoothstep } from '../core/math'
import { Random } from '../core/random'
import type { ForestController, FrameContext } from '../core/types'
import type { DenModel } from '../den/DenModel'
import type { TerrainModel } from '../terrain/TerrainModel'
import type { WorldModel } from '../world/WorldModel'
import { REACTION_DURATION, type CubActivity, type CubPose, type CubReaction, type WolfCubModel } from './WolfCubModel'
import type { WolfCubView } from './WolfCubView'

const WALK_SPEED = 0.85
const TURN_RATE = 3.2

/** Ce que le louveteau fait de lui-même quand il est réveillé, avec son poids et sa durée. */
const DAY_ACTIVITIES: { activity: CubActivity; weight: number; duration: [number, number] }[] = [
  { activity: 'idle', weight: 3, duration: [4, 8] },
  { activity: 'sit', weight: 2.6, duration: [7, 15] },
  { activity: 'walk', weight: 2.4, duration: [6, 12] },
  { activity: 'sniff', weight: 1.4, duration: [2.5, 4.5] },
  { activity: 'lie', weight: 1.2, duration: [12, 22] },
  { activity: 'play', weight: 1, duration: [2.5, 3.5] },
]

/**
 * Le comportement du louveteau : une petite machine à états qui choisit
 * ses activités selon l'heure, se laisse interrompre par les réactions
 * et calcule une posture cible. La vue ne fait qu'afficher cette posture.
 */
export class WolfCubController implements ForestController {
  private readonly random = new Random(8)

  private readonly model: WolfCubModel
  private readonly view: WolfCubView
  private readonly world: WorldModel
  private readonly terrain: TerrainModel
  private readonly den: DenModel

  constructor(
    model: WolfCubModel,
    view: WolfCubView,
    world: WorldModel,
    terrain: TerrainModel,
    den: DenModel,
  ) {
    this.model = model
    this.view = view
    this.world = world
    this.terrain = terrain
    this.den = den
    const startsAsleep = world.daylight < 0.12
    model.position.copy(startsAsleep ? den.doorstep : this.randomSpot())
    model.heading = startsAsleep ? den.facing + 0.6 : 0.3
    this.begin(startsAsleep ? 'sleep' : 'sit', 8)
  }

  react(reaction: CubReaction) {
    const { model } = this
    if (model.activity === 'sleep' && reaction !== 'joy' && reaction !== 'startle' && reaction !== 'pet') {
      // Un louveteau qui dort ne se réveille pas pour si peu : il remue juste une oreille
      model.earFlick = 1
      return
    }
    if (model.activity === 'sleep' || model.activity === 'lie')
      this.begin('idle', 5)
    model.reaction = reaction
    model.reactionTime = 0
  }

  /** Frappe au clavier : une oreille se tourne vers le bruit. */
  notice() {
    this.model.earFlick = Math.max(this.model.earFlick, 0.6)
  }

  setPolite(polite: boolean) {
    this.model.polite = polite
  }

  update(frame: FrameContext) {
    const { model } = this
    const delta = frame.delta
    if (delta > 0) {
      this.decide(delta)
      this.move(delta)
      this.updateReaction(delta)
      this.composePose(frame)
      this.animateCycles(delta)
      this.blendPose(delta)
    }
    model.position.y = this.terrain.heightAt(model.position.x, model.position.z)
    this.view.sync(model)
  }

  dispose() {
    this.view.dispose()
  }

  private begin(activity: CubActivity, duration: number) {
    const { model } = this
    model.activity = activity
    model.activityTime = 0
    model.activityDuration = duration
    if (activity === 'walk') model.destination.copy(this.randomSpot())
  }

  private decide(delta: number) {
    const { model, world, random } = this
    model.activityTime += delta
    const night = world.daylight < 0.12

    if (night && model.activity !== 'sleep' && model.activity !== 'howl' && !model.reaction) {
      // La nuit tombe : retour à la tanière pour dormir
      const home = model.position.distanceTo(this.den.doorstep)
      if (home > 0.35) {
        if (model.activity !== 'walk' || model.destination.distanceTo(this.den.doorstep) > 0.01) {
          this.begin('walk', 60)
          model.destination.copy(this.den.doorstep)
        }
      } else {
        this.begin('sleep', 60)
      }
      return
    }

    if (model.activity === 'sleep') {
      if (!night) {
        this.begin('idle', 6)
        return
      }
      // Pleine lune haute : il arrive qu'il se redresse pour hurler
      const fullMoon = world.moonIllumination > 0.85 && world.moonAltitude > 12
      if (fullMoon && random.chance(delta / 240)) this.begin('howl', 5)
      return
    }
    if (model.activity === 'howl' && model.activityTime > model.activityDuration) {
      this.begin('sleep', 60)
      return
    }

    const arrived = model.activity === 'walk' && model.position.distanceTo(model.destination) < 0.2
    if (model.activityTime > model.activityDuration || arrived) {
      const total = DAY_ACTIVITIES.reduce((sum, item) => sum + item.weight, 0)
      let roll = random.next() * total
      const next = DAY_ACTIVITIES.find((item) => (roll -= item.weight) < 0) ?? DAY_ACTIVITIES[0]
      this.begin(next.activity, random.range(...next.duration))
    }
  }

  private move(delta: number) {
    const { model } = this
    if (model.activity !== 'walk' || model.reaction) return
    const dx = model.destination.x - model.position.x
    const dz = model.destination.z - model.position.z
    const desired = Math.atan2(dx, dz)
    model.heading += angleDelta(model.heading, desired) * Math.min(TURN_RATE * delta, 1)
    // Il ralentit pour tourner, comme un vrai petit animal
    const alignment = Math.max(Math.cos(angleDelta(model.heading, desired)), 0)
    const step = Math.min(WALK_SPEED * delta * alignment, Math.hypot(dx, dz))
    model.position.x += Math.sin(model.heading) * step
    model.position.z += Math.cos(model.heading) * step
  }

  private updateReaction(delta: number) {
    const { model } = this
    model.earFlick = Math.max(model.earFlick - delta * 1.8, 0)
    if (!model.reaction) return
    model.reactionTime += delta
    if (model.reactionTime > REACTION_DURATION[model.reaction]) model.reaction = null
  }

  /** Posture visée selon l'activité, puis corrigée par la réaction en cours et le regard. */
  private composePose(frame: FrameContext) {
    const { model } = this
    const pose = model.targetPose
    Object.assign(pose, {
      sit: 0, lie: 0, curl: 0, headYaw: 0, headPitch: 0, headTilt: 0, ears: 0.9,
      tailWag: 0.18, tailLift: 0.15, stride: 0, hop: 0, blink: 0, mouth: 0,
    } satisfies CubPose)

    const t = model.activityTime
    switch (model.activity) {
      case 'sit':
        pose.sit = 1
        pose.tailLift = -0.3
        pose.tailWag = 0.08
        break
      case 'walk':
        pose.stride = 1
        pose.ears = 0.8
        pose.tailLift = 0.3
        break
      case 'sniff':
        pose.headPitch = 0.75 + Math.sin(t * 6) * 0.05
        pose.ears = 0.7
        pose.tailWag = 0.35
        pose.tailLift = 0.45
        break
      case 'lie':
        pose.lie = 1
        pose.ears = 0.7
        pose.tailLift = -0.5
        break
      case 'sleep':
        pose.lie = 1
        pose.curl = 1
        pose.blink = 1
        pose.ears = 0.25
        pose.tailWag = 0
        pose.tailLift = -0.6
        break
      case 'howl':
        pose.sit = 1
        pose.headPitch = -0.95
        pose.mouth = t > 0.6 && t < 4.4 ? 0.75 + Math.sin(t * 3) * 0.15 : 0
        pose.blink = 0.6
        pose.ears = 0.55
        pose.tailLift = -0.4
        break
      case 'play': {
        // Petits bonds de jeu, l'avant-train baissé
        const bounce = Math.max(Math.sin(t * 6.5), 0)
        pose.hop = bounce * 0.16
        pose.headPitch = 0.3
        pose.tailWag = 1
        pose.tailLift = 0.7
        pose.mouth = 0.25
        break
      }
      default:
        break
    }

    // Regard : vers la personne devant l'écran, en suivant un peu le pointeur
    const awake = model.activity !== 'sleep'
    if (awake && model.activity !== 'howl' && model.activity !== 'sniff') {
      const camera = frame.camera.position
      const towardCamera = angleDelta(model.heading, Math.atan2(camera.x - model.position.x, camera.z - model.position.z))
      let yaw = clamp(towardCamera + frame.pointer.x * 0.35, -1.1, 1.1)
      if (model.polite) yaw = towardCamera > 0 ? -1 : 1
      else if (model.glanceYaw !== 0) yaw = model.glanceYaw
      pose.headYaw = yaw
      pose.headPitch += model.polite ? 0.15 : -frame.pointer.y * 0.18
    }
    if (model.polite) pose.ears = Math.min(pose.ears, 0.55)

    // Réactions : elles priment sur l'activité le temps de leur durée
    const reactionTime = model.reactionTime
    switch (model.reaction) {
      case 'joy':
        pose.hop = Math.max(Math.sin(reactionTime * 7.5), 0) * 0.3 * smoothstep(2.3, 1.4, reactionTime)
        pose.tailWag = 1
        pose.tailLift = 0.8
        pose.ears = 1
        pose.mouth = 0.45
        pose.sit = 0
        pose.lie = 0
        break
      case 'love':
        pose.headTilt = 0.35
        pose.tailWag = 0.9
        pose.ears = 0.55
        pose.blink = 0.55
        break
      case 'startle':
        pose.ears = 1
        pose.hop = reactionTime < 0.3 ? Math.sin((reactionTime / 0.3) * Math.PI) * 0.12 : 0
        pose.tailLift = 0.6
        pose.tailWag = 0
        pose.headPitch -= 0.15
        break
      case 'sad':
        pose.ears = 0
        pose.headPitch = 0.45
        pose.headYaw *= 0.3
        pose.tailLift = -0.9
        pose.tailWag = 0
        break
      case 'nod':
        pose.headPitch = Math.sin(reactionTime * 10) * 0.28
        pose.ears = 0.95
        break
      case 'pet':
        pose.headTilt = Math.sin(reactionTime * 3) * 0.3
        pose.blink = 0.7
        pose.tailWag = 1
        pose.mouth = 0.3
        pose.ears = 0.45
        break
      default:
        break
    }
    // Clignement naturel, sauf yeux déjà fermés
    if (model.blinkTimer < 0.13) pose.blink = Math.max(pose.blink, 1)
  }

  private animateCycles(delta: number) {
    const { model, random } = this
    model.gaitPhase += delta * 8.5 * model.pose.stride
    model.breathPhase += delta * (model.activity === 'sleep' ? 1.5 : model.pose.mouth > 0.2 ? 6 : 2.6)
    model.wagPhase += delta * (6 + model.pose.tailWag * 9)
    model.blinkTimer -= delta
    if (model.blinkTimer < 0) model.blinkTimer = random.range(2.5, 6.5)
    // De temps à autre, un coup d'œil ailleurs
    model.glanceTimer -= delta
    if (model.glanceTimer < 0) {
      model.glanceYaw = model.glanceYaw === 0 ? random.range(-1, 1) : 0
      model.glanceTimer = model.glanceYaw === 0 ? random.range(4, 9) : random.range(1, 2)
    }
  }

  private blendPose(delta: number) {
    const { pose, targetPose } = this.model
    for (const key of Object.keys(pose) as (keyof CubPose)[]) {
      // Les clignements et les bonds sont vifs, le reste se pose en douceur
      const rate = key === 'blink' || key === 'hop' || key === 'mouth' ? 22 : key === 'headPitch' || key === 'headYaw' ? 5 : 3.2
      pose[key] = damp(pose[key], targetPose[key], rate, delta)
    }
  }

  private randomSpot() {
    const angle = this.random.range(0, Math.PI * 2)
    const radius = Math.sqrt(this.random.next()) * PLAY_AREA.radius
    const x = PLAY_AREA.x + Math.cos(angle) * radius
    const z = PLAY_AREA.z + Math.sin(angle) * radius
    return this.model.destination.clone().set(x, this.terrain.heightAt(x, z), z)
  }
}
