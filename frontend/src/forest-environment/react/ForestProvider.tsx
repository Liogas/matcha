import { useCallback, useEffect, useMemo, useRef, useState, useSyncExternalStore } from 'react'
import type { ReactNode } from 'react'
import { ForestEnvironment } from '../ForestEnvironment'
import type { CubReaction } from '../wolf-cub/WolfCubModel'
import { WorldController } from '../world/WorldController'
import { WorldModel } from '../world/WorldModel'
import { ForestContext, type ForestContextValue } from './ForestContext'

const MOTION_KEY = 'foret-animee'
/** Sans décor 3D, le monde n'a besoin d'avancer que pour le thème jour/nuit. */
const FALLBACK_TICK = 30_000

function readMotionPreference() {
  try {
    const stored = window.localStorage.getItem(MOTION_KEY)
    if (stored !== null) return stored === 'oui'
  } catch {
    // Stockage indisponible (navigation privée) : on suit la préférence du système
  }
  return !window.matchMedia('(prefers-reduced-motion: reduce)').matches
}

function isPasswordField(target: EventTarget | null) {
  return target instanceof HTMLInputElement && target.type === 'password'
}

function isTextField(target: EventTarget | null) {
  return target instanceof HTMLTextAreaElement || target instanceof HTMLInputElement
}

/**
 * Crée le monde (toujours) et le décor 3D (si WebGL le permet), puis relie
 * le décor à l'interface : thème jour/nuit sur <html>, réactions du louveteau,
 * préférence d'animation.
 */
export function ForestProvider({ children }: { children: ReactNode }) {
  const [{ worldModel, world }] = useState(() => {
    const model = new WorldModel()
    return { worldModel: model, world: new WorldController(model) }
  })
  const [environment, setEnvironment] = useState<ForestEnvironment | null>(null)
  const [animated, setAnimatedState] = useState(readMotionPreference)
  const insetRef = useRef(0)
  // Référence stable : les fonctions exposées ne changent pas quand le décor apparaît
  const environmentRef = useRef<ForestEnvironment | null>(null)

  // Construit le décor après le premier affichage : l'interface apparaît sans attendre
  useEffect(() => {
    let created: ForestEnvironment | null = null
    const frame = requestAnimationFrame(() => {
      created = ForestEnvironment.create(worldModel, world)
      setEnvironment(created)
    })
    return () => {
      cancelAnimationFrame(frame)
      created?.dispose()
      setEnvironment(null)
    }
  }, [worldModel, world])

  useEffect(() => {
    environmentRef.current = environment
    environment?.setMotion(animated)
    environment?.setRightInset(insetRef.current)
  }, [environment, animated])

  useEffect(() => {
    if (environment) return
    const interval = window.setInterval(() => world.tick(0), FALLBACK_TICK)
    return () => window.clearInterval(interval)
  }, [environment, world])

  // Le thème de l'interface suit le ciel
  const summary = useSyncExternalStore(
    (listener) => world.subscribe(listener),
    () => world.getSummary(),
  )
  useEffect(() => {
    const root = document.documentElement
    root.dataset.daylight = summary.isNight ? 'night' : 'day'
    root.dataset.season = summary.season
    root.dataset.phase = summary.phase
  }, [summary])

  // Le louveteau détourne le regard quand on tape un mot de passe, et dresse l'oreille à la frappe
  useEffect(() => {
    if (!environment) return
    const onFocusIn = (event: FocusEvent) => environment.setPolite(isPasswordField(event.target))
    const onFocusOut = (event: FocusEvent) => {
      if (isPasswordField(event.target)) environment.setPolite(false)
    }
    const onInput = (event: Event) => {
      if (isTextField(event.target)) environment.notice()
    }
    document.addEventListener('focusin', onFocusIn)
    document.addEventListener('focusout', onFocusOut)
    document.addEventListener('input', onInput)
    return () => {
      document.removeEventListener('focusin', onFocusIn)
      document.removeEventListener('focusout', onFocusOut)
      document.removeEventListener('input', onInput)
    }
  }, [environment])

  const setAnimated = useCallback((next: boolean) => {
    setAnimatedState(next)
    try {
      window.localStorage.setItem(MOTION_KEY, next ? 'oui' : 'non')
    } catch {
      // Préférence non mémorisée : elle vaut pour cette visite
    }
  }, [])

  const react = useCallback((reaction: CubReaction) => environmentRef.current?.react(reaction), [])
  const setRightInset = useCallback((fraction: number) => {
    insetRef.current = fraction
    environmentRef.current?.setRightInset(fraction)
  }, [])

  const value = useMemo<ForestContextValue>(() => ({
    world,
    environment,
    react,
    setRightInset,
    animated,
    setAnimated,
  }), [world, environment, react, setRightInset, animated, setAnimated])

  return <ForestContext.Provider value={value}>{children}</ForestContext.Provider>
}
