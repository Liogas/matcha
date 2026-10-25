import { Fragment, useLayoutEffect, useRef } from 'react'
import type { ReactNode } from 'react'
import { defaultFormatDay, defaultFormatTime, toDate } from '../shared/time'
import type { ChatMessage } from './types'
import './messaging.css'

/** En deçà de cette distance du bas, on considère que la personne suit la conversation. */
const STICKY_THRESHOLD = 96

export interface MessageThreadProps {
  messages: ChatMessage[]
  loading?: boolean
  /** Affiché quand la conversation est vide. */
  emptyState?: ReactNode
  formatTime?: (date: Date) => string
  formatDay?: (date: Date) => string
  labels?: { loading: string; today: string; yesterday: string; pending: string }
  ariaLabel?: string
  /** Langue des dates par défaut (ex. « fr-FR ») ; celle du navigateur sinon. */
  locale?: string
}

const DEFAULT_LABELS = { loading: 'Chargement des messages…', today: 'Aujourd’hui', yesterday: 'Hier', pending: 'Envoi…' }

/**
 * Fil de messages : séparateurs de jour, bulles regroupées par auteur,
 * défilement automatique seulement si l'on est déjà en bas du fil.
 */
export function MessageThread({
  messages,
  loading = false,
  emptyState,
  locale,
  formatTime = (date) => defaultFormatTime(date, locale),
  formatDay,
  labels: labelOverrides,
  ariaLabel = 'Messages',
}: MessageThreadProps) {
  const labels = { ...DEFAULT_LABELS, ...labelOverrides }
  const listRef = useRef<HTMLDivElement>(null)
  const stickToBottom = useRef(true)
  const dayLabel = formatDay ?? ((date: Date) => defaultFormatDay(date, labels, locale))

  useLayoutEffect(() => {
    const list = listRef.current
    if (!list) return
    const last = messages[messages.length - 1]
    if (stickToBottom.current || last?.own) list.scrollTop = list.scrollHeight
  }, [messages])

  return (
    <div
      className="uik-thread"
      ref={listRef}
      role="log"
      aria-label={ariaLabel}
      aria-live="polite"
      onScroll={(event) => {
        const list = event.currentTarget
        stickToBottom.current = list.scrollHeight - list.scrollTop - list.clientHeight < STICKY_THRESHOLD
      }}
    >
      {loading ? (
        <p className="uik-chat__empty">{labels.loading}</p>
      ) : messages.length === 0 ? (
        <div className="uik-chat__empty">{emptyState}</div>
      ) : messages.map((message, index) => {
        const date = toDate(message.createdAt)
        const previous = messages[index - 1]
        const previousDate = previous ? toDate(previous.createdAt) : null
        const newDay = !previousDate || previousDate.toDateString() !== date.toDateString()
        const sameAuthor = !newDay && previous?.own === message.own
        return (
          <Fragment key={message.id}>
            {newDay && <p className="uik-thread__day"><span>{dayLabel(date)}</span></p>}
            <div
              className="uik-bubble"
              data-own={message.own ? 'true' : undefined}
              data-grouped={sameAuthor ? 'true' : undefined}
              data-pending={message.pending ? 'true' : undefined}
            >
              <p className="uik-bubble__body">{message.body}</p>
              <time className="uik-bubble__time" dateTime={date.toISOString()}>
                {message.pending ? labels.pending : formatTime(date)}
              </time>
            </div>
          </Fragment>
        )
      })}
    </div>
  )
}
