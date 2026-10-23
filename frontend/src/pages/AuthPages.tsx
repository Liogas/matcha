import { motion, useReducedMotion } from 'motion/react'
import { useEffect, useState } from 'react'
import type { FormEvent } from 'react'
import type { ReactNode } from 'react'
import { Link, useNavigate, useSearchParams } from 'react-router-dom'
import {
  ApiError,
  confirmPasswordReset,
  describeApiError,
  registerAccount,
  requestPasswordReset,
  resendVerification,
  verifyEmail,
} from '../api/auth'
import { useAuth } from '../context/useAuth'
import { useHeart } from '../context/useHeart'

function PagePanel({
  eyebrow,
  title,
  description,
  children,
  footer,
}: {
  eyebrow: string
  title: string
  description: string
  children: ReactNode
  footer?: ReactNode
}) {
  const reduceMotion = useReducedMotion()
  return (
    <motion.section
      className="auth-panel"
      initial={reduceMotion ? false : { opacity: 0, y: 22, filter: 'blur(8px)' }}
      animate={{ opacity: 1, y: 0, filter: 'blur(0px)' }}
      exit={reduceMotion ? undefined : { opacity: 0, y: -14, filter: 'blur(8px)' }}
      transition={{ duration: reduceMotion ? 0 : 0.34, ease: [0.22, 1, 0.36, 1] }}
    >
      <p className="eyebrow">{eyebrow}</p>
      <h1>{title}</h1>
      <p className="panel-description">{description}</p>
      {children}
      {footer && <div className="panel-footer">{footer}</div>}
    </motion.section>
  )
}

function Feedback({
  message,
  kind = 'error',
}: {
  message: string
  kind?: 'error' | 'success'
}) {
  const { react } = useHeart()

  // Le cœur réagit à chaque nouveau message : tristesse pour une erreur, acquiescement sinon
  useEffect(() => {
    if (message)
      react(kind === 'success' ? 'nod' : 'sad')
  }, [message, kind, react])

  if (!message) return null
  return (
    <p
      className={`form-message ${kind === 'success' ? 'success-message' : 'error-message'}`}
      role={kind === 'success' ? 'status' : 'alert'}
    >
      {message}
    </p>
  )
}

