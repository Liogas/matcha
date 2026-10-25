import { useCallback, useRef, useState } from 'react'
import type { Toast, ToastInput } from './types'

/** File de toasts locale au composant qui l'appelle (en général, la racine de l'app). */
export function useToasts(limit = 4) {
  const [toasts, setToasts] = useState<Toast[]>([])
  const nextId = useRef(0)

  const push = useCallback((toast: ToastInput) => {
    nextId.current += 1
    const id = toast.id ?? `toast-${nextId.current}`
    setToasts((current) => [...current.filter((item) => item.id !== id), { ...toast, id }].slice(-limit))
    return id
  }, [limit])

  const dismiss = useCallback((id: Toast['id']) => {
    setToasts((current) => current.filter((toast) => toast.id !== id))
  }, [])

  const clear = useCallback(() => setToasts([]), [])

  return { toasts, push, dismiss, clear }
}
