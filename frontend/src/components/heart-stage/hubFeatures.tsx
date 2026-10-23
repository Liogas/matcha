import type { ReactNode } from 'react'

export interface HubFeature {
  path: string
  label: string
  hint: string
  icon: ReactNode
}

const iconProps = {
  width: 22,
  height: 22,
  viewBox: '0 0 24 24',
  fill: 'none',
  stroke: 'currentColor',
  strokeWidth: 1.8,
  strokeLinecap: 'round' as const,
  strokeLinejoin: 'round' as const,
  'aria-hidden': true,
}

export const HUB_FEATURES: HubFeature[] = [
  {
    path: '/app/decouvrir',
    label: 'Découvrir',
    hint: 'Des profils qui pourraient vous plaire',
    icon: <svg {...iconProps}><circle cx="11" cy="11" r="6.5" /><path d="m20 20-4.2-4.2" /></svg>,
  },
  {
    path: '/app/matchs',
    label: 'Matchs',
    hint: 'Les coups de cœur réciproques',
    icon: <svg {...iconProps}><path d="M12 20s-7-4.4-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 10c0 5.6-7 10-7 10Z" /></svg>,
  },
  {
    path: '/app/messages',
    label: 'Messages',
    hint: 'Vos conversations',
    icon: <svg {...iconProps}><path d="M5 18.5V6.5A2.5 2.5 0 0 1 7.5 4h9A2.5 2.5 0 0 1 19 6.5v7a2.5 2.5 0 0 1-2.5 2.5H8Z" /></svg>,
  },
  {
    path: '/app/notifications',
    label: 'Notifications',
    hint: 'Ce qui s’est passé pendant votre absence',
    icon: <svg {...iconProps}><path d="M6 16V11a6 6 0 0 1 12 0v5l1.5 2h-15Z" /><path d="M10 20.5a2 2 0 0 0 4 0" /></svg>,
  },
  {
    path: '/app/profil',
    label: 'Profil',
    hint: 'Ce que les autres voient de vous',
    icon: <svg {...iconProps}><circle cx="12" cy="8.5" r="3.5" /><path d="M5 20a7 7 0 0 1 14 0" /></svg>,
  },
]
