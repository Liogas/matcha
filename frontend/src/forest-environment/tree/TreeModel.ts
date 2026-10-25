import { Color, Quaternion, Vector3 } from 'three'
import { TREE } from '../core/composition'
import { DEG } from '../core/math'
import { Random } from '../core/random'

export interface BranchSegment {
  start: Vector3
  end: Vector3
  radiusStart: number
  radiusEnd: number
}

export interface FoliageCluster {
  /** Position de repos. */
  readonly anchor: Vector3
  /** Position courante, balancée par le vent. */
  readonly position: Vector3
  readonly size: number
  /** Le bouquet n'est présent que si la densité du feuillage dépasse ce seuil. */
  readonly threshold: number
  /** Avance ou retard de ce bouquet sur le calendrier des couleurs. */
  readonly lateness: number
  readonly shade: number
  readonly phase: number
  /** Échelle courante (0 = tombé). */
  scale: number
  readonly color: Color
}

const MAX_DEPTH = 4
const UP = new Vector3(0, 1, 0)

/**
 * Le grand chêne : sa charpente est générée une fois pour toutes (graine fixe),
 * seul son feuillage évolue avec les saisons.
 */
export class TreeModel {
  readonly branches: BranchSegment[] = []
  readonly roots: BranchSegment[] = []
  readonly clusters: FoliageCluster[] = []
  /** Densité et teinte courantes du feuillage. */
  foliage = 1
  foliageStage = 1
  private readonly random = new Random(1789)

  constructor(groundHeight: number) {
    const base = new Vector3(TREE.x, groundHeight - 0.4, TREE.z)
    this.grow(base, new Vector3(0.12, 1, 0.05).normalize(), 5.6, 0.78, 0)
    this.growRoots(base.clone().setY(groundHeight + 0.35))
  }

  /** Bouquets de feuillage visibles : de quoi faire tomber des feuilles. */
  get leafyClusters() {
    return this.clusters.filter((cluster) => cluster.scale > 0.2)
  }

  private grow(start: Vector3, direction: Vector3, length: number, radius: number, depth: number) {
    const random = this.random
    const pieces = depth === 0 ? 4 : 3
    const heading = direction.clone()
    let point = start.clone()
    let currentRadius = radius

    for (let piece = 0; piece < pieces; piece += 1) {
      // Une branche n'est jamais droite : elle cherche la lumière en ondulant
      heading.add(new Vector3(random.range(-0.22, 0.22), random.range(-0.05, 0.12), random.range(-0.22, 0.22)))
      if (depth > 1) heading.y -= 0.04 * depth
      heading.normalize()
      const next = point.clone().addScaledVector(heading, length / pieces)
      const nextRadius = radius * (1 - ((piece + 1) / pieces) * 0.32)
      this.branches.push({ start: point, end: next, radiusStart: currentRadius, radiusEnd: nextRadius })
      point = next
      currentRadius = nextRadius
    }

    if (depth >= MAX_DEPTH) {
      this.addCluster(point, random.range(0.85, 1.25))
      this.addTwigs(point, heading, currentRadius)
      return
    }
    if (depth >= MAX_DEPTH - 1)
      this.addCluster(point.clone().addScaledVector(heading, -length * 0.3), random.range(0.9, 1.3))

    const children = depth === 0 ? 3 : random.chance(0.55) ? 2 : 3
    const offset = random.range(0, Math.PI * 2)
    for (let child = 0; child < children; child += 1) {
      const tilt = (depth === 0 ? random.range(38, 58) : random.range(24, 46)) * DEG
      const azimuth = offset + (child / children) * Math.PI * 2 + random.range(-0.4, 0.4)
      const childDirection = this.deviate(heading, tilt, azimuth)
      this.grow(point, childDirection, length * random.range(0.6, 0.76), currentRadius * 0.74, depth + 1)
    }
  }

  /** Brindilles nues : elles dessinent la silhouette de l'arbre en hiver. */
  private addTwigs(point: Vector3, heading: Vector3, radius: number) {
    for (let twig = 0; twig < 3; twig += 1) {
      const direction = this.deviate(heading, this.random.range(25, 55) * DEG, this.random.range(0, Math.PI * 2))
      const end = point.clone().addScaledVector(direction, this.random.range(0.6, 1.1))
      this.branches.push({ start: point, end, radiusStart: radius * 0.7, radiusEnd: 0.012 })
    }
  }

  private addCluster(point: Vector3, size: number) {
    const random = this.random
    this.clusters.push({
      anchor: point.clone(),
      position: point.clone(),
      size,
      threshold: random.range(0.05, 0.9),
      lateness: random.range(-0.35, 0.35),
      shade: random.range(0.82, 1.12),
      phase: random.range(0, Math.PI * 2),
      scale: 1,
      color: new Color(),
    })
  }

  private growRoots(base: Vector3) {
    const random = this.random
    for (let root = 0; root < 7; root += 1) {
      const angle = (root / 7) * Math.PI * 2 + random.range(-0.3, 0.3)
      const reach = random.range(1.8, 2.8)
      const middle = base.clone().add(new Vector3(Math.cos(angle) * reach * 0.45, 0.05, Math.sin(angle) * reach * 0.45))
      const end = base.clone().add(new Vector3(Math.cos(angle) * reach, -0.75, Math.sin(angle) * reach))
      const thickness = random.range(0.26, 0.38)
      this.roots.push(
        { start: base.clone().setY(base.y + 0.5), end: middle, radiusStart: thickness, radiusEnd: thickness * 0.6 },
        { start: middle, end, radiusStart: thickness * 0.6, radiusEnd: 0.05 },
      )
    }
  }

  /** Direction écartée de `heading` d'un angle `tilt`, tournée de `azimuth` autour de lui. */
  private deviate(heading: Vector3, tilt: number, azimuth: number) {
    const side = new Vector3().crossVectors(heading, Math.abs(heading.y) > 0.95 ? new Vector3(1, 0, 0) : UP).normalize()
    return heading.clone()
      .applyQuaternion(new Quaternion().setFromAxisAngle(side, tilt))
      .applyQuaternion(new Quaternion().setFromAxisAngle(heading, azimuth))
      .normalize()
  }
}
