import { useState } from 'react'
import { Link } from 'react-router-dom'
import { useAuth } from '../context/useAuth'
import { NotificationBell } from '../features/notifications/NotificationBell'
import { MainNav } from './MainNav'
import { HOME_PATH } from './navigation'
import './site-header.css'

/** L'en-tête n'a pas de fond : il se pose directement sur le ciel du décor. */
export function SiteHeader() {
  const { isAuthenticated, isReady, logout, bootstrapError } = useAuth()
  const [logoutError, setLogoutError] = useState('')
  const signedIn = isReady && isAuthenticated

  return (
    <header className="site-header">
      <div className="site-header__bar">
        <Link className="site-header__brand" to={signedIn ? HOME_PATH : '/login'}>
          matcha
          <span className="site-header__tagline">la clairière</span>
        </Link>

        {signedIn && <MainNav />}

        {signedIn && (
          <div className="site-header__actions">
            <NotificationBell />
            <button
              className="site-header__logout"
              type="button"
              onClick={() => {
                setLogoutError('')
                void logout().catch(() => setLogoutError('La déconnexion a échoué.'))
              }}
            >
              Se déconnecter
            </button>
          </div>
        )}
      </div>

      {(bootstrapError || logoutError) && (
        <p className="site-header__alert" role="alert">{logoutError || bootstrapError}</p>
      )}
    </header>
  )
}
