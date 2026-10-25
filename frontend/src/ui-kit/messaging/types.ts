import type { ReactNode } from 'react'

export interface ChatConversation {
  id: string | number
  title: string
  subtitle?: string
  avatar?: ReactNode
  unreadCount?: number
}

export interface ChatMessage {
  id: string | number
  body: string
  createdAt: Date | string
  /** Message envoyé par la personne connectée. */
  own: boolean
  /** En cours d'envoi. */
  pending?: boolean
}

export type ConnectionStatus = 'connecting' | 'online' | 'offline'
