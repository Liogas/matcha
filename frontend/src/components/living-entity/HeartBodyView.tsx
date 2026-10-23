import      { useEffect, useMemo, useRef }  from 'react';
import      { Color, ExtrudeGeometry }     from 'three';
import type { Mesh, MeshPhysicalMaterial }  from 'three';
import      { useFrame }                   from '@react-three/fiber';
import      { mergeVertices }              from 'three/examples/jsm/utils/BufferGeometryUtils.js';
import type { LivingEntityAppearance }     from './LivingEntityAppearance.ts';
import type { EntityPose }                 from './LivingEntityModel.ts';
import      { createHeartShape }           from './heartShape.ts';
 
interface HeartBodyViewProps {
    appearance: LivingEntityAppearance;
    /** Pose partagée : battement et rougissement. */
    pose: EntityPose;
}

const BLUSH_COLOR = new Color('#ff1f5a');
 
export default function HeartBodyView({
    appearance,
    pose,
}: HeartBodyViewProps) {
    const meshRef     = useRef<Mesh>(null);
    const materialRef = useRef<MeshPhysicalMaterial>(null);
 
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
 
    useFrame(() => {
        meshRef.current?.scale.setScalar(1 + pose.beat);

        const material = materialRef.current;
        if (material) {
            // Le cœur s'illumine légèrement à chaque contraction et quand il rougit
            material.emissive.copy(BLUSH_COLOR);
            material.emissiveIntensity = pose.blush * 0.22 + pose.beat * 1.2;
        }
    });
 
    return (
        <mesh ref={meshRef} geometry={geometry} position={[0, -0.05, 0]}>
            <meshPhysicalMaterial
                ref={materialRef}
                color={appearance.bodyColor}
                roughness={0.42}
                metalness={0}
                clearcoat={0.35}
                clearcoatRoughness={0.35}
                sheen={0.6}
                sheenRoughness={0.45}
                sheenColor={appearance.secondaryColor}
            />
        </mesh>
    );
}
