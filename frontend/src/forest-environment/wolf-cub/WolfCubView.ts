import {
  BufferGeometry,
  ConeGeometry,
  CylinderGeometry,
  Group,
  IcosahedronGeometry,
  Mesh,
  MeshLambertMaterial,
  type Material,
} from 'three'
import { facet, jitter } from '../core/geometry'
import type { WolfCubModel } from './WolfCubModel'

/** Le louveteau est un peu plus grand que nature : il doit se voir depuis la caméra. */
const CUB_SCALE = 1.55
const BODY_HEIGHT = 0.36

interface Leg {
  pivot: Group
  front: boolean
  phase: number
}

/**
 * Le louveteau en volumes simples : chaque partie mobile est suspendue
 * à un pivot (tête, oreilles, mâchoire, pattes, queue en trois segments).
 */
export class WolfCubView {
  readonly root = new Group()
  private readonly body = new Group()
  private readonly torso = new Group()
  private readonly head = new Group()
  private readonly jaw = new Group()
  private readonly ears: Group[] = []
  private readonly eyes: Mesh[] = []
  private readonly tail: Group[] = []
  private readonly legs: Leg[] = []
  private readonly geometries: BufferGeometry[] = []
  private readonly materials: Material[] = []

  constructor() {
    const fur = this.material('#857563')
    const furDark = this.material('#5b4e43')
    const cream = this.material('#d8cab0')
    const black = this.material('#16110d')
    const shine = this.material('#f4f0e6')

    this.root.scale.setScalar(CUB_SCALE)
    this.root.add(this.body)
    this.body.position.y = BODY_HEIGHT
    this.body.add(this.torso)

    // Corps : un ovoïde allongé, poitrail clair
    this.torso.add(this.part(this.blob(0.2, 0.19, 0.33, 1), fur))
    this.torso.add(this.part(this.blob(0.14, 0.13, 0.17, 2), cream, 0, -0.06, 0.15))
    this.torso.add(this.part(this.blob(0.12, 0.08, 0.2, 3), furDark, 0, 0.13, -0.04))

    // Tête
    this.head.position.set(0, 0.1, 0.27)
    this.head.rotation.order = 'YXZ'
    this.torso.add(this.head)
    this.head.add(this.part(this.blob(0.16, 0.15, 0.155, 4), fur, 0, 0.08, 0.06))
    this.head.add(this.part(this.blob(0.075, 0.06, 0.06, 5), cream, 0.085, 0.03, 0.1))
    this.head.add(this.part(this.blob(0.075, 0.06, 0.06, 6), cream, -0.085, 0.03, 0.1))
    const muzzle = this.part(new CylinderGeometry(0.045, 0.075, 0.17, 6), cream, 0, 0.045, 0.2)
    muzzle.rotation.x = Math.PI / 2
    this.head.add(muzzle)
    this.head.add(this.part(this.blob(0.03, 0.022, 0.025, 7), black, 0, 0.07, 0.29))
    this.jaw.position.set(0, 0.0, 0.13)
    this.head.add(this.jaw)
    const jaw = this.part(this.blob(0.04, 0.018, 0.075, 8), cream, 0, 0, 0.06)
    this.jaw.add(jaw)

    for (const side of [-1, 1]) {
      const eye = this.part(this.blob(0.022, 0.024, 0.016, 9), black, side * 0.068, 0.115, 0.175)
      const glint = this.part(this.blob(0.007, 0.007, 0.004, 10), shine, side * 0.004 + 0.006, 0.008, 0.013)
      eye.add(glint)
      this.eyes.push(eye)
      this.head.add(eye)

      const ear = new Group()
      ear.position.set(side * 0.085, 0.19, 0.02)
      ear.rotation.z = -side * 0.28
      const shell = this.part(new ConeGeometry(0.052, 0.13, 4), furDark, 0, 0.065, 0)
      shell.rotation.y = Math.PI / 4
      ear.add(shell)
      this.ears.push(ear)
      this.head.add(ear)
    }

    // Queue : trois segments, de plus en plus fins, bout sombre
    let parent: Group = this.torso
    const segments: [number, number, number, Material][] = [
      [0.06, 0.045, 0.15, fur],
      [0.045, 0.035, 0.14, fur],
      [0.035, 0.012, 0.11, furDark],
    ]
    segments.forEach(([radiusStart, radiusEnd, length, material], index) => {
      const joint = new Group()
      joint.position.set(0, index === 0 ? 0.07 : 0, index === 0 ? -0.3 : -segments[index - 1][2])
      const piece = this.part(new CylinderGeometry(radiusEnd, radiusStart, length, 6), material, 0, 0, -length / 2)
      piece.rotation.x = Math.PI / 2
      joint.add(piece)
      parent.add(joint)
      this.tail.push(joint)
      parent = joint
    })

    // Pattes : diagonales en phase (avant gauche avec arrière droite)
    const legs: [number, number, number, boolean, number][] = [
      [0.1, -0.08, 0.19, true, 0],
      [-0.1, -0.08, 0.19, true, Math.PI],
      [0.11, -0.07, -0.19, false, Math.PI],
      [-0.11, -0.07, -0.19, false, 0],
    ]
    for (const [x, y, z, front, phase] of legs) {
      const pivot = new Group()
      pivot.position.set(x, y, z)
      pivot.add(this.part(new CylinderGeometry(0.04, 0.05, 0.24, 6), fur, 0, -0.12, 0))
      pivot.add(this.part(this.blob(0.045, 0.028, 0.06, 11), cream, 0, -0.26, 0.015))
      this.torso.add(pivot)
      this.legs.push({ pivot, front, phase })
    }
  }