function validatePassword(password: string) {
  const length = Array.from(password).length
  if (length < 12 || length > 128) {
    return 'Choisissez un mot de passe entre 12 et 128 caractères.'
  }
  return ''
}

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
      navigate('/app', { replace: true })
    } catch (submitError) {
      setError(describeApiError(submitError))
    } finally {
      setIsSubmitting(false)
    }
  }

  return (
    <PagePanel
      eyebrow="RAVI DE VOUS REVOIR"
      title="On reprend ?"
      description="Connectez-vous pour retrouver votre espace."
      footer={<>Pas encore de compte ? <Link to="/register">Créer un compte</Link></>}
    >
      <form className="auth-form" onSubmit={handleSubmit}>
        <label>
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
        <label>
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
        <div className="form-links">
          <Link to="/forgot-password">Mot de passe oublié ?</Link>
          <Link to="/resend-verification">Renvoyer l’e-mail de vérification</Link>
        </div>
        <Feedback message={error} />
        <button className="button button-primary" disabled={isSubmitting || !isReady}>
          {isSubmitting ? 'Connexion…' : isReady ? 'Se connecter' : 'Vérification de la session…'}
        </button>
      </form>
    </PagePanel>
  )
}

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
          /account created, but verification email delivery failed/i.test(
            submitError.message,
          ),
      )
    } finally {
      setIsSubmitting(false)
    }
  }

  return (
    <PagePanel
      eyebrow="BIENVENUE"
      title="Créons votre espace."
      description="Quelques informations pour commencer. Votre profil pourra être complété ensuite."
      footer={<>Déjà inscrit·e ? <Link to="/login">Se connecter</Link></>}
    >
      {success ? (
        <div className="success-card">
          <Feedback message={success} kind="success" />
          <p>Le lien de confirmation reste valable 24 heures.</p>
          <Link className="button button-primary button-link" to={`/resend-verification?email=${encodeURIComponent(form.email.trim())}`}>
            Renvoyer le lien
          </Link>
        </div>
      ) : (
        <form className="auth-form" onSubmit={handleSubmit}>
          <div className="form-grid">
            <label>
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
            <label>
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
          <label>
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
            <span className="field-hint">3 à 30 lettres, chiffres ou _</span>
          </label>
          <label>
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
          <label>
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
            <span className="field-hint">12 à 128 caractères</span>
          </label>
          <label>
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
          {showResendLink && (
            <Link
              className="form-recovery-link"
              to={`/resend-verification?email=${encodeURIComponent(form.email.trim())}`}
            >
              Renvoyer l’e-mail de vérification
            </Link>
          )}
          <button className="button button-primary" disabled={isSubmitting}>
            {isSubmitting ? 'Création…' : 'Créer mon compte'}
          </button>
        </form>
      )}
    </PagePanel>
  )
}

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
      eyebrow="VÉRIFICATION"
      title="Un nouveau lien ?"
      description="Saisissez votre adresse. Pour protéger votre compte, le message affiché ne révèle pas si un compte existe."
      footer={<Link to="/login">Retour à la connexion</Link>}
    >
      <form className="auth-form" onSubmit={handleSubmit}>
        <label>
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
        <button className="button button-primary" disabled={isSubmitting}>
          {isSubmitting ? 'Envoi…' : 'Renvoyer le lien'}
        </button>
      </form>
    </PagePanel>
  )
}

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
      eyebrow="DERNIÈRE ÉTAPE"
      title={success ? 'Adresse confirmée.' : 'Confirmer votre adresse ?'}
      description={
        success
          ? 'Votre compte est prêt. Vous pouvez maintenant vous connecter.'
          : 'Cliquez pour confirmer votre adresse e-mail et activer votre compte.'
      }
      footer={<Link to="/resend-verification">Besoin d’un nouveau lien ?</Link>}
    >
      {!token ? (
        <Feedback message="Le lien de confirmation ne contient pas de jeton." />
      ) : success ? (
        <Feedback message={success} kind="success" />
      ) : (
        <div className="auth-form">
          <Feedback message={error} />
          <button
            className="button button-primary"
            onClick={handleVerify}
            disabled={isSubmitting}
          >
            {isSubmitting ? 'Confirmation…' : 'Confirmer mon adresse'}
          </button>
        </div>
      )}
      {success && <Link className="button button-primary button-link" to="/login">Se connecter</Link>}
    </PagePanel>
  )
}

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
      eyebrow="ON S’OCCUPE DE ÇA"
      title="Mot de passe oublié ?"
      description="Entrez l’adresse associée à votre compte. Si un compte existe, vous recevrez un lien pour choisir un nouveau mot de passe."
      footer={<Link to="/login">Retour à la connexion</Link>}
    >
      <form className="auth-form" onSubmit={handleSubmit}>
        <label>
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
        <button className="button button-primary" disabled={isSubmitting}>
          {isSubmitting ? 'Envoi…' : 'Recevoir un lien'}
        </button>
      </form>
    </PagePanel>
  )
}

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
      eyebrow="NOUVEAU DÉPART"
      title="Choisissez un mot de passe."
      description="Le lien est valable 30 minutes et ne peut être utilisé qu’une fois."
      footer={<Link to="/login">Retour à la connexion</Link>}
    >
      {!token ? (
        <Feedback message="Le lien de réinitialisation ne contient pas de jeton." />
      ) : success ? (
        <>
          <Feedback message={success} kind="success" />
          <Link className="button button-primary button-link" to="/login">Continuer vers la connexion</Link>
        </>
      ) : (
        <form className="auth-form" onSubmit={handleSubmit}>
          <label>
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
          <label>
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
          <button className="button button-primary" disabled={isSubmitting}>
            {isSubmitting ? 'Mise à jour…' : 'Choisir ce mot de passe'}
          </button>
        </form>
      )}
    </PagePanel>
  )
}
