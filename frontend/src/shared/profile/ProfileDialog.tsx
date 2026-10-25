import { motion, useReducedMotion } from 'motion/react'
import { useEffect, useRef, useState } from 'react'
import { createPortal } from 'react-dom'
import { Link } from 'react-router-dom'
import { useRelation, useSocial } from '../../context/useSocial'
import { ProfileImage } from './ProfileImage'
import { LikeButton, ProfileTags, RelationChip } from './ProfileWidgets'
import { displayName } from './profileFormat'
import './profile.css'

/**
 * Fiche complète d'un autre profil, avec toutes ses photos.
 * Rendue dans <body> : le flou du panneau casserait le positionnement fixe.
 */
export function ProfileDialog({
  accountId,
  onClose,
}: {
  accountId: number
  onClose: () => void
}) {
  const { profilesById, ensureProfiles, conversations } = useSocial()
  const { matched } = useRelation(accountId)
  const reduceMotion = useReducedMotion()
  const closeButton = useRef<HTMLButtonElement>(null)
  const [photoIndex, setPhotoIndex] = useState(0)
  const profile = profilesById[accountId]
  const conversation = conversations.find((item) => item.other_account_id === accountId)

  useEffect(() => {
    ensureProfiles([accountId])
  }, [accountId, ensureProfiles])

  useEffect(() => {
    closeButton.current?.focus()
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose()
    }
    window.addEventListener('keydown', onKeyDown)
    return () => window.removeEventListener('keydown', onKeyDown)
  }, [onClose])

  const photos = profile?.photos ?? []
  const photo = photos[Math.min(photoIndex, photos.length - 1)]

  return createPortal(
    <div className="dialog-backdrop" onClick={onClose}>
      <motion.div
        className="dialog profile-dialog"
        role="dialog"
        aria-modal="true"
        aria-label={profile ? `Profil de ${profile.first_name}` : 'Profil'}
        initial={reduceMotion ? false : { opacity: 0, y: 24, scale: 0.97 }}
        animate={{ opacity: 1, y: 0, scale: 1 }}
        transition={{ duration: 0.28, ease: [0.22, 1, 0.36, 1] }}
        onClick={(event) => event.stopPropagation()}
      >
        <button ref={closeButton} className="dialog__close" type="button" onClick={onClose} aria-label="Fermer">
          ×
        </button>

        {profile === undefined ? (
          <p className="loading-text profile-dialog__status">Chargement du profil…</p>
        ) : profile === null ? (
          <p className="feedback feedback--error profile-dialog__status">Ce profil n’est plus disponible.</p>
        ) : (
          <>
            <div className="profile-dialog__gallery">
              <ProfileImage className="profile-dialog__photo" path={photo?.url} name={profile.first_name} />
              {photos.length > 1 && (
                <div className="profile-dialog__dots">
                  {photos.map((item, index) => (
                    <button
                      key={item.id}
                      type="button"
                      aria-pressed={index === photoIndex}
                      onClick={() => setPhotoIndex(index)}
                      aria-label={`Photo ${index + 1}`}
                    />
                  ))}
                </div>
              )}
            </div>
            <div className="profile-dialog__body">
              <div className="section__head">
                <h2>{displayName(profile)}</h2>
                <RelationChip accountId={accountId} />
              </div>
              <p className="form__hint">@{profile.username}</p>
              <ProfileTags profile={profile} />
              <p className="profile-dialog__bio">{profile.biography}</p>
              <LikeButton accountId={accountId} />
              {matched && conversation && (
                <Link className="button button--ghost" to={`/messages?c=${conversation.id}`}>
                  Écrire à {profile.first_name}
                </Link>
              )}
            </div>
          </>
        )}
      </motion.div>
    </div>,
    document.body,
  )
}
