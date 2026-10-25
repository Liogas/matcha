import { useState } from 'react'
import type { FormEvent } from 'react'
import { Link, useSearchParams } from 'react-router-dom'
import { describeApiError, resendVerification } from '../../api/auth'
import { PagePanel } from '../../layout/PagePanel'
import { Feedback } from '../../shared/ui/Feedback'

export function ResendVerificationPage() {
  const [searchParams] = useSearchParams()
  const [email, setEmail] = useState(searchParams.get('email') ?? '')
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')
  const [isSubmitting, setIsSubmitting] = useState(false)

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    setError('')
    setSuccess('')
    setIsSubmitting(true)
    try {
      await resendVerification(email.trim())
      setSuccess('Si un compte non vérifié existe à cette adresse, un e-mail vient d’être envoyé.')
    } catch (submitError) {
      setError(describeApiError(submitError))
    } finally {
      setIsSubmitting(false)
    }
  }

  return (
    <PagePanel
      size="narrow"
      eyebrow="Vérification"
      title="Un nouveau lien ?"
      description="Saisissez votre adresse. Pour protéger votre compte, le message affiché ne révèle pas si un compte existe."
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
          {isSubmitting ? 'Envoi…' : 'Renvoyer le lien'}
        </button>
      </form>
    </PagePanel>
  )
}
