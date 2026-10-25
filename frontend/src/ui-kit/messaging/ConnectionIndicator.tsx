import type { ConnectionStatus } from './types'
import './messaging.css'

const DEFAULT_LABELS: Record<ConnectionStatus, string> = {
  connecting: 'Connexion…',
  online: 'En direct',
  offline: 'Hors ligne, reconnexion…',
}

export function ConnectionIndicator({
  status,
  labels = DEFAULT_LABELS,
}: {
  status: ConnectionStatus
  labels?: Record<ConnectionStatus, string>
}) {
  return (
    <p className="uik-connection" data-status={status} role="status">
      <span className="uik-connection__dot" aria-hidden="true" />
      {labels[status]}
    </p>
  )
}
