import { useState } from 'react'
import type { ChangeEvent } from 'react'
import {
  deleteProfilePhoto,
  describeApiError,
  replaceProfilePhoto,
  setPrimaryProfilePhoto,
  uploadProfilePhoto,
  type Profile,
  type ProfilePhoto,
} from '../../api/auth'
import { useAuth } from '../../context/useAuth'
import { useSocial } from '../../context/useSocial'
import { ProfileImage } from '../../shared/profile/ProfileImage'
import { Feedback } from '../../shared/ui/Feedback'

const PHOTO_SLOTS = 5

export function PhotoManager({ profile }: { profile: Profile }) {
  const { accessToken } = useAuth()
  const { reloadMyProfile } = useSocial()
  const [busyPosition, setBusyPosition] = useState<number | null>(null)
  const [error, setError] = useState('')

  async function run(position: number, action: (token: string) => Promise<unknown>) {
    if (!accessToken) return
    setError('')
    setBusyPosition(position)
    try {
      await action(accessToken)
      await reloadMyProfile()
    } catch (actionError) {
      setError(describeApiError(actionError))
    } finally {
      setBusyPosition(null)
    }
  }

  function handleFile(event: ChangeEvent<HTMLInputElement>, position: number, existing?: ProfilePhoto) {
    const file = event.target.files?.[0]
    event.target.value = ''
    if (!file) return
    void run(position, (token) => existing
      ? replaceProfilePhoto(token, existing.id, file)
      : uploadProfilePhoto(token, position, file))
  }

  return (
    <section className="section">
      <div className="section__head">
        <h2>Vos photos</h2>
        <span className="count">{profile.photos.length}/{PHOTO_SLOTS}</span>
      </div>
      <p className="section__hint">
        JPEG, PNG ou WebP. Le serveur les vérifie et les convertit en WebP.
        La photo principale apparaît dans les listes.
      </p>
      <div className="photo-grid">
        {Array.from({ length: PHOTO_SLOTS }, (_, index) => {
          const position = index + 1
          const photo = profile.photos.find((item) => item.position === position)
          const inputId = `profile-photo-${position}`
          const locked = busyPosition !== null
          return (
            <div className="photo-slot" data-primary={photo?.is_primary ? 'true' : undefined} key={position}>
              {photo
                ? <ProfileImage className="photo-slot__image" path={photo.url} name={profile.first_name} />
                : <span className="photo-slot__empty" aria-hidden="true">+</span>}
              {photo?.is_primary && <span className="photo-slot__badge">Principale</span>}
              <div className="photo-slot__actions">
                <label className="photo-slot__action" data-disabled={locked ? 'true' : undefined} htmlFor={inputId}>
                  {busyPosition === position ? '…' : photo ? 'Remplacer' : 'Ajouter'}
                </label>
                <input
                  id={inputId}
                  className="visually-hidden"
                  type="file"
                  accept="image/jpeg,image/png,image/webp"
                  onChange={(event) => handleFile(event, position, photo)}
                  disabled={locked}
                />
                {photo && !photo.is_primary && (
                  <button
                    className="photo-slot__action"
                    type="button"
                    disabled={locked}
                    onClick={() => void run(position, (token) => setPrimaryProfilePhoto(token, photo.id))}
                  >
                    Principale
                  </button>
                )}
                {photo && (
                  <button
                    className="photo-slot__action photo-slot__action--danger"
                    type="button"
                    disabled={locked}
                    onClick={() => void run(position, (token) => deleteProfilePhoto(token, photo.id))}
                    aria-label={`Supprimer la photo ${position}`}
                  >
                    Supprimer
                  </button>
                )}
              </div>
            </div>
          )
        })}
      </div>
      <Feedback message={error} />
    </section>
  )
}
