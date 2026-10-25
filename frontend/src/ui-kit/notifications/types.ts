import type { ReactNode } from 'react'

export interface NotificationAction {
  label: string
  onSelect: () => void
}

export interface NotificationItem {
  id: string | number
  title: string
  description?: string
  timestamp?: Date
  unread?: boolean
  /** Avatar, icône… affiché à gauche. */
  leading?: ReactNode
  action?: NotificationAction
}

export type ToastTone = 'neutral' | 'success' | 'danger' | 'highlight'

export interface Toast {
  id: string | number
  title: string
  description?: string
  tone?: ToastTone
  leading?: ReactNode
  action?: NotificationAction
  /** Durée d'affichage en millisecondes ; 0 = jusqu'à fermeture manuelle. */
  duration?: number
}

export type ToastInput = Omit<Toast, 'id'> & { id?: Toast['id'] }

export type ToasterPosition = 'top-left' | 'top-right' | 'bottom-left' | 'bottom-right'
