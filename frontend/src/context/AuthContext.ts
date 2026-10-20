import { createContext } from 'react'
import type { AuthUser } from '../api/auth'

export interface AuthContextValue {
  accessToken: string | null
  bootstrapError: string
  isAuthenticated: boolean
  isReady: boolean
  user: AuthUser | null
  login: (email: string, password: string) => Promise<void>
  logout: () => Promise<void>
  logoutAll: () => Promise<void>
}

export const AuthContext = createContext<AuthContextValue | null>(null)
