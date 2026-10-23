import { AnimatePresence } from 'motion/react'
import { useEffect, useRef, useState } from 'react'
import type { CSSProperties, ReactNode } from 'react'
import {
  BrowserRouter,
  Link,
  Navigate,
  Route,
  Routes,
  useLocation,
  useNavigate,
} from 'react-router-dom'
import { HeartHub } from './components/heart-stage/HeartHub'
import { HeartStage } from './components/heart-stage/HeartStage'
import {
  computeHeartLayout,
  useViewportSize,
  type HeartLayout,
  type HeartLayoutMode,
} from './components/heart-stage/heartLayout'
import { AuthProvider } from './context/AuthProvider'
import { HeartProvider } from './context/HeartProvider'
import { useAuth } from './context/useAuth'
import { useHeart, useHeartMood } from './context/useHeart'
import { FeaturePanel, HubPage } from './pages/AppPages'
import {
  ForgotPasswordPage,
  LoginPage,
  RegisterPage,
  ResendVerificationPage,
  ResetPasswordPage,
  VerifyEmailPage,
} from './pages/AuthPages'
import './App.css'

function layoutModeFor(pathname: string): HeartLayoutMode {
  if (pathname === '/app')
    return 'hub'
  if (pathname.startsWith('/app/'))
    return 'side'
  return 'stage'
}

function RequireSession({ children }: { children: ReactNode }) {
  const { isReady, isAuthenticated } = useAuth()

  if (!isReady)
    return <p className="session-loading">On réveille votre espace…</p>

  return isAuthenticated ? <>{children}</> : <Navigate to="/login" replace />
}

/**
 * Zone cliquable posée sur le cœur 3D : c'est un vrai bouton,
 * donc il est atteignable au clavier et lisible par les lecteurs d'écran.
 */
function HeartButton({ layout, mode }: { layout: HeartLayout; mode: HeartLayoutMode }) {
  const { react } = useHeart()
  const mood = useHeartMood()
  const navigate = useNavigate()

  return (
    <>
      <button
        className="heart-button"
        type="button"
        style={{
          left: layout.x - layout.size / 2,
          top: layout.y - layout.size / 2,
          width: layout.size,
          height: layout.size,
        }}
        aria-label={mode === 'side' ? 'Revenir au cœur' : 'Dire bonjour au cœur'}
        onClick={() => {
          react('poke')
          if (mode === 'side')
            navigate('/app')
        }}
      />
      {mood === 'sleepy' && (
        <span
          className="heart-zzz"
          style={{ left: layout.x + layout.size * 0.32, top: layout.y - layout.size * 0.5 }}
          aria-hidden="true"
        >
          <span>z</span><span>z</span><span>z</span>
        </span>
      )}
    </>
  )
}

function Shell() {
  const location = useLocation()
  const { bootstrapError, isAuthenticated, isReady, logout } = useAuth()
  const { react } = useHeart()
  const viewport = useViewportSize()
  const [logoutError, setLogoutError] = useState('')
  const mode = layoutModeFor(location.pathname)
  const layout = computeHeartLayout(mode, viewport)

  // Le cœur saute de joie à la connexion (pas lors d'une simple restauration de session)
  const wasAuthenticated = useRef<boolean | null>(null)
  useEffect(() => {
    if (!isReady)
      return
    if (wasAuthenticated.current === false && isAuthenticated)
      react('joy')
    wasAuthenticated.current = isAuthenticated
  }, [isReady, isAuthenticated, react])

  const style = {
    '--heart-x': `${layout.x}px`,
    '--heart-y': `${layout.y}px`,
    '--heart-size': `${layout.size}px`,
  } as CSSProperties

  return (
    <div className={`shell shell-${mode}`} style={style}>
      <HeartStage layout={layout} viewport={viewport} />
      <HeartButton layout={layout} mode={mode} />

      <header className="topline">
        <Link className="brand" to={isAuthenticated ? '/app' : '/login'}>matcha</Link>
        {isReady && isAuthenticated && (
          <button
            className="topline-action"
            type="button"
            onClick={() => {
              setLogoutError('')
              void logout().catch(() => setLogoutError('La déconnexion a échoué.'))
            }}
          >
            Se déconnecter
          </button>
        )}
      </header>

      {(bootstrapError || logoutError) && (
        <p className="global-notice" role="alert">{logoutError || bootstrapError}</p>
      )}

      <AnimatePresence>
        {mode === 'hub' && isAuthenticated && <HeartHub layout={layout} viewport={viewport} />}
      </AnimatePresence>

      <main className="panel-area">
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
            <Route path="/app" element={<RequireSession><HubPage /></RequireSession>} />
            <Route path="/app/:feature" element={<RequireSession><FeaturePanel /></RequireSession>} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Routes>
        </AnimatePresence>
      </main>
    </div>
  )
}

function App() {
  return (
    <AuthProvider>
      <HeartProvider>
        <BrowserRouter>
          <Shell />
        </BrowserRouter>
      </HeartProvider>
    </AuthProvider>
  )
}

export default App
