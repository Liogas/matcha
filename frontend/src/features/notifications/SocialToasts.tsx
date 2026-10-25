import { useEffect, useRef } from 'react'
import { useNavigate } from 'react-router-dom'
import { useSocial } from '../../context/useSocial'
import { Toaster, useToasts, type ToastTone } from '../../ui-kit/notifications'
import { EVENT_TEXT } from './eventText'

const TONES: Record<string, ToastTone> = {
  match: 'highlight',
  like: 'success',
  message: 'neutral',
  unlike: 'neutral',
}

/** Affiche un toast pour chaque nouvel événement social arrivé pendant la visite. */
export function SocialToasts() {
  const { events, profilesById } = useSocial()
  const { toasts, push, dismiss } = useToasts()
  const navigate = useNavigate()
  const lastSeenId = useRef<number | null>(null)

  useEffect(() => {
    const newest = events[0]?.id ?? 0
    // Premier passage : on ne rejoue pas l'historique déjà présent
    if (lastSeenId.current === null) {
      lastSeenId.current = newest
      return
    }
    const fresh = events.filter((event) => event.id > (lastSeenId.current ?? 0) && event.kind !== 'unlike')
    lastSeenId.current = Math.max(lastSeenId.current, newest)
    for (const event of fresh.reverse()) {
      const name = profilesById[event.accountId]?.first_name ?? 'Quelqu’un'
      push({
        title: EVENT_TEXT[event.kind](name),
        description: event.preview,
        tone: TONES[event.kind],
        action: event.kind === 'message' && event.conversationId
          ? { label: 'Lire', onSelect: () => navigate(`/messages?c=${event.conversationId}`) }
          : event.kind === 'match'
            ? { label: 'Voir', onSelect: () => navigate('/matchs') }
            : undefined,
      })
    }
  }, [events, profilesById, push, navigate])

  return <Toaster toasts={toasts} onDismiss={dismiss} position="bottom-left" />
}
