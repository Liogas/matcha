import { createContext } from 'react'
import type {
  MessagingConversation,
  MessagingMessage,
  Profile,
} from '../api/auth'

/** Ce qui s'est passé depuis l'ouverture de la session (le backend n'a pas encore de notifications). */
export interface SocialEvent {
  id: number
  kind: 'like' | 'match' | 'unlike' | 'message'
  accountId: number
  conversationId?: number
  preview?: string
  at: Date
  read: boolean
}

export type SocketStatus = 'connecting' | 'open' | 'closed'

export interface SocialContextValue {
  isLoaded: boolean
  loadError: string
  myProfile: Profile | null
  setMyProfile: (profile: Profile) => void
  reloadMyProfile: () => Promise<void>

  likedIds: Set<number>
  incomingIds: Set<number>
  matchIds: Set<number>
  conversations: MessagingConversation[]
  refresh: () => Promise<void>
  like: (accountId: number) => Promise<{ matched: boolean }>
  unlike: (accountId: number) => Promise<void>

  /** Profils publics déjà chargés ; `null` si le profil n'est plus visible. */
  profilesById: Record<number, Profile | null>
  ensureProfiles: (accountIds: number[]) => void

  socketStatus: SocketStatus
  subscribeMessages: (listener: (message: MessagingMessage) => void) => () => void
  sendMessage: (conversationId: number, body: string) => Promise<void>
  /** La conversation affichée ne compte pas de non-lus. */
  setActiveConversation: (conversationId: number | null) => void
  markConversationRead: (conversationId: number) => Promise<void>

  events: SocialEvent[]
  markEventsRead: () => void
}

export const SocialContext = createContext<SocialContextValue | null>(null)
