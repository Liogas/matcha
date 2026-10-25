import { useState } from 'react'
import type { FormEvent } from 'react'
import { Link, useSearchParams } from 'react-router-dom'
import { confirmPasswordReset, describeApiError } from '../../api/auth'
import { PagePanel } from '../../layout/PagePanel'
import { Feedback } from '../../shared/ui/Feedback'
import { validatePassword } from './passwordRules'

export function ResetPasswordPage() {
  const [searchParams] = useSearchParams()
  const token = searchParams.get('token') ?? ''
  const [password, setPassword] = useState('')
  const [confirmation, setConfirmation] = useState('')
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')
  const [isSubmitting, setIsSubmitting] = useState(false)

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    setError('')
    setSuccess('')
    const passwordError = validatePassword(password)
    if (passwordError) {
      setError(passwordError)
      return
    }
    if (password !== confirmation) {
      setError('Les deux mots de passe ne correspondent pas.')
      return
    }

    setIsSubmitting(true)
    try {
      await confirmPasswordReset(token, password)
      setSuccess('Mot de passe mis à jour. Connectez-vous à nouveau.')
    } catch (submitError) {
      setError(describeApiError(submitError))
    } finally {
      setIsSubmitting(false)
    }
  }

  return (
    <PagePanel
      size="narrow"
      eyebrow="Nouveau départ"
      title="Choisissez un mot de passe."
      description="Le lien est valable 30 minutes et ne peut être utilisé qu’une fois."
      footer={<Link to="/login">Retour à la connexion</Link>}
    >
      {!token ? (
        <Feedback message="Le lien de réinitialisation ne contient pas de jeton." />
      ) : success ? (
        <div className="stack">
          <Feedback message={success} kind="success" />
          <Link className="button button--primary button--block" to="/login">Continuer vers la connexion</Link>
        </div>
      ) : (
        <form className="form" onSubmit={(event) => void handleSubmit(event)}>
          <label className="field">
            Nouveau mot de passe
            <input
              type="password"
              autoComplete="new-password"
              value={password}
              onChange={(event) => setPassword(event.target.value)}
              minLength={12}
              maxLength={128}
              required
            />
          </label>
          <label className="field">
            Confirmer le mot de passe
            <input
              type="password"
              autoComplete="new-password"
              value={confirmation}
              onChange={(event) => setConfirmation(event.target.value)}
              minLength={12}
              maxLength={128}
              required
            />
          </label>
          <Feedback message={error} />
          <button className="button button--primary button--block" disabled={isSubmitting}>
            {isSubmitting ? 'Mise à jour…' : 'Choisir ce mot de passe'}
          </button>
        </form>
      )}
    </PagePanel>
  )
}
