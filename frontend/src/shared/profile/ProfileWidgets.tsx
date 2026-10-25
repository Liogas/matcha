import { useState } from 'react'
import type { ReactNode } from 'react'
import { Link } from 'react-router-dom'
import { describeApiError, type Profile } from '../../api/auth'
import { useRelation, useSocial } from '../../context/useSocial'
import { Feedback } from '../ui/Feedback'
import { ProfileImage } from './ProfileImage'
import { displayName, genderLabel, orientationLabel } from './profileFormat'
import './profile.css'

export function RelationChip({ accountId }: { accountId: number }) {
  const { liked, likesMe, matched } = useRelation(accountId)
  if (matched)
    return <span className="chip chip--match">Match</span>
  if (likesMe)
    return <span className="chip chip--incoming">Vous a liké</span>
  if (liked)
    return <span className="chip">Liké</span>
  return null
}

export function LikeButton({ accountId }: { accountId: number }) {
  const { like, unlike, myProfile } = useSocial()
  const { liked, likesMe, matched } = useRelation(accountId)
  const [isBusy, setIsBusy] = useState(false)
  const [confirmUnmatch, setConfirmUnmatch] = useState(false)
  const [error, setError] = useState('')
  const canLike = Boolean(myProfile?.completed)

  async function run(action: () => Promise<unknown>) {
    setError('')
    setIsBusy(true)
    try {
      await action()
    } catch (actionError) {
      setError(describeApiError(actionError))
    } finally {
      setIsBusy(false)
      setConfirmUnmatch(false)
    }
  }

  if (!liked && !canLike) {
    return (
      <p className="form__hint">
        <Link to="/profil">Complétez votre profil</Link> pour pouvoir liker.
      </p>
    )
  }

  return (
    <div className="stack">
      <div className="button-row">
        {!liked ? (
          <button
            className="button button--primary"
            type="button"
            disabled={isBusy}
            onClick={() => void run(() => like(accountId))}
          >
            {isBusy ? '…' : likesMe ? 'Liker en retour' : 'J’aime'}
          </button>
        ) : matched && !confirmUnmatch ? (
          <button className="button button--ghost" type="button" onClick={() => setConfirmUnmatch(true)}>
            Annuler le match
          </button>
        ) : matched ? (
          <>
            <span className="form__hint">La conversation sera supprimée.</span>
            <button
              className="button button--danger"
              type="button"
              disabled={isBusy}
              onClick={() => void run(() => unlike(accountId))}
            >
              {isBusy ? '…' : 'Confirmer'}
            </button>
            <button className="button button--ghost" type="button" onClick={() => setConfirmUnmatch(false)}>
              Garder
            </button>
          </>
        ) : (
          <button
            className="button button--ghost"
            type="button"
            disabled={isBusy}
            onClick={() => void run(() => unlike(accountId))}
          >
            {isBusy ? '…' : 'Retirer mon like'}
          </button>
        )}
      </div>
      <Feedback message={error} />
    </div>
  )
}

/** Vignette cliquable d'un profil, utilisée dans les grilles. */
export function ProfileTile({
  profile,
  onOpen,
}: {
  profile: Profile
  onOpen: (accountId: number) => void
}) {
  return (
    <button className="profile-tile" type="button" onClick={() => onOpen(profile.account_id)}>
      <ProfileImage className="profile-tile__photo" path={profile.photo_url} name={profile.first_name} />
      <span className="profile-tile__body">
        <span className="profile-tile__name">{displayName(profile)}</span>
        <span className="profile-tile__city">{profile.city}</span>
        <RelationChip accountId={profile.account_id} />
      </span>
    </button>
  )
}

/** Ligne compacte d'un profil, pour les listes (matchs, likes). */
export function ProfileRow({
  accountId,
  onOpen,
  action,
}: {
  accountId: number
  onOpen: (accountId: number) => void
  action?: ReactNode
}) {
  const { profilesById } = useSocial()
  const profile = profilesById[accountId]

  if (profile === null) {
    return (
      <li className="profile-row profile-row--missing">
        Profil indisponible (n° {accountId})
      </li>
    )
  }

  return (
    <li className="profile-row">
      <button className="profile-row__main" type="button" onClick={() => onOpen(accountId)}>
        <ProfileImage className="portrait--avatar" path={profile?.photo_url} name={profile?.first_name ?? '?'} />
        <span className="profile-row__text">
          <strong>{profile ? displayName(profile) : '…'}</strong>
          <small>{profile?.city}</small>
        </span>
        <RelationChip accountId={accountId} />
      </button>
      {action}
    </li>
  )
}

export function ProfileTags({ profile }: { profile: Profile }) {
  return (
    <div className="chip-row">
      {profile.gender && <span className="chip">{genderLabel(profile.gender)}</span>}
      {profile.sexual_orientation && <span className="chip">{orientationLabel(profile.sexual_orientation)}</span>}
      {profile.city && <span className="chip">{profile.city}</span>}
    </div>
  )
}
