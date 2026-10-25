import { useContext } from 'react'
import { SocialContext } from './SocialContext'

export function useSocial() {
  const context = useContext(SocialContext)
  if (!context)
    throw new Error('useSocial doit être utilisé dans un SocialProvider.')
  return context
}

/** Compteurs de l'en-tête : messages non lus, likes en attente, notifications non vues. */
export function useSocialBadges() {
  const { conversations, events, incomingIds, matchIds, likedIds } = useSocial()
  return {
    unreadMessages: conversations.reduce((total, item) => total + item.unread_count, 0),
    pendingLikes: [...incomingIds].filter((id) => !matchIds.has(id) && !likedIds.has(id)).length,
    unseenEvents: events.filter((event) => !event.read).length,
  }
}

/** Relation entre la personne connectée et un autre profil. */
export function useRelation(accountId: number) {
  const { likedIds, incomingIds, matchIds } = useSocial()
  return {
    liked: likedIds.has(accountId),
    likesMe: incomingIds.has(accountId),
    matched: matchIds.has(accountId),
  }
}
