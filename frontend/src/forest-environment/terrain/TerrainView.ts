import {
  BufferAttribute,
  BufferGeometry,
  Color,
  Mesh,
  MeshLambertMaterial,
} from 'three'
import { applyEffects, type SharedUniforms } from '../core/materialEffects'
import { hash01 } from '../core/random'
import type { ForestQuality } from '../core/types'
import { TERRAIN_BOUNDS, type GroundWeights, type TerrainModel } from './TerrainModel'

const WEIGHTS_PER_FACE = 5

/**
 * Le sol en facettes. Chaque facette garde la composition de son sol
 * (herbe, terre, roche…) : changer de saison ne fait que repeindre.
 */
export class TerrainView {
  readonly root: Mesh<BufferGeometry, MeshLambertMaterial>
  private readonly weights: Float32Array
  private readonly shades: Float32Array
  private readonly colors: Float32Array

  constructor(model: TerrainModel, shared: SharedUniforms, quality: ForestQuality) {
    const columns = quality === 'high' ? 140 : 90
    const rows = quality === 'high' ? 110 : 70
    const { minX, maxX, minZ, maxZ } = TERRAIN_BOUNDS
    const stepX = (maxX - minX) / columns
    const stepZ = (maxZ - minZ) / rows

    const heights = new Float32Array((columns + 1) * (rows + 1))
    for (let row = 0; row <= rows; row += 1)
      for (let column = 0; column <= columns; column += 1)
        heights[row * (columns + 1) + column] = model.heightAt(minX + column * stepX, minZ + row * stepZ)

    const faces = columns * rows * 2
    const positions = new Float32Array(faces * 9)
    let cursor = 0
    const vertex = (column: number, row: number) => {
      positions[cursor++] = minX + column * stepX
      positions[cursor++] = heights[row * (columns + 1) + column]
      positions[cursor++] = minZ + row * stepZ
    }
    for (let row = 0; row < rows; row += 1) {
      for (let column = 0; column < columns; column += 1) {
        // Diagonale alternée : évite l'effet de grille régulière
        if ((row + column) % 2 === 0) {
          vertex(column, row); vertex(column, row + 1); vertex(column + 1, row)
          vertex(column + 1, row); vertex(column, row + 1); vertex(column + 1, row + 1)
        } else {
          vertex(column, row); vertex(column, row + 1); vertex(column + 1, row + 1)
          vertex(column, row); vertex(column + 1, row + 1); vertex(column + 1, row)
        }
      }
    }

    const geometry = new BufferGeometry()
    geometry.setAttribute('position', new BufferAttribute(positions, 3))
    geometry.computeVertexNormals()
    this.colors = new Float32Array(faces * 9)
    geometry.setAttribute('color', new BufferAttribute(this.colors, 3))

    // Composition du sol au centre de chaque facette
    this.weights = new Float32Array(faces * WEIGHTS_PER_FACE)
    this.shades = new Float32Array(faces)
    const normals = geometry.getAttribute('normal') as BufferAttribute
    const sample: GroundWeights = { grass: 1, dirt: 0, rock: 0, forest: 0, litter: 0 }
    for (let face = 0; face < faces; face += 1) {
      const base = face * 9
      const x = (positions[base] + positions[base + 3] + positions[base + 6]) / 3
      const z = (positions[base + 2] + positions[base + 5] + positions[base + 8]) / 3
      const slope = 1 - normals.getY(face * 3)
      model.groundAt(x, z, slope * 2.2, sample)
      this.weights.set([sample.grass, sample.dirt, sample.rock, sample.forest, sample.litter], face * WEIGHTS_PER_FACE)
      this.shades[face] = 0.9 + hash01(face) * 0.2
    }

    const material = new MeshLambertMaterial({ vertexColors: true, flatShading: true })
    applyEffects(material, shared, { snow: true, snowSlope: 0.6 })
    this.root = new Mesh(geometry, material)
    this.root.receiveShadow = true
    this.repaint(model)
  }

  repaint(model: TerrainModel) {
    const color = new Color()
    const { weights, shades, colors } = this
    const faces = shades.length
    for (let face = 0; face < faces; face += 1) {
      const w = face * WEIGHTS_PER_FACE
      color.copy(model.grass)
        .lerp(model.forestFloor, weights[w + 3])
        .lerp(model.litter, weights[w + 4] * model.litterAmount)
        .lerp(model.dirt, weights[w + 1])
        .lerp(model.rock, weights[w + 2])
        .multiplyScalar(shades[face])
      for (let corner = 0; corner < 3; corner += 1)
        color.toArray(colors, face * 9 + corner * 3)
    }
    this.root.geometry.getAttribute('color').needsUpdate = true
  }

  dispose() {
    this.root.geometry.dispose()
    this.root.material.dispose()
  }
}
