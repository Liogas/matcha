/** Liens principaux de l'en-tête, dans l'ordre d'affichage. */
export const MAIN_NAVIGATION = [
  { path: '/profil', label: 'Profil' },
  { path: '/decouvrir', label: 'Découvrir' },
  { path: '/matchs', label: 'Matchs' },
  { path: '/messages', label: 'Messages' },
] as const

/** Pages d'information du pied de page (contenu à rédiger). */
export const LEGAL_NAVIGATION = [
  { slug: 'conditions', label: 'Conditions générales' },
  { slug: 'confidentialite', label: 'Confidentialité' },
  { slug: 'mentions-legales', label: 'Mentions légales' },
  { slug: 'contact', label: 'Contact' },
] as const

export type LegalSlug = (typeof LEGAL_NAVIGATION)[number]['slug']

export const HOME_PATH = '/decouvrir'
