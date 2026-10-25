import { useEffect, useRef } from 'react'
import { useForest } from './useForest'

/**
 * Emplacement du décor dans la page. Sans WebGL, le conteneur garde
 * son fond en dégradé (voir la classe CSS fournie par l'application).
 */
export function ForestCanvas({ className }: { className?: string }) {
  const { environment } = useForest()
  const container = useRef<HTMLDivElement>(null)

  useEffect(() => {
    const element = container.current
    if (!environment || !element) return
    environment.mount(element)
    return () => environment.unmount()
  }, [environment])

  return <div ref={container} className={className} data-ready={environment ? 'true' : 'false'} aria-hidden="true" />
}
