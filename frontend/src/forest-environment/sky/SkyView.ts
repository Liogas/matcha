import {
  BackSide,
  Color,
  Group,
  IcosahedronGeometry,
  Matrix3,
  Mesh,
  ShaderMaterial,
  SphereGeometry,
  Vector3,
} from 'three'
import { GLSL_NOISE } from '../core/glsl'
import type { FrameContext } from '../core/types'
import type { WorldModel } from '../world/WorldModel'
import type { SkyModel } from './SkyModel'

const DOME_RADIUS = 450
const MOON_DISTANCE = 400
const MOON_RADIUS = 5.5

const DOME_VERTEX = /* glsl */ `
  varying vec3 vDirection;
  void main() {
    vDirection = normalize(position);
    vec4 world = modelMatrix * vec4(position, 1.0);
    gl_Position = projectionMatrix * viewMatrix * world;
    gl_Position.z = gl_Position.w; // toujours au fond
  }
`

const DOME_FRAGMENT = /* glsl */ `
  uniform vec3 uZenith;
  uniform vec3 uHorizon;
  uniform vec3 uGlow;
  uniform float uGlowStrength;
  uniform vec3 uSunColor;
  uniform vec3 uSunDirection;
  uniform float uSunVisibility;
  uniform vec3 uMoonDirection;
  uniform float uMoonVisibility;
  uniform float uStarVisibility;
  uniform mat3 uStarRotation;
  uniform float uTime;
  varying vec3 vDirection;
  ${GLSL_NOISE}

  float starLayer(vec3 direction, float density, float threshold) {
    vec3 p = direction * density;
    vec3 cell = floor(p);
    vec3 local = fract(p);
    float seed = hash13(cell);
    vec3 center = vec3(hash13(cell + 1.7), hash13(cell + 4.3), hash13(cell + 8.1)) * 0.7 + 0.15;
    float dist = length(local - center);
    float star = smoothstep(0.075, 0.0, dist) * step(threshold, seed);
    float twinkle = 0.65 + 0.35 * sin(uTime * (1.5 + seed * 3.0) + seed * 60.0);
    return star * twinkle * (0.4 + 0.6 * fract(seed * 13.7));
  }

  void main() {
    vec3 direction = normalize(vDirection);
    float height = clamp(direction.y, 0.0, 1.0);

    // Dégradé : l'horizon s'étale, le zénith s'installe vite
    vec3 color = mix(uHorizon, uZenith, pow(height, 0.5));

    // Halo du soleil bas : large et chaud, surtout près de l'horizon
    float sunDot = max(dot(direction, uSunDirection), 0.0);
    float horizonBand = 1.0 - smoothstep(0.0, 0.45, height);
    color += uGlow * pow(sunDot, 5.0) * uGlowStrength * horizonBand * 0.9;
    color += uGlow * pow(sunDot, 48.0) * uGlowStrength * 0.5;
    // Disque solaire et sa couronne
    color += uSunColor * pow(sunDot, 900.0) * uSunVisibility * 0.8;
    color += uSunColor * smoothstep(0.99955, 0.99975, sunDot) * uSunVisibility * 2.5;

    // Halo de la lune
    float moonDot = max(dot(direction, uMoonDirection), 0.0);
    color += vec3(0.55, 0.62, 0.8) * pow(moonDot, 260.0) * uMoonVisibility * 0.22;

    // Étoiles et voie lactée, fixées sur la voûte céleste
    if (uStarVisibility > 0.001) {
      vec3 celestial = uStarRotation * direction;
      float stars = starLayer(celestial, 150.0, 0.93) + starLayer(celestial, 320.0, 0.965) * 0.6;
      float band = exp(-pow(dot(celestial, normalize(vec3(0.35, 0.55, 0.76))), 2.0) * 22.0);
      float milky = band * (fbm2(celestial.xz * 6.0 + celestial.y * 3.0) * 0.8 + 0.2);
      float fade = smoothstep(0.0, 0.18, height) * uStarVisibility;
      color += vec3(0.85, 0.9, 1.0) * stars * fade;
      color += vec3(0.42, 0.46, 0.62) * milky * fade * 0.16;
    }

    gl_FragColor = vec4(color, 1.0);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
  }
`

const MOON_VERTEX = /* glsl */ `
  varying vec3 vNormal;
  varying vec3 vLocal;
  void main() {
    vNormal = normalize(mat3(modelMatrix) * normal);
    vLocal = position;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`

