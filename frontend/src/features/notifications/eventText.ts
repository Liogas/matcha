import type { SocialEvent } from '../../context/SocialContext'

export const EVENT_TEXT: Record<SocialEvent['kind'], (name: string) => string> = {
  like: (name) => `${name} vous a liké`,
  match: (name) => `C’est un match avec ${name} !`,
  unlike: (name) => `${name} a retiré son like`,
  message: (name) => `Nouveau message de ${name}`,
}
