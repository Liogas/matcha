import { AnimatePresence } from 'motion/react'
import { Navigate, Route, Routes, useLocation } from 'react-router-dom'
import { useAuth } from '../context/useAuth'
import { ForgotPasswordPage } from '../features/auth/ForgotPasswordPage'
import { LoginPage } from '../features/auth/LoginPage'
import { RegisterPage } from '../features/auth/RegisterPage'
import { ResendVerificationPage } from '../features/auth/ResendVerificationPage'
import { ResetPasswordPage } from '../features/auth/ResetPasswordPage'
import { VerifyEmailPage } from '../features/auth/VerifyEmailPage'
import { DiscoverPage } from '../features/discover/DiscoverPage'
import { LegalPage } from '../features/legal/LegalPage'
import { MatchesPage } from '../features/matches/MatchesPage'
import { MessagesPage } from '../features/messages/MessagesPage'
import { ProfilePage } from '../features/profile/ProfilePage'
import { HOME_PATH } from '../layout/navigation'
import { GuestOnly, RequireSession } from './RouteGuards'

export function AppRoutes() {
  const location = useLocation()
  const { isAuthenticated } = useAuth()

  return (
    <AnimatePresence mode="wait" initial={false}>
      <Routes location={location} key={location.pathname}>
        <Route path="/" element={<Navigate to={isAuthenticated ? HOME_PATH : '/login'} replace />} />

        <Route path="/login" element={<GuestOnly><LoginPage /></GuestOnly>} />
        <Route path="/register" element={<GuestOnly><RegisterPage /></GuestOnly>} />
        {/* Ces adresses figurent dans les e-mails envoyés par le backend : ne pas les renommer */}
        <Route path="/resend-verification" element={<ResendVerificationPage />} />
        <Route path="/verify-email" element={<VerifyEmailPage />} />
        <Route path="/forgot-password" element={<ForgotPasswordPage />} />
        <Route path="/reset-password" element={<ResetPasswordPage />} />

        <Route path="/profil" element={<RequireSession><ProfilePage /></RequireSession>} />
        <Route path="/decouvrir" element={<RequireSession><DiscoverPage /></RequireSession>} />
        <Route path="/matchs" element={<RequireSession><MatchesPage /></RequireSession>} />
        <Route path="/messages" element={<RequireSession><MessagesPage /></RequireSession>} />

        <Route path="/infos/:page" element={<LegalPage />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </AnimatePresence>
  )
}
