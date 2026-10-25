import { BirdsController } from './birds/BirdsController'
import { BirdsModel } from './birds/BirdsModel'
import { BirdsView } from './birds/BirdsView'
import { CloudsController } from './clouds/CloudsController'
import { CloudsModel } from './clouds/CloudsModel'
import { CloudsView } from './clouds/CloudsView'
import { TREE } from './core/composition'
import { EffectsController } from './core/EffectsController'
import { ForestEngine } from './core/ForestEngine'
import { createSharedUniforms } from './core/materialEffects'
import type { ForestQuality } from './core/types'
import { DenController } from './den/DenController'
import { DenModel } from './den/DenModel'
import { DenView } from './den/DenView'
import { GrassController } from './grass/GrassController'
import { GrassModel } from './grass/GrassModel'
import { GrassView } from './grass/GrassView'
import { HorizonController } from './horizon/HorizonController'
import { HorizonModel } from './horizon/HorizonModel'
import { HorizonView } from './horizon/HorizonView'
import { LightingController } from './lighting/LightingController'
import { LightingModel } from './lighting/LightingModel'
import { LightingView } from './lighting/LightingView'
import { ParticlesController } from './particles/ParticlesController'
import { ParticlesModel } from './particles/ParticlesModel'
import { ParticlesView } from './particles/ParticlesView'
import { RiverController } from './river/RiverController'
import { RiverModel } from './river/RiverModel'
import { RiverPath } from './river/RiverPath'
import { RiverView } from './river/RiverView'
import { SkyController } from './sky/SkyController'
import { SkyModel } from './sky/SkyModel'
import { SkyView } from './sky/SkyView'
import { TerrainController } from './terrain/TerrainController'
import { TerrainModel } from './terrain/TerrainModel'
import { TerrainView } from './terrain/TerrainView'
import { TreeController } from './tree/TreeController'
import { TreeModel } from './tree/TreeModel'
import { TreeView } from './tree/TreeView'
import { WolfCubController } from './wolf-cub/WolfCubController'
import { WolfCubModel, type CubReaction } from './wolf-cub/WolfCubModel'
import { WolfCubView } from './wolf-cub/WolfCubView'
import { WoodsController } from './woods/WoodsController'
import { WoodsModel } from './woods/WoodsModel'
import { WoodsView } from './woods/WoodsView'
import type { WorldController } from './world/WorldController'
import type { WorldModel } from './world/WorldModel'

function detectQuality(): ForestQuality {
  const cores = navigator.hardwareConcurrency ?? 4
  const coarse = window.matchMedia('(pointer: coarse)').matches
  return cores <= 4 || coarse ? 'low' : 'high'
}

/**
 * Point d'entrée du décor : assemble chaque élément (modèle + vue + contrôleur)
 * et les confie au moteur, dans l'ordre où ils doivent se mettre à jour.
 * Le monde (heure, saison) vient de l'extérieur : il existe même sans WebGL.
 */
export class ForestEnvironment {
  private readonly engine: ForestEngine
  private readonly cub: WolfCubController

  private constructor(world: WorldModel, worldController: WorldController) {
    const quality = detectQuality()
    const engine = new ForestEngine(quality)
    this.engine = engine
    const { scene } = engine
    const shared = createSharedUniforms()

    // 1. Le temps et la lumière : tout le reste en dépend.
    // Le monde appartient à l'interface : le moteur l'anime mais ne le détruit pas.
    engine.add({ update: (frame) => worldController.update(frame), dispose: () => {} })
    engine.add(new EffectsController(shared, world))

    const sky = new SkyModel()
    const skyView = new SkyView()
    scene.add(skyView.root)
    engine.add(new SkyController(sky, skyView, world))

    const lighting = new LightingModel()
    const lightingView = new LightingView(scene, engine.renderer, quality)
    scene.add(lightingView.root)
    engine.add(new LightingController(lighting, lightingView, world, sky))

    // 2. Les lointains peints
    const clouds = new CloudsModel(quality === 'high' ? 16 : 10)
    const cloudsView = new CloudsView(clouds)
    scene.add(cloudsView.root)
    engine.add(new CloudsController(clouds, cloudsView, world, sky))

    const horizon = new HorizonModel()
    const horizonView = new HorizonView(horizon)
    scene.add(horizonView.root)
    engine.add(new HorizonController(horizon, horizonView, world, sky))

    const birds = new BirdsModel(14)
    const birdsView = new BirdsView(birds)
    scene.add(birdsView.root)
    engine.add(new BirdsController(birds, birdsView, world, sky))

    // 3. Le sol, l'eau, la végétation
    const riverPath = new RiverPath()
    const terrain = new TerrainModel(riverPath)
    const terrainView = new TerrainView(terrain, shared, quality)
    scene.add(terrainView.root)
    engine.add(new TerrainController(terrain, terrainView, world))

    const river = new RiverModel(riverPath)
    const riverView = new RiverView(river, terrain, shared)
    scene.add(riverView.root)
    engine.add(new RiverController(river, riverView, world, sky, lighting))

    const woods = new WoodsModel(terrain, quality)
    const woodsView = new WoodsView(woods, shared)
    scene.add(woodsView.root)
    engine.add(new WoodsController(woods, woodsView, world))

    const tree = new TreeModel(terrain.heightAt(TREE.x, TREE.z))
    const treeView = new TreeView(tree, shared)
    scene.add(treeView.root)
    engine.add(new TreeController(tree, treeView, world))

    const grass = new GrassModel(terrain, quality)
    const grassView = new GrassView(grass, shared)
    scene.add(grassView.root)
    engine.add(new GrassController(grass, grassView, world))

    const den = new DenModel(terrain)
    const denView = new DenView(den, shared)
    scene.add(denView.root)
    engine.add(new DenController(denView))

    // 4. Ce qui vit dans la clairière
    const cubModel = new WolfCubModel()
    const cubView = new WolfCubView()
    scene.add(cubView.root)
    this.cub = new WolfCubController(cubModel, cubView, world, terrain, den)
    engine.add(this.cub)
    engine.addPickable(cubView.root, () => this.cub.react('pet'))

    const particles = new ParticlesModel(quality)
    const particlesView = new ParticlesView(particles, engine.renderer.getPixelRatio())
    scene.add(particlesView.root)
    engine.add(new ParticlesController(particles, particlesView, world, tree, terrain))
  }

  /** Renvoie `null` si WebGL est indisponible : l'interface garde alors un fond dégradé. */
  static create(world: WorldModel, worldController: WorldController) {
    try {
      return new ForestEnvironment(world, worldController)
    } catch (error) {
      console.warn('forest-environment : décor 3D indisponible.', error)
      return null
    }
  }

  mount(container: HTMLElement) {
    this.engine.mount(container)
  }

  unmount() {
    this.engine.unmount()
  }

  react(reaction: CubReaction) {
    this.cub.react(reaction)
  }

  notice() {
    this.cub.notice()
  }

  setPolite(polite: boolean) {
    this.cub.setPolite(polite)
  }

  setMotion(animated: boolean) {
    this.engine.setMotion(animated ? 1 : 0)
  }

  setRightInset(fraction: number) {
    this.engine.setRightInset(fraction)
  }

  dispose() {
    this.engine.dispose()
  }
}
