import { useContext, useSyncExternalStore } from 'react'
import { HeartContext } from './HeartContext'

export function useHeart() {
  const context = useContext(HeartContext)
  if (!context)
    throw new Error('useHeart doit être utilisé dans un HeartProvider.')
  return context
}

/** Humeur de fond du cœur (éveillé, endormi), pour l'interface HTML. */
export function useHeartMood() {
  const { controller } = useHeart()
  return useSyncExternalStore(
    (listener) => controller.subscribeMood(listener),
    () => controller.mood,
  )
}
