import { Shape } from 'three';

/**
 * Cœur simple et symétrique : même silhouette que l'original,
 * avec la pointe du bas légèrement arrondie pour éviter les artefacts de bevel.
 */
export function createHeartShape(): Shape {
    const shape = new Shape();
 
    // Côté droit : de la pointe jusqu'au creux
    shape.moveTo(0.05, -0.76);
    shape.bezierCurveTo(0.25, -0.55, 0.95, -0.05, 0.85, 0.5);
    shape.bezierCurveTo(0.78, 1.0, 0.2, 1.0, 0, 0.55);
 
    // Côté gauche : du creux jusqu'à la pointe
    shape.bezierCurveTo(-0.2, 1.0, -0.78, 1.0, -0.85, 0.5);
    shape.bezierCurveTo(-0.95, -0.05, -0.25, -0.55, -0.05, -0.76);
 
    // Pointe arrondie
    shape.quadraticCurveTo(0, -0.84, 0.05, -0.76);
    shape.closePath();
 
    return shape;
}
