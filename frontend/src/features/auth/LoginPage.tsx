import { useState } from 'react'
import type { FormEvent } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { describeApiError } from '../../api/auth'
import { useAuth } from '../../context/useAuth'
import { HOME_PATH } from '../../layout/navigation'
import { PagePanel } from '../../layout/PagePanel'
import { Feedback } from '../../shared/ui/Feedback'

export function LoginPage() {
  const { isReady, login } = useAuth()
  const navigate = useNavigate()
  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [error, setError] = useState('')
  const [isSubmitting, setIsSubmitting] = useState(false)

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    setError('')
    setIsSubmitting(true)
    try {
      await login(email.trim(), password)
      navigate(HOME_PATH, { replace: true })
    } catch (submitError) {
      setError(describeApiError(submitError))
    } finally {
      setIsSubmitting(false)
    }
  }

  return (
    <PagePanel
      size="narrow"
      eyebrow="Bon retour"
      title="On reprend le sentier ?"
      description="Connectez-vous pour retrouver votre espace."
      footer={<>Pas encore de compte ? <Link to="/register">Créer un compte</Link></>}
    >
      <form className="form" onSubmit={(event) => void handleSubmit(event)}>
        <label className="field">
          Adresse e-mail
          <input
            type="email"
            name="email"
            autoComplete="email"
            value={email}
            onChange={(event) => setEmail(event.target.value)}
            required
          />
        </label>
        <label className="field">
          Mot de passe
          <input
            type="password"
            name="password"
            autoComplete="current-password"
            value={password}
            onChange={(event) => setPassword(event.target.value)}
            required
          />
        </label>
        <div className="form__links">
          <Link to="/forgot-password">Mot de passe oublié ?</Link>
          <Link to="/resend-verification">Renvoyer l’e-mail de vérification</Link>
        </div>
        <Feedback message={error} />
        <button className="button button--primary button--block" disabled={isSubmitting || !isReady}>
          {isSubmitting ? 'Connexion…' : isReady ? 'Se connecter' : 'Vérification de la session…'}
        </button>
      </form>
    </PagePanel>
  )
}
