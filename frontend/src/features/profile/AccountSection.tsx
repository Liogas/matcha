import { useState } from 'react'
import { useAuth } from '../../context/useAuth'
import { Feedback } from '../../shared/ui/Feedback'

export function AccountSection() {
  const { user, logoutAll } = useAuth()
  const [confirming, setConfirming] = useState(false)
  const [isBusy, setIsBusy] = useState(false)
  const [error, setError] = useState('')

  return (
    <section className="section">
      <h2>Compte</h2>
      {user?.email && <p className="section__hint">Connecté·e avec {user.email}</p>}
      <div className="button-row">
        {!confirming ? (
          <button className="button button--ghost" type="button" onClick={() => setConfirming(true)}>
            Se déconnecter de tous les appareils
          </button>
        ) : (
          <>
            <span className="form__hint">Toutes vos sessions seront fermées, y compris celle-ci.</span>
            <button
              className="button button--danger"
              type="button"
              disabled={isBusy}
              onClick={() => {
                setIsBusy(true)
                setError('')
                void logoutAll().catch(() => {
                  setError('La déconnexion globale a échoué.')
                  setIsBusy(false)
                })
              }}
            >
              {isBusy ? '…' : 'Confirmer'}
            </button>
            <button className="button button--ghost" type="button" onClick={() => setConfirming(false)}>
              Annuler
            </button>
          </>
        )}
      </div>
      <Feedback message={error} />
    </section>
  )
}
