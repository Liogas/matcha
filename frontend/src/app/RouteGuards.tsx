import type { ReactNode } from 'react'
import { Navigate } from 'react-router-dom'
import { useAuth } from '../context/useAuth'
import { HOME_PATH } from '../layout/navigation'

/** Pages réservées aux personnes connectées. */
export function RequireSession({ children }: { children: ReactNode }) {
  const { isReady, isAuthenticated } = useAuth()
  if (!isReady)
    return <p className="app__waking" role="status">On réveille la clairière…</p>
  return isAuthenticated ? <>{children}</> : <Navigate to="/login" replace />
}

/** Pages de connexion et d'inscription : inutiles une fois connecté. */
export function GuestOnly({ children }: { children: ReactNode }) {
  const { isReady, isAuthenticated } = useAuth()
  return isReady && isAuthenticated ? <Navigate to={HOME_PATH} replace /> : <>{children}</>
}
