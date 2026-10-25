import { useEffect, useMemo } from 'react'
import { Link, useSearchParams } from 'react-router-dom'
import type { SocketStatus } from '../../context/SocialContext'
import { useSocial } from '../../context/useSocial'
import { PagePanel } from '../../layout/PagePanel'
import { ProfileImage } from '../../shared/profile/ProfileImage'
import {
  ChatLayout,
  ConnectionIndicator,
  ConversationList,
  type ChatConversation,
  type ConnectionStatus,
} from '../../ui-kit/messaging'
import { ConversationThread } from './ConversationThread'
import './messages.css'

const CONNECTION: Record<SocketStatus, ConnectionStatus> = {
  connecting: 'connecting',
  open: 'online',
  closed: 'offline',
}

const CONNECTION_LABELS: Record<ConnectionStatus, string> = {
  connecting: 'Connexion au temps réel…',
  online: 'Temps réel actif',
  offline: 'Temps réel coupé, reconnexion…',
}

export function MessagesPage() {
  const { conversations, profilesById, ensureProfiles, socketStatus, isLoaded, loadError } = useSocial()
  const [searchParams, setSearchParams] = useSearchParams()
  const selectedId = Number(searchParams.get('c')) || null
  const selected = conversations.find((item) => item.id === selectedId)

  useEffect(() => {
    ensureProfiles(conversations.map((item) => item.other_account_id))
  }, [conversations, ensureProfiles])

  const nameOf = (accountId: number) => profilesById[accountId]?.first_name ?? `Membre n° ${accountId}`

  const items = useMemo<ChatConversation[]>(() => conversations.map((conversation) => {
    const profile = profilesById[conversation.other_account_id]
    const name = profile?.first_name ?? `Membre n° ${conversation.other_account_id}`
    return {
      id: conversation.id,
      title: name,
      subtitle: profile?.city,
      unreadCount: conversation.unread_count,
      avatar: <ProfileImage className="portrait--avatar" path={profile?.photo_url} name={name} />,
    }
  }), [conversations, profilesById])

  return (
    <PagePanel
      eyebrow="Messages"
      title="Vos conversations"
      description="Une conversation s’ouvre automatiquement à chaque match."
      actions={<ConnectionIndicator status={CONNECTION[socketStatus]} labels={CONNECTION_LABELS} />}
    >
      {loadError && <p className="feedback feedback--error" role="alert">{loadError}</p>}
      {isLoaded && conversations.length === 0 ? (
        <p className="empty-state">
          Aucune conversation pour l’instant. Rendez-vous dans <Link to="/matchs">vos matchs</Link>.
        </p>
      ) : (
        <ChatLayout
          hasSelection={Boolean(selected)}
          onBack={selected ? () => setSearchParams({}, { replace: true }) : undefined}
          header={selected && <strong className="messages__with">{nameOf(selected.other_account_id)}</strong>}
          sidebar={(
            <ConversationList
              conversations={items}
              selectedId={selectedId}
              onSelect={(id) => setSearchParams({ c: String(id) }, { replace: true })}
            />
          )}
        >
          {selected ? (
            <ConversationThread
              key={selected.id}
              conversationId={selected.id}
              otherName={nameOf(selected.other_account_id)}
            />
          ) : (
            <p className="uik-chat__empty">Choisissez une conversation.</p>
          )}
        </ChatLayout>
      )}
    </PagePanel>
  )
}
