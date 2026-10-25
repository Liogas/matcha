import { useLayoutEffect, useRef, useState } from 'react'
import type { FormEvent, KeyboardEvent } from 'react'
import './messaging.css'

export interface MessageComposerProps {
  /** Si la promesse échoue, le brouillon est conservé. */
  onSend: (body: string) => Promise<void> | void
  placeholder?: string
  maxLength?: number
  disabled?: boolean
  labels?: { send: string; field: string; sending: string }
}

const DEFAULT_LABELS = { send: 'Envoyer', field: 'Message', sending: 'Envoi…' }
const MAX_ROWS_HEIGHT = 160

/** Zone de saisie qui grandit avec le texte. Entrée envoie, Maj+Entrée va à la ligne. */
export function MessageComposer({
  onSend,
  placeholder = 'Écrire un message…',
  maxLength = 2000,
  disabled = false,
  labels: labelOverrides,
}: MessageComposerProps) {
  const labels = { ...DEFAULT_LABELS, ...labelOverrides }
  const [draft, setDraft] = useState('')
  const [sending, setSending] = useState(false)
  const fieldRef = useRef<HTMLTextAreaElement>(null)

  useLayoutEffect(() => {
    const field = fieldRef.current
    if (!field) return
    field.style.height = 'auto'
    field.style.height = `${Math.min(field.scrollHeight, MAX_ROWS_HEIGHT)}px`
  }, [draft])

  async function submit(event?: FormEvent) {
    event?.preventDefault()
    const body = draft.trim()
    if (!body || sending || disabled) return
    setSending(true)
    try {
      await onSend(body)
      setDraft('')
    } catch {
      // L'appelant affiche l'erreur ; on garde le texte pour un nouvel essai
    } finally {
      setSending(false)
      fieldRef.current?.focus()
    }
  }

  function onKeyDown(event: KeyboardEvent<HTMLTextAreaElement>) {
    if (event.key === 'Enter' && !event.shiftKey && !event.nativeEvent.isComposing) {
      event.preventDefault()
      void submit()
    }
  }

  return (
    <form className="uik-composer" onSubmit={(event) => void submit(event)}>
      <textarea
        ref={fieldRef}
        className="uik-composer__field"
        rows={1}
        value={draft}
        maxLength={maxLength}
        placeholder={placeholder}
        aria-label={labels.field}
        disabled={disabled}
        onChange={(event) => setDraft(event.target.value)}
        onKeyDown={onKeyDown}
      />
      <button className="uik-composer__send" type="submit" disabled={disabled || sending || !draft.trim()}>
        {sending ? labels.sending : labels.send}
      </button>
    </form>
  )
}
