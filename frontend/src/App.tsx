import { AnimatePresence, useReducedMotion } from 'motion/react'
import { lazy, Suspense, useState } from 'react'
import type { ReactNode } from 'react'
import {
  BrowserRouter,
  Link,
  Navigate,
  Route,
  Routes,
  useLocation,
} from 'react-router-dom'
import { AuthProvider } from './context/AuthProvider'
import { useAuth } from './context/useAuth'
import {
  ForgotPasswordPage,
  LoginPage,
  RegisterPage,
  ResendVerificationPage,
  ResetPasswordPage,
  VerifyEmailPage,
} from './pages/AuthPages'
import { ProfilePage } from './pages/ProfilePage'
import './App.css'

const AvatarScene = lazy(() =>
  import('./components/AvatarScene').then((module) => ({
    default: module.AvatarScene,
  })),
)

function RequireSession({ children }: { children: ReactNode }) {
  const { isReady, isAuthenticated } = useAuth()

  if (!isReady) {
    return <p className="session-loading">On réveille votre espace…</p>
  }

  return isAuthenticated ? <>{children}</> : <Navigate to="/login" replace />
}

function AnimatedRoutes() {
  const location = useLocation()
  const { bootstrapError, isAuthenticated, isReady, logout } = useAuth()
  const shouldReduceMotion = useReducedMotion()
  const [avatarReaction, setAvatarReaction] = useState(0)
  const [logoutError, setLogoutError] = useState('')
  const avatarMood = location.pathname.includes('register')
    ? 'welcoming'
    : location.pathname.includes('password')
      ? 'thoughtful'
      : location.pathname.includes('verify')
        ? 'curious'
        : 'playful'

  return (
    <div className="app-shell">
      <Suspense fallback={<div className="avatar-fallback" aria-hidden="true" />}>
        <AvatarScene
          mood={avatarMood}
          reaction={avatarReaction}
          onInteract={() => setAvatarReaction((current) => current + 1)}
          reduceMotion={shouldReduceMotion ?? false}
        />
      </Suspense>
      <div className="scene-wash" aria-hidden="true" />

      <header className="site-header">
        <Link className="brand" to={isAuthenticated ? '/app' : '/login'}>
          <span className="brand-mark" aria-hidden="true">m</span>
          <span>matcha</span>
        </Link>
        <nav aria-label="Navigation principale">
          {isReady && isAuthenticated ? (
            <>
              <Link className="header-link" to="/app">Mon espace</Link>
              <button
                className="header-link header-button"
                type="button"
                onClick={() => {
                  setLogoutError('')
                  void logout().catch(() => {
                    setLogoutError('La déconnexion a échoué.')
                  })
                }}
              >
                Se déconnecter
              </button>
            </>
          ) : (
            <>
              <span className="header-note">Des rencontres, à votre rythme.</span>
              <Link className="header-link" to="/login">Connexion</Link>
            </>
          )}
        </nav>
      </header>

      <main className="main-content">
        {bootstrapError && (
          <p className="global-notice" role="status">{bootstrapError}</p>
        )}
        {logoutError && (
          <p className="global-notice error-message" role="alert">{logoutError}</p>
        )}
        <AnimatePresence mode="wait" initial={false}>
          <Routes location={location} key={location.pathname}>
            <Route path="/" element={<Navigate to={isAuthenticated ? '/app' : '/login'} replace />} />
            <Route
              path="/login"
              element={isReady && isAuthenticated ? <Navigate to="/app" replace /> : <LoginPage />}
            />
            <Route
              path="/register"
              element={isReady && isAuthenticated ? <Navigate to="/app" replace /> : <RegisterPage />}
            />
            <Route path="/resend-verification" element={<ResendVerificationPage />} />
            <Route path="/verify-email" element={<VerifyEmailPage />} />
            <Route path="/forgot-password" element={<ForgotPasswordPage />} />
            <Route path="/reset-password" element={<ResetPasswordPage />} />
            <Route
              path="/app"
              element={
                <RequireSession>
                  <ProfilePage />
                </RequireSession>
              }
            />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Routes>
        </AnimatePresence>
      </main>

      <footer className="site-footer">
        <span>Un espace pour se rencontrer autrement.</span>
        <button
          className="avatar-interaction"
          type="button"
          onClick={() => setAvatarReaction((current) => current + 1)}
          aria-label="Faire réagir le petit avatar"
        >
          <span aria-hidden="true">✦</span> Faire coucou
        </button>
      </footer>
    </div>
  )
}

function App() {
  return (
    <AuthProvider>
      <BrowserRouter>
        <AnimatedRoutes />
      </BrowserRouter>
    </AuthProvider>
  )
}

export default App
