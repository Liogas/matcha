import type { ReactNode } from 'react'
import { ForestCanvas } from '../forest-environment'
import { SiteFooter } from './SiteFooter'
import { SiteHeader } from './SiteHeader'
import './app-layout.css'

/**
 * Squelette de toutes les pages : le décor en fond fixe, l'en-tête sur le ciel,
 * le contenu dans un panneau à droite, le pied de page sur l'herbe.
 */
export function AppLayout({ children, overlay }: { children: ReactNode; overlay?: ReactNode }) {
  return (
    <div className="app-layout">
      <ForestCanvas className="app-layout__scenery" />
      <a className="app-layout__skip" href="#contenu">Aller au contenu</a>
      <SiteHeader />
      <main className="app-layout__main" id="contenu" tabIndex={-1}>
        {children}
      </main>
      <SiteFooter />
      {overlay}
    </div>
  )
}
