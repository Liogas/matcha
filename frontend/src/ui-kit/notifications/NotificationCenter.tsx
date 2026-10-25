import { useCallback, useEffect, useId, useRef, useState } from 'react'
import type { ReactNode } from 'react'
import { defaultFormatTime } from '../shared/time'
import type { NotificationItem } from './types'
import './notifications.css'

export interface NotificationCenterLabels {
  title: string
  empty: string
  markAllRead: string
  /** Libellé accessible du bouton, avec le nombre de non-lus. */
  trigger: (unread: number) => string
}

const DEFAULT_LABELS: NotificationCenterLabels = {
  title: 'Notifications',
  empty: 'Rien de nouveau pour le moment.',
  markAllRead: 'Tout marquer comme lu',
  trigger: (unread) => (unread > 0 ? `Notifications, ${unread} non lue${unread > 1 ? 's' : ''}` : 'Notifications'),
}

export interface NotificationCenterProps {
  items: NotificationItem[]
  /** Par défaut : le nombre d'éléments `unread`. */
  unreadCount?: number
  onOpenChange?: (open: boolean) => void
  onMarkAllRead?: () => void
  labels?: Partial<NotificationCenterLabels>
  formatTime?: (date: Date) => string
  /** Icône du bouton (une cloche par défaut). */
  icon?: ReactNode
  /** Contenu libre sous le titre (résumé, lien…). */
  header?: ReactNode
  /** Langue des dates par défaut (ex. « fr-FR ») ; celle du navigateur sinon. */
  locale?: string
  className?: string
}

function BellIcon() {
  return (
    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
      <path d="M6 16v-5a6 6 0 0 1 12 0v5l1.5 2h-15Z" />
      <path d="M10 20.5a2 2 0 0 0 4 0" />
    </svg>
  )
}

/**
 * Bouton cloche + panneau déroulant de notifications.
 * Aucune dépendance au reste de l'application : tout arrive par les props.
 */
export function NotificationCenter({
  items,
  unreadCount,
  onOpenChange,
  onMarkAllRead,
  labels: labelOverrides,
  locale,
  formatTime = (date) => defaultFormatTime(date, locale),
  icon,
  header,
  className = '',
}: NotificationCenterProps) {
  const labels = { ...DEFAULT_LABELS, ...labelOverrides }
  const [open, setOpen] = useState(false)
  const rootRef = useRef<HTMLDivElement>(null)
  const triggerRef = useRef<HTMLButtonElement>(null)
  const panelId = useId()
  const unread = unreadCount ?? items.filter((item) => item.unread).length

  const toggle = useCallback((next: boolean) => {
    setOpen(next)
    onOpenChange?.(next)
  }, [onOpenChange])

  // Fermeture à l'Échap ou au clic extérieur ; le focus revient sur la cloche
  useEffect(() => {
    if (!open) return
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key !== 'Escape') return
      toggle(false)
      triggerRef.current?.focus()
    }
    const onPointerDown = (event: PointerEvent) => {
      if (!rootRef.current?.contains(event.target as Node)) toggle(false)
    }
    document.addEventListener('keydown', onKeyDown)
    document.addEventListener('pointerdown', onPointerDown)
    return () => {
      document.removeEventListener('keydown', onKeyDown)
      document.removeEventListener('pointerdown', onPointerDown)
    }
  }, [open, toggle])

  return (
    <div className={`uik-notifications ${className}`} ref={rootRef}>
      <button
        ref={triggerRef}
        type="button"
        className="uik-notifications__trigger"
        aria-label={labels.trigger(unread)}
        aria-expanded={open}
        aria-controls={panelId}
        onClick={() => toggle(!open)}
      >
        {icon ?? <BellIcon />}
        {unread > 0 && <span className="uik-notifications__badge" aria-hidden="true">{unread > 99 ? '99+' : unread}</span>}
      </button>

      {open && (
        <div className="uik-notifications__panel" id={panelId} role="dialog" aria-label={labels.title}>
          <div className="uik-notifications__head">
            <p className="uik-notifications__title">{labels.title}</p>
            {onMarkAllRead && unread > 0 && (
              <button type="button" className="uik-notifications__link" onClick={onMarkAllRead}>
                {labels.markAllRead}
              </button>
            )}
          </div>
          {header}
          {items.length === 0 ? (
            <p className="uik-notifications__empty">{labels.empty}</p>
          ) : (
            <ul className="uik-notifications__list">
              {items.map((item) => (
                <li key={item.id} className="uik-notifications__item" data-unread={item.unread ? 'true' : undefined}>
                  {item.leading && <span className="uik-notifications__leading">{item.leading}</span>}
                  <span className="uik-notifications__text">
                    <span className="uik-notifications__item-title">{item.title}</span>
                    {item.description && <span className="uik-notifications__item-description">{item.description}</span>}
                    {item.timestamp && (
                      <time className="uik-notifications__time" dateTime={item.timestamp.toISOString()}>
                        {formatTime(item.timestamp)}
                      </time>
                    )}
                  </span>
                  {item.action && (
                    <button
                      type="button"
                      className="uik-notifications__action"
                      onClick={() => {
                        item.action?.onSelect()
                        toggle(false)
                      }}
                    >
                      {item.action.label}
                    </button>
                  )}
                </li>
              ))}
            </ul>
          )}
        </div>
      )}
    </div>
  )
}
