import { AnimatePresence, motion, useReducedMotion } from 'motion/react'
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

function SessionPage() {
  const { logout, logoutAll, user } = useAuth()
  const shouldReduceMotion = useReducedMotion()
  const [message, setMessage] = useState('')

  async function handleLogout() {
    setMessage('')
    try {
      await logout()
    } catch {
      setMessage('La déconnexion a échoué. Réessayez.')
    }
  }

  async function handleLogoutAll() {
    setMessage('')
    try {
      await logoutAll()
    } catch {
      setMessage('La révocation des sessions a échoué. Réessayez.')
    }
  }

  return (
    <motion.section
      className="auth-panel session-panel"
      initial={shouldReduceMotion ? false : { opacity: 0, y: 18, filter: 'blur(8px)' }}
      animate={{ opacity: 1, y: 0, filter: 'blur(0px)' }}
      exit={shouldReduceMotion ? undefined : { opacity: 0, y: -12, filter: 'blur(8px)' }}
      transition={{ duration: shouldReduceMotion ? 0 : 0.3, ease: 'easeOut' }}
    >
      <p className="eyebrow">ESPACE PERSONNEL</p>
      <h1>Vous voilà.</h1>
      <p className="panel-description">
        Votre session est active. Le profil Matcha et son avatar personnalisable
        arriveront dans une prochaine étape.
      </p>
      <div className="session-card">
        <span className="status-dot" />
        <div>
          <strong>Session sécurisée</strong>
          <span>{user?.email ?? 'Compte connecté'}</span>
        </div>
      </div>
      {message && <p className="form-message error-message">{message}</p>}
      <div className="session-actions">
        <button className="button button-primary" onClick={handleLogout}>
          Se déconnecter
        </button>
        <button className="button button-secondary" onClick={handleLogoutAll}>
          Déconnecter tous les appareils
        </button>
      </div>
    </motion.section>
  )
}

function AnimatedRoutes() {
  const location = useLocation()
  const { bootstrapError, isAuthenticated, isReady } = useAuth()
  const shouldReduceMotion = useReducedMotion()
  const [avatarReaction, setAvatarReaction] = useState(0)
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
            <Link className="header-link" to="/app">Mon espace</Link>
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
                  <SessionPage />
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
