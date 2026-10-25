import type { ChatConversation } from './types'
import './messaging.css'

export interface ConversationListProps {
  conversations: ChatConversation[]
  selectedId?: ChatConversation['id'] | null
  onSelect: (id: ChatConversation['id']) => void
  ariaLabel?: string
  emptyLabel?: string
  /** Libellé accessible du compteur de non-lus. */
  unreadLabel?: (count: number) => string
}

export function ConversationList({
  conversations,
  selectedId = null,
  onSelect,
  ariaLabel = 'Conversations',
  emptyLabel = 'Aucune conversation.',
  unreadLabel = (count) => `${count} non lu${count > 1 ? 's' : ''}`,
}: ConversationListProps) {
  if (conversations.length === 0)
    return <p className="uik-chat__empty">{emptyLabel}</p>

  return (
    <ul className="uik-conversations" aria-label={ariaLabel}>
      {conversations.map((conversation) => {
        const selected = conversation.id === selectedId
        const unread = conversation.unreadCount ?? 0
        return (
          <li key={conversation.id}>
            <button
              type="button"
              className="uik-conversations__item"
              aria-current={selected ? 'true' : undefined}
              onClick={() => onSelect(conversation.id)}
            >
              {conversation.avatar && <span className="uik-conversations__avatar">{conversation.avatar}</span>}
              <span className="uik-conversations__text">
                <span className="uik-conversations__title">{conversation.title}</span>
                {conversation.subtitle && <span className="uik-conversations__subtitle">{conversation.subtitle}</span>}
              </span>
              {unread > 0 && (
                <span className="uik-conversations__unread" aria-label={unreadLabel(unread)}>
                  {unread > 99 ? '99+' : unread}
                </span>
              )}
            </button>
          </li>
        )
      })}
    </ul>
  )
}
