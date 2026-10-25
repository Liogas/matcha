import type { ReactNode } from 'react'
import './messaging.css'

export interface ChatLayoutProps {
  sidebar: ReactNode
  /** En-tête au-dessus du fil (nom de l'interlocuteur…). */
  header?: ReactNode
  children: ReactNode
  /** Sur petit écran : la liste s'efface quand une conversation est ouverte. */
  hasSelection: boolean
  onBack?: () => void
  labels?: { back: string }
}

/** Liste et fil côte à côte ; sur mobile, l'un ou l'autre avec un bouton retour. */
export function ChatLayout({ sidebar, header, children, hasSelection, onBack, labels = { back: 'Conversations' } }: ChatLayoutProps) {
  return (
    <div className="uik-chat" data-selection={hasSelection ? 'true' : 'false'}>
      <div className="uik-chat__sidebar">{sidebar}</div>
      <div className="uik-chat__main">
        {(header || onBack) && (
          <div className="uik-chat__header">
            {onBack && (
              <button type="button" className="uik-chat__back" onClick={onBack}>
                <span aria-hidden="true">←</span> {labels.back}
              </button>
            )}
            {header}
          </div>
        )}
        {children}
      </div>
    </div>
  )
}
