import { useEffect, useRef } from 'react'
import type { ReactNode } from 'react'
import { BrowserRouter } from 'react-router-dom'
import { AuthProvider } from '../context/AuthProvider'
import { SocialProvider } from '../context/SocialProvider'
import { useAuth } from '../context/useAuth'
import { SocialToasts } from '../features/notifications/SocialToasts'
import { ForestProvider, useForest } from '../forest-environment'
import { AppLayout } from '../layout/AppLayout'
import { AppRoutes } from './AppRoutes'
import './app.css'

/** Une clé par compte : changer d'utilisateur repart d'un état social vierge. */
function SocialBoundary({ children }: { children: ReactNode }) {
  const { user } = useAuth()
  return <SocialProvider key={user?.id ?? 'guest'}>{children}</SocialProvider>
}

/** Le louveteau saute de joie à la connexion (pas lors d'une simple restauration de session). */
function WelcomeReaction() {
  const { isReady, isAuthenticated } = useAuth()
  const { react } = useForest()
  const wasAuthenticated = useRef<boolean | null>(null)

  useEffect(() => {
    if (!isReady) return
    if (wasAuthenticated.current === false && isAuthenticated) react('joy')
    wasAuthenticated.current = isAuthenticated
  }, [isReady, isAuthenticated, react])

  return null
}

function Shell() {
  const { isReady, isAuthenticated } = useAuth()
  return (
    <AppLayout overlay={isReady && isAuthenticated ? <SocialToasts /> : null}>
      <WelcomeReaction />
      <AppRoutes />
    </AppLayout>
  )
}

export function App() {
  return (
    <ForestProvider>
      <AuthProvider>
        <SocialBoundary>
          <BrowserRouter>
            <Shell />
          </BrowserRouter>
        </SocialBoundary>
      </AuthProvider>
    </ForestProvider>
  )
}
