import      { useEffect, useMemo, useRef }  from 'react';
import      { ExtrudeGeometry, Shape }     from 'three';
import type { Mesh }                       from 'three';
import      { useFrame }                   from '@react-three/fiber';
import      { mergeVertices }              from 'three/examples/jsm/utils/BufferGeometryUtils.js';
import type { LivingEntityAppearance }     from './LivingEntityAppearance.ts';
 
interface HeartBodyViewProps {
    appearance: LivingEntityAppearance;
    /** Fait battre le cœur (désactivé par défaut). */
    heartbeat?: boolean;
}
 
/**
 * Cœur simple et symétrique : même silhouette que l'original,
 * avec la pointe du bas légèrement arrondie pour éviter les artefacts de bevel.
 */
function createHeartShape(): Shape {
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
 
export default function HeartBodyView({
    appearance,
    heartbeat = false,
}: HeartBodyViewProps) {
    const meshRef = useRef<Mesh>(null);
 
    const geometry = useMemo(() => {
        const extruded = new ExtrudeGeometry(createHeartShape(), {
            depth: 0.3,
            bevelEnabled: true,
            bevelThickness: 0.12,
            bevelSize: 0.08,
            bevelSegments: 8,
            curveSegments: 32,
            steps: 1,
        });
 
        // ExtrudeGeometry n'est pas indexée, donc ses normales sont plates (aspect facetté).
        // On soude les sommets pour obtenir un ombrage lisse.
        extruded.deleteAttribute('normal');
        extruded.deleteAttribute('uv');
        const smooth = mergeVertices(extruded, 1e-4);
        smooth.computeVertexNormals();
        extruded.dispose();
 
        return smooth;
    }, []);
 
    useEffect(() => () => geometry.dispose(), [geometry]);
 
    useFrame(({ clock }) => {
        const mesh = meshRef.current;
        if (!mesh)
            return;
 
        if (!heartbeat) {
            mesh.scale.setScalar(1);
            return;
        }
 
        const period = 1.1;
        const t      = (clock.elapsedTime % period) / period;
        const pulse  = (center: number, width: number) =>
            Math.exp(-((t - center) ** 2) / (2 * width ** 2));
 
        mesh.scale.setScalar(1 + 0.07 * pulse(0.08, 0.035) + 0.045 * pulse(0.26, 0.04));
    });
 
    return (
        <mesh ref={meshRef} geometry={geometry} position={[0, -0.05, 0]}>
            <meshStandardMaterial
                color={appearance.bodyColor}
                roughness={0.35}
                metalness={0.05}
            />
        </mesh>
    );
}
 