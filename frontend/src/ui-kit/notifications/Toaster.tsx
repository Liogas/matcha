import { useEffect, useRef, useState } from 'react'
import type { Toast, ToasterPosition } from './types'
import './notifications.css'

const DEFAULT_DURATION = 6000

export interface ToasterProps {
  toasts: Toast[]
  onDismiss: (id: Toast['id']) => void
  position?: ToasterPosition
  labels?: { dismiss: string; region: string }
  className?: string
}

function ToastCard({ toast, onDismiss, dismissLabel }: { toast: Toast; onDismiss: () => void; dismissLabel: string }) {
  const [paused, setPaused] = useState(false)
  const remaining = useRef(toast.duration ?? DEFAULT_DURATION)
  const onDismissRef = useRef(onDismiss)

  useEffect(() => {
    onDismissRef.current = onDismiss
  }, [onDismiss])

  // Le compte à rebours s'arrête au survol et au focus : le temps de lire ou d'agir
  useEffect(() => {
    if (paused || remaining.current <= 0) return
    const startedAt = Date.now()
    const timer = window.setTimeout(() => onDismissRef.current(), remaining.current)
    return () => {
      window.clearTimeout(timer)
      remaining.current -= Date.now() - startedAt
    }
  }, [paused])

  return (
    <li
      className="uik-toast"
      data-tone={toast.tone ?? 'neutral'}
      onMouseEnter={() => setPaused(true)}
      onMouseLeave={() => setPaused(false)}
      onFocus={() => setPaused(true)}
      onBlur={() => setPaused(false)}
    >
      {toast.leading && <span className="uik-toast__leading">{toast.leading}</span>}
      <span className="uik-toast__text">
        <span className="uik-toast__title">{toast.title}</span>
        {toast.description && <span className="uik-toast__description">{toast.description}</span>}
      </span>
      {toast.action && (
        <button
          type="button"
          className="uik-toast__action"
          onClick={() => {
            toast.action?.onSelect()
            onDismiss()
          }}
        >
          {toast.action.label}
        </button>
      )}
      <button type="button" className="uik-toast__close" aria-label={dismissLabel} onClick={onDismiss}>
        <span aria-hidden="true">×</span>
      </button>
    </li>
  )
}

/** Pile de toasts annoncés aux lecteurs d'écran sans voler le focus. */
export function Toaster({
  toasts,
  onDismiss,
  position = 'bottom-right',
  labels = { dismiss: 'Fermer', region: 'Notifications récentes' },
  className = '',
}: ToasterProps) {
  return (
    <section className={`uik-toaster ${className}`} data-position={position} aria-label={labels.region}>
      <ol className="uik-toaster__list" aria-live="polite">
        {toasts.map((toast) => (
          <ToastCard key={toast.id} toast={toast} onDismiss={() => onDismiss(toast.id)} dismissLabel={labels.dismiss} />
        ))}
      </ol>
    </section>
  )
}