const MOON_FRAGMENT = /* glsl */ `
  uniform vec3 uSunDirection;
  uniform float uVisibility;
  uniform float uDaylight;
  varying vec3 vNormal;
  varying vec3 vLocal;
  ${GLSL_NOISE}
  void main() {
    // La phase n'est pas simulée : elle découle de la vraie direction du soleil
    float lit = smoothstep(-0.08, 0.12, dot(normalize(vNormal), uSunDirection));
    float maria = fbm2(vLocal.xy * 0.35 + vLocal.z * 0.2);
    vec3 surface = mix(vec3(0.62, 0.62, 0.6), vec3(0.98, 0.96, 0.9), smoothstep(0.35, 0.65, maria));
    vec3 earthshine = vec3(0.06, 0.08, 0.13);
    vec3 color = mix(earthshine, surface * 1.15, lit);
    // De jour, la lune n'est qu'un disque pâle
    float alpha = uVisibility * mix(1.0, lit * 0.55, uDaylight);
    gl_FragColor = vec4(color, alpha);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
  }
`

/** Dôme céleste dessiné à la main et lune éclairée par le vrai soleil. */
export class SkyView {
  readonly root = new Group()
  private readonly domeMaterial: ShaderMaterial
  private readonly moonMaterial: ShaderMaterial
  private readonly dome: Mesh
  private readonly moon: Mesh

  constructor() {
    this.domeMaterial = new ShaderMaterial({
      vertexShader: DOME_VERTEX,
      fragmentShader: DOME_FRAGMENT,
      side: BackSide,
      depthWrite: false,
      uniforms: {
        uZenith: { value: new Color() },
        uHorizon: { value: new Color() },
        uGlow: { value: new Color() },
        uGlowStrength: { value: 0 },
        uSunColor: { value: new Color() },
        uSunDirection: { value: new Vector3() },
        uSunVisibility: { value: 0 },
        uMoonDirection: { value: new Vector3() },
        uMoonVisibility: { value: 0 },
        uStarVisibility: { value: 0 },
        uStarRotation: { value: new Matrix3() },
        uTime: { value: 0 },
      },
    })
    this.dome = new Mesh(new SphereGeometry(DOME_RADIUS, 48, 24), this.domeMaterial)
    this.dome.renderOrder = -10
    this.dome.frustumCulled = false

    this.moonMaterial = new ShaderMaterial({
      vertexShader: MOON_VERTEX,
      fragmentShader: MOON_FRAGMENT,
      transparent: true,
      depthWrite: false,
      uniforms: {
        uSunDirection: { value: new Vector3() },
        uVisibility: { value: 0 },
        uDaylight: { value: 0 },
      },
    })
    this.moon = new Mesh(new IcosahedronGeometry(MOON_RADIUS, 4), this.moonMaterial)
    this.moon.renderOrder = -9
    this.moon.frustumCulled = false

    this.root.add(this.dome, this.moon)
  }

  sync(model: SkyModel, world: WorldModel, frame: FrameContext) {
    const uniforms = this.domeMaterial.uniforms
    uniforms.uZenith.value.copy(model.zenith)
    uniforms.uHorizon.value.copy(model.horizon)
    uniforms.uGlow.value.copy(model.glow)
    uniforms.uGlowStrength.value = model.glowStrength
    uniforms.uSunColor.value.copy(model.sunColor)
    uniforms.uSunDirection.value.copy(model.sunDirection)
    uniforms.uSunVisibility.value = model.sunVisibility
    uniforms.uMoonDirection.value.copy(model.moonDirection)
    uniforms.uMoonVisibility.value = model.moonVisibility
    uniforms.uStarVisibility.value = model.starVisibility
    uniforms.uStarRotation.value.copy(model.starRotation)
    uniforms.uTime.value = frame.elapsed

    // Le ciel accompagne la caméra : il est infiniment loin
    this.root.position.copy(frame.camera.position)
    this.moon.position.copy(model.moonDirection).multiplyScalar(MOON_DISTANCE)
    this.moon.visible = model.moonVisibility > 0.01
    this.moonMaterial.uniforms.uSunDirection.value.copy(model.sunDirection)
    this.moonMaterial.uniforms.uVisibility.value = model.moonVisibility
    this.moonMaterial.uniforms.uDaylight.value = world.daylight
  }

  dispose() {
    this.dome.geometry.dispose()
    this.moon.geometry.dispose()
    this.domeMaterial.dispose()
    this.moonMaterial.dispose()
  }
}
