import { useState } from 'react'
import type { FormEvent } from 'react'
import { Link } from 'react-router-dom'
import { describeApiError, requestPasswordReset } from '../../api/auth'
import { PagePanel } from '../../layout/PagePanel'
import { Feedback } from '../../shared/ui/Feedback'

export function ForgotPasswordPage() {
  const [email, setEmail] = useState('')
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')
  const [isSubmitting, setIsSubmitting] = useState(false)

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    setError('')
    setSuccess('')
    setIsSubmitting(true)
    try {
      await requestPasswordReset(email.trim())
      setSuccess('Si un compte existe à cette adresse, des instructions viennent d’être envoyées.')
    } catch (submitError) {
      setError(describeApiError(submitError))
    } finally {
      setIsSubmitting(false)
    }
  }

  return (
    <PagePanel
      size="narrow"
      eyebrow="Pas d’inquiétude"
      title="Mot de passe oublié ?"
      description="Entrez l’adresse associée à votre compte. Si un compte existe, vous recevrez un lien pour choisir un nouveau mot de passe."
      footer={<Link to="/login">Retour à la connexion</Link>}
    >
      <form className="form" onSubmit={(event) => void handleSubmit(event)}>
        <label className="field">
          Adresse e-mail
          <input
            type="email"
            autoComplete="email"
            value={email}
            onChange={(event) => setEmail(event.target.value)}
            required
          />
        </label>
        <Feedback message={error} />
        <Feedback message={success} kind="success" />
        <button className="button button--primary button--block" disabled={isSubmitting}>
          {isSubmitting ? 'Envoi…' : 'Recevoir un lien'}
        </button>
      </form>
    </PagePanel>
  )
}
