import type { Material, WebGLProgramParametersWithUniforms } from 'three'

/**
 * Uniformes partagés par tous les matériaux du décor : un seul objet
 * mis à jour par image suffit à faire onduler herbe et feuillages ensemble.
 */
export interface SharedUniforms {
  uTime: { value: number }
  uWind: { value: number }
  uSnow: { value: number }
}

export function createSharedUniforms(): SharedUniforms {
  return {
    uTime: { value: 0 },
    uWind: { value: 0.4 },
    uSnow: { value: 0 },
  }
}

export interface EffectOptions {
  /**
   * `grass` : brins plus souples vers la pointe (y local de 0 à 1).
   * `canopy` : balancement lent des houppiers, proportionnel à la hauteur.
   */
  wind?: 'grass' | 'canopy'
  /** Amplitude du vent pour ce matériau. */
  windScale?: number
  /** Dépose la neige sur les faces tournées vers le ciel. */
  snow?: boolean
  /** Seuil de pente au-delà duquel la neige ne tient plus (0 = vertical, 1 = plat). */
  snowSlope?: number
  /** Facteur d'écrasement de la hauteur sous la neige (herbe). */
  flattenUnderSnow?: boolean
  /**
   * Garde la normale du maillage sur les deux faces (brins d'herbe, feuilles) :
   * sinon le revers d'un brin, vu de dos, paraît noir.
   */
  keepNormals?: boolean
}

const WIND_GRASS = /* glsl */ `
  #ifdef USE_INSTANCING
    vec3 forestRoot = (modelMatrix * instanceMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  #else
    vec3 forestRoot = (modelMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  #endif
  float forestBend = position.y * position.y;
  float forestGust = sin(uTime * 1.3 + forestRoot.x * 0.21 + forestRoot.z * 0.17) * 0.6
    + sin(uTime * 2.9 + forestRoot.x * 0.83 - forestRoot.z * 0.4) * 0.25;
  transformed.x += (forestGust + 0.35) * uWind * forestBend * uWindScale;
  transformed.z += forestGust * uWind * forestBend * uWindScale * 0.45;
`

const WIND_CANOPY = /* glsl */ `
  #ifdef USE_INSTANCING
    vec3 forestRoot = (modelMatrix * instanceMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  #else
    vec3 forestRoot = (modelMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  #endif
  float forestSway = sin(uTime * 0.7 + forestRoot.x * 0.13 + forestRoot.z * 0.11)
    + 0.35 * sin(uTime * 1.9 + forestRoot.z * 0.5);
  float forestHeight = max(position.y, 0.0);
  transformed.x += forestSway * uWind * forestHeight * 0.012 * uWindScale;
  transformed.z += forestSway * uWind * forestHeight * 0.006 * uWindScale;
`

/**
 * Greffe vent et neige sur un matériau standard de three.js (Lambert, Standard…)
 * sans réécrire son éclairage, ses ombres ni son brouillard.
 */
export function applyEffects<T extends Material>(material: T, shared: SharedUniforms, options: EffectOptions): T {
  const windScale = { value: options.windScale ?? 1 }
  const snowSlope = { value: options.snowSlope ?? 0.55 }

  material.onBeforeCompile = (shader: WebGLProgramParametersWithUniforms) => {
    Object.assign(shader.uniforms, shared, { uWindScale: windScale, uSnowSlope: snowSlope })

    let vertexHeader = 'uniform float uTime;\nuniform float uWind;\nuniform float uWindScale;\nuniform float uSnow;\n'
    let vertexBody = ''
    if (options.wind === 'grass') vertexBody += WIND_GRASS
    if (options.wind === 'canopy') vertexBody += WIND_CANOPY
    if (options.flattenUnderSnow) vertexBody += 'transformed.y *= 1.0 - uSnow * 0.85;\n'

    if (options.snow) {
      vertexHeader += 'varying float vForestUp;\nvarying vec3 vForestWorld;\n'
      vertexBody += /* glsl */ `
        #ifdef USE_INSTANCING
          vec3 forestNormal = mat3(modelMatrix) * mat3(instanceMatrix) * objectNormal;
          vForestWorld = (modelMatrix * instanceMatrix * vec4(transformed, 1.0)).xyz;
        #else
          vec3 forestNormal = mat3(modelMatrix) * objectNormal;
          vForestWorld = (modelMatrix * vec4(transformed, 1.0)).xyz;
        #endif
        vForestUp = normalize(forestNormal).y;
      `
    }

    shader.vertexShader = vertexHeader + shader.vertexShader.replace(
      '#include <begin_vertex>',
      `#include <begin_vertex>\n${vertexBody}`,
    )

    if (options.keepNormals) {
      shader.fragmentShader = shader.fragmentShader.replace(
        '#include <normal_fragment_begin>',
        'float faceDirection = gl_FrontFacing ? 1.0 : -1.0;\nvec3 normal = normalize(vNormal);\nvec3 nonPerturbedNormal = normal;',
      )
    }

    if (options.snow) {
      shader.fragmentShader = /* glsl */ `
        uniform float uSnow;
        uniform float uSnowSlope;
        varying float vForestUp;
        varying vec3 vForestWorld;
        float forestHash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
        float forestNoise(vec2 p) {
          vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
          return mix(mix(forestHash(i), forestHash(i + vec2(1.0, 0.0)), f.x),
                     mix(forestHash(i + vec2(0.0, 1.0)), forestHash(i + vec2(1.0, 1.0)), f.x), f.y);
        }
      ` + shader.fragmentShader.replace(
        '#include <color_fragment>',
        /* glsl */ `#include <color_fragment>
        float forestPatch = forestNoise(vForestWorld.xz * 0.35) * 0.6 + forestNoise(vForestWorld.xz * 1.7) * 0.4;
        float forestCover = smoothstep(uSnowSlope, uSnowSlope + 0.25, vForestUp)
          * smoothstep(1.0 - uSnow * 1.4, 1.15 - uSnow * 1.4, forestPatch + 0.25);
        diffuseColor.rgb = mix(diffuseColor.rgb, vec3(0.9, 0.93, 0.97), forestCover * min(uSnow * 1.6, 1.0));`,
      )
    }
  }
  // Clé de cache : deux matériaux aux options différentes ne partagent pas le même programme
  material.customProgramCacheKey = () => JSON.stringify(options)
  return material
}
