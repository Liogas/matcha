import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
} from 'react'
import type { ReactNode } from 'react'
import {
  ApiError,
  getCurrentUser,
  refreshSession,
  signIn,
  signOut,
  signOutAll,
  type AuthUser,
} from '../api/auth'
import { AuthContext, type AuthContextValue } from './AuthContext'

let sessionRestorePromise:
  | Promise<{ accessToken: string; user: AuthUser }>
  | undefined

export function AuthProvider({ children }: { children: ReactNode }) {
  const [accessToken, setAccessToken] = useState<string | null>(null)
  const [user, setUser] = useState<AuthUser | null>(null)
  const [isReady, setIsReady] = useState(false)
  const [bootstrapError, setBootstrapError] = useState('')
  const sessionGeneration = useRef(0)

  const clearSession = useCallback(() => {
    sessionGeneration.current += 1
    setAccessToken(null)
    setUser(null)
  }, [])

  useEffect(() => {
    let active = true

    async function restoreSession() {
      const pendingRestore = sessionRestorePromise ??= (async () => {
        const token = await refreshSession()
        const currentUser = await getCurrentUser(token)
        return { accessToken: token, user: currentUser }
      })()
      void pendingRestore.then(
        () => {
          if (sessionRestorePromise === pendingRestore)
            sessionRestorePromise = undefined
        },
        () => {
          if (sessionRestorePromise === pendingRestore)
            sessionRestorePromise = undefined
        },
      )

      try {
        const session = await pendingRestore
        if (!active) return
        setAccessToken(session.accessToken)
        setUser(session.user)
      } catch (error) {
        if (!active) return
        if (!(error instanceof ApiError && error.status === 401)) {
          setBootstrapError(
            'La session précédente n’a pas pu être vérifiée. Vous pouvez vous reconnecter.',
          )
        }
      } finally {
        if (active) setIsReady(true)
      }
    }

    void restoreSession()
    return () => {
      active = false
    }
  }, [])

  useEffect(() => {
    if (!accessToken) return
    let active = true
    let retryTimeout: number | undefined
    const generation = sessionGeneration.current

    async function renewSession() {
      try {
        const refreshedToken = await refreshSession()
        const refreshedUser = await getCurrentUser(refreshedToken)
        if (!active || generation !== sessionGeneration.current) return
        setAccessToken(refreshedToken)
        setUser((current) => ({
          ...refreshedUser,
          email: current?.email,
        }))
        setBootstrapError('')
      } catch (error) {
        if (!active || generation !== sessionGeneration.current) return
        if (error instanceof ApiError && error.status === 401) {
          clearSession()
          return
        }
        setBootstrapError(
          'La session n’a pas pu être renouvelée. Vérifiez votre connexion.',
        )
        retryTimeout = window.setTimeout(() => void renewSession(), 60_000)
      }
    }

    const timeout = window.setTimeout(() => void renewSession(), 12 * 60 * 1000)
    return () => {
      active = false
      window.clearTimeout(timeout)
      if (retryTimeout !== undefined) window.clearTimeout(retryTimeout)
    }
  }, [accessToken, clearSession])

  const login = useCallback(async (email: string, password: string) => {
    const session = await signIn(email, password)
    sessionGeneration.current += 1
    setAccessToken(session.accessToken)
    setUser(session.user)
    setBootstrapError('')
  }, [])

  const logout = useCallback(async () => {
    await signOut()
    clearSession()
  }, [clearSession])

  const logoutAll = useCallback(async () => {
    if (!accessToken) return
    await signOutAll(accessToken)
    clearSession()
  }, [accessToken, clearSession])

  const value = useMemo<AuthContextValue>(() => ({
    accessToken,
    bootstrapError,
    isAuthenticated: accessToken !== null,
    isReady,
    user,
    login,
    logout,
    logoutAll,
  }), [
    accessToken,
    bootstrapError,
    isReady,
    user,
    login,
    logout,
    logoutAll,
  ])

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>
}
