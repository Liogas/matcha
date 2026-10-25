import { useCallback, useEffect, useMemo, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { useSocial } from '../../context/useSocial'
import { ProfileDialog } from '../../shared/profile/ProfileDialog'
import { ProfileImage } from '../../shared/profile/ProfileImage'
import { NotificationCenter, type NotificationItem } from '../../ui-kit/notifications'
import { EVENT_TEXT } from './eventText'

/**
 * Adaptateur entre le fil d'événements social et le centre de notifications du ui-kit.
 * Les événements restent « non lus » tant que le panneau est ouvert, puis sont marqués lus.
 */
export function NotificationBell() {
  const { events, markEventsRead, profilesById, ensureProfiles } = useSocial()
  const navigate = useNavigate()
  const [openId, setOpenId] = useState<number | null>(null)
  const closeDialog = useCallback(() => setOpenId(null), [])

  useEffect(() => {
    ensureProfiles(events.map((event) => event.accountId))
  }, [events, ensureProfiles])

  const items = useMemo<NotificationItem[]>(() => events.map((event) => {
    const profile = profilesById[event.accountId]
    const name = profile?.first_name ?? `Membre n° ${event.accountId}`
    return {
      id: event.id,
      title: EVENT_TEXT[event.kind](name),
      description: event.preview ? `« ${event.preview} »` : undefined,
      timestamp: event.at,
      unread: !event.read,
      leading: <ProfileImage className="portrait--avatar" path={profile?.photo_url} name={name} />,
      action: event.kind === 'message' && event.conversationId
        ? { label: 'Lire', onSelect: () => navigate(`/messages?c=${event.conversationId}`) }
        : profile
          ? { label: 'Voir', onSelect: () => setOpenId(event.accountId) }
          : undefined,
    }
  }), [events, profilesById, navigate])

  return (
    <>
      <NotificationCenter
        items={items}
        onOpenChange={(open) => {
          if (!open) markEventsRead()
        }}
        onMarkAllRead={markEventsRead}
        locale="fr-FR"
        labels={{ empty: 'Rien de nouveau depuis votre arrivée.' }}
      />
      {openId !== null && <ProfileDialog accountId={openId} onClose={closeDialog} />}
    </>
  )
}