  sync(model: WolfCubModel) {
    const { pose } = model
    const sit = pose.sit
    const lie = pose.lie
    const curl = pose.curl
    const breath = Math.sin(model.breathPhase)
    const gait = model.gaitPhase

    this.root.position.set(model.position.x, model.position.y + pose.hop * CUB_SCALE, model.position.z)
    this.root.rotation.y = model.heading + curl * 0.4

    // Corps : redressé pour s'asseoir, allongé au sol, roulé pour dormir
    const pitch = -0.5 * sit * (1 - lie)
    this.body.position.y = BODY_HEIGHT - 0.07 * sit * (1 - lie) - 0.19 * lie + Math.abs(Math.sin(gait)) * 0.012 * pose.stride
    this.torso.rotation.set(pitch, 0, curl * 0.18)
    this.torso.scale.set(1 + breath * 0.02, 1 + breath * 0.025, 1)

    this.head.rotation.set(
      pose.headPitch - pitch + curl * 0.55,
      pose.headYaw + curl * 1.05,
      pose.headTilt,
    )
    this.head.position.y = 0.1 - curl * 0.07
    this.jaw.rotation.x = pose.mouth * 0.45

    this.ears.forEach((ear, index) => {
      const flick = model.earFlick * Math.sin(model.earFlick * 30) * (index === 0 ? 0.5 : 0.15)
      ear.rotation.x = -(1 - pose.ears) * 1.05 + flick
    })
    this.eyes.forEach((eye) => {
      eye.scale.y = 1 - pose.blink * 0.88
    })

    // Queue : levée ou basse, et qui remue en ondulant d'un segment à l'autre
    this.tail.forEach((joint, index) => {
      const lift = index === 0 ? 0.25 + pose.tailLift * 0.65 + lie * -0.25 : pose.tailLift * 0.15 - 0.1
      joint.rotation.x = lift
      joint.rotation.y = Math.sin(model.wagPhase - index * 0.9) * pose.tailWag * (0.45 + index * 0.1) + curl * 0.75
    })

    for (const leg of this.legs) {
      const swing = Math.sin(gait + leg.phase) * 0.55 * pose.stride
      if (leg.front) {
        leg.pivot.rotation.x = swing - pitch - lie * 1.35
        leg.pivot.scale.y = 1 + sit * 0.12
      } else {
        leg.pivot.rotation.x = swing - pitch * 0.2 - sit * 0.9 * (1 - lie) + lie * 1.3
        leg.pivot.scale.y = 1
      }
    }
  }

  dispose() {
    this.geometries.forEach((geometry) => geometry.dispose())
    this.materials.forEach((material) => material.dispose())
  }

  private material(color: string) {
    const material = new MeshLambertMaterial({ color, flatShading: true })
    this.materials.push(material)
    return material
  }

  private blob(x: number, y: number, z: number, seed: number) {
    const geometry = jitter(new IcosahedronGeometry(1, 1), 0.12, seed)
    geometry.scale(x, y, z)
    return geometry
  }

  private part(geometry: BufferGeometry, material: Material, x = 0, y = 0, z = 0) {
    const faceted = facet(geometry)
    this.geometries.push(faceted)
    const mesh = new Mesh(faceted, material)
    mesh.position.set(x, y, z)
    mesh.castShadow = true
    mesh.receiveShadow = true
    return mesh
  }
}
