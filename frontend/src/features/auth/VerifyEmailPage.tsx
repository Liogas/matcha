import { useState } from 'react'
import { Link, useSearchParams } from 'react-router-dom'
import { describeApiError, verifyEmail } from '../../api/auth'
import { PagePanel } from '../../layout/PagePanel'
import { Feedback } from '../../shared/ui/Feedback'

export function VerifyEmailPage() {
  const [searchParams] = useSearchParams()
  const token = searchParams.get('token') ?? ''
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')
  const [isSubmitting, setIsSubmitting] = useState(false)

  async function handleVerify() {
    setError('')
    setIsSubmitting(true)
    try {
      await verifyEmail(token)
      setSuccess('Votre adresse e-mail est confirmée. Vous pouvez maintenant vous connecter.')
    } catch (submitError) {
      setError(describeApiError(submitError))
    } finally {
      setIsSubmitting(false)
    }
  }

  return (
    <PagePanel
      size="narrow"
      eyebrow="Dernière étape"
      title={success ? 'Adresse confirmée.' : 'Confirmer votre adresse ?'}
      description={success
        ? 'Votre compte est prêt. Vous pouvez maintenant vous connecter.'
        : 'Cliquez pour confirmer votre adresse e-mail et activer votre compte.'}
      footer={<Link to="/resend-verification">Besoin d’un nouveau lien ?</Link>}
    >
      {!token ? (
        <Feedback message="Le lien de confirmation ne contient pas de jeton." />
      ) : success ? (
        <div className="stack">
          <Feedback message={success} kind="success" />
          <Link className="button button--primary button--block" to="/login">Se connecter</Link>
        </div>
      ) : (
        <div className="form">
          <Feedback message={error} />
          <button
            className="button button--primary button--block"
            type="button"
            onClick={() => void handleVerify()}
            disabled={isSubmitting}
          >
            {isSubmitting ? 'Confirmation…' : 'Confirmer mon adresse'}
          </button>
        </div>
      )}
    </PagePanel>
  )
}
