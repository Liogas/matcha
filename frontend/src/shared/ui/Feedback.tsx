import { useEffect } from 'react'
import { useForest } from '../../forest-environment'

/** Message d'erreur ou de succès d'un formulaire ; le louveteau y réagit. */
export function Feedback({
  message,
  kind = 'error',
}: {
  message: string
  kind?: 'error' | 'success'
}) {
  const { react } = useForest()

  // Oreilles basses pour une erreur, petit hochement de tête pour un succès
  useEffect(() => {
    if (message)
      react(kind === 'success' ? 'nod' : 'sad')
  }, [message, kind, react])

  if (!message) return null
  return (
    <p className={`feedback feedback--${kind}`} role={kind === 'success' ? 'status' : 'alert'}>
      {message}
    </p>
  )
}
