import { useState } from 'react'
import type { FormEvent } from 'react'
import { Link } from 'react-router-dom'
import { ApiError, describeApiError, registerAccount } from '../../api/auth'
import { PagePanel } from '../../layout/PagePanel'
import { Feedback } from '../../shared/ui/Feedback'
import { validatePassword } from './passwordRules'

export function RegisterPage() {
  const [form, setForm] = useState({
    email: '',
    username: '',
    first_name: '',
    last_name: '',
    password: '',
    passwordConfirmation: '',
  })
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')
  const [showResendLink, setShowResendLink] = useState(false)
  const [isSubmitting, setIsSubmitting] = useState(false)

  function updateField(field: keyof typeof form, value: string) {
    setForm((current) => ({ ...current, [field]: value }))
  }

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    setError('')
    setSuccess('')
    setShowResendLink(false)
    const passwordError = validatePassword(form.password)
    if (passwordError) {
      setError(passwordError)
      return
    }
    if (form.password !== form.passwordConfirmation) {
      setError('Les deux mots de passe ne correspondent pas.')
      return
    }

    setIsSubmitting(true)
    try {
      await registerAccount({
        email: form.email.trim(),
        username: form.username.trim(),
        first_name: form.first_name.trim(),
        last_name: form.last_name.trim(),
        password: form.password,
      })
      setSuccess('Compte créé. Consultez votre boîte mail pour confirmer votre adresse.')
    } catch (submitError) {
      setError(describeApiError(submitError))
      setShowResendLink(
        submitError instanceof ApiError &&
          /account created, but verification email delivery failed/i.test(submitError.message),
      )
    } finally {
      setIsSubmitting(false)
    }
  }

  const resendLink = `/resend-verification?email=${encodeURIComponent(form.email.trim())}`

  return (
    <PagePanel
      size="narrow"
      eyebrow="Bienvenue"
      title="Faites votre place dans la clairière."
      description="Quelques informations pour commencer. Votre profil pourra être complété ensuite."
      footer={<>Déjà inscrit·e ? <Link to="/login">Se connecter</Link></>}
    >
      {success ? (
        <div className="stack">
          <Feedback message={success} kind="success" />
          <p className="muted">Le lien de confirmation reste valable 24 heures.</p>
          <Link className="button button--ghost button--block" to={resendLink}>Renvoyer le lien</Link>
        </div>
      ) : (
        <form className="form" onSubmit={(event) => void handleSubmit(event)}>
          <div className="form__row">
            <label className="field">
              Prénom
              <input
                name="first_name"
                autoComplete="given-name"
                value={form.first_name}
                onChange={(event) => updateField('first_name', event.target.value)}
                maxLength={80}
                required
              />
            </label>
            <label className="field">
              Nom
              <input
                name="last_name"
                autoComplete="family-name"
                value={form.last_name}
                onChange={(event) => updateField('last_name', event.target.value)}
                maxLength={80}
                required
              />
            </label>
          </div>
          <label className="field">
            Nom d’utilisateur
            <input
              name="username"
              autoComplete="username"
              value={form.username}
              onChange={(event) => updateField('username', event.target.value)}
              minLength={3}
              maxLength={30}
              pattern="[A-Za-z0-9_]+"
              required
            />
            <span className="form__hint">3 à 30 lettres, chiffres ou _</span>
          </label>
          <label className="field">
            Adresse e-mail
            <input
              type="email"
              name="email"
              autoComplete="email"
              value={form.email}
              onChange={(event) => updateField('email', event.target.value)}
              maxLength={254}
              required
            />
          </label>
          <label className="field">
            Mot de passe
            <input
              type="password"
              name="password"
              autoComplete="new-password"
              value={form.password}
              onChange={(event) => updateField('password', event.target.value)}
              minLength={12}
              maxLength={128}
              required
            />
            <span className="form__hint">12 à 128 caractères</span>
          </label>
          <label className="field">
            Confirmer le mot de passe
            <input
              type="password"
              name="password_confirmation"
              autoComplete="new-password"
              value={form.passwordConfirmation}
              onChange={(event) => updateField('passwordConfirmation', event.target.value)}
              minLength={12}
              maxLength={128}
              required
            />
          </label>
          <Feedback message={error} />
          {showResendLink && <Link to={resendLink}>Renvoyer l’e-mail de vérification</Link>}
          <button className="button button--primary button--block" disabled={isSubmitting}>
            {isSubmitting ? 'Création…' : 'Créer mon compte'}
          </button>
        </form>
      )}
    </PagePanel>
  )
}
