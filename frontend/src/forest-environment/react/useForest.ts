import { useContext, useSyncExternalStore } from 'react'
import { ForestContext } from './ForestContext'

export function useForest() {
  const context = useContext(ForestContext)
  if (!context)
    throw new Error('useForest doit être utilisé dans un ForestProvider.')
  return context
}

/** Heure du jour, saison et lune, pour l'interface HTML. */
export function useForestSummary() {
  const { world } = useForest()
  return useSyncExternalStore(
    (listener) => world.subscribe(listener),
    () => world.getSummary(),
  )
}
