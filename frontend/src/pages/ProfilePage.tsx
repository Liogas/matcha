import { motion } from 'motion/react'
import { useCallback, useEffect, useRef, useState } from 'react'
import type { ChangeEvent, FormEvent } from 'react'
import {
  deleteProfilePhoto,
  describeApiError,
  getProfile,
  getProfiles,
  getImageUrl,
  getIncomingLikeProfileIds,
  getLikedProfileIds,
  getMatchProfileIds,
  getProfilePhoto,
  getConversationMessages,
  getConversations,
  likeProfile,
  markConversationRead,
  openMessagingSocket,
  replaceProfilePhoto,
  sendConversationMessage,
  setPrimaryProfilePhoto,
  updateProfile,
  uploadProfilePhoto,
  type Profile,
  type ProfilePhoto,
  type MessagingConversation,
  type MessagingMessage,
  type MessagingSocketMessage,
  unlikeProfile,
} from '../api/auth'
import { useAuth } from '../context/useAuth'

function getAge(dateOfBirth: string) {
  if (!dateOfBirth) return null
  const birthDate = new Date(`${dateOfBirth}T00:00:00`)
  if (Number.isNaN(birthDate.getTime())) return null
  const today = new Date()
  let age = today.getFullYear() - birthDate.getFullYear()
  const birthdayPassed =
    today.getMonth() > birthDate.getMonth() ||
    (today.getMonth() === birthDate.getMonth() &&
      today.getDate() >= birthDate.getDate())
  if (!birthdayPassed) age -= 1
  return age >= 0 ? age : null
}

const emptyProfile: Profile = {
  account_id: 0,
  username: '',
  first_name: '',
  last_name: '',
  date_of_birth: '',
  gender: '',
  sexual_orientation: 'bisexual',
  biography: '',
  city: '',
  completed: false,
  photos: [],
}

function PhotoSlots({
  profile,
  onChanged,
}: {
  profile: Profile
  onChanged: () => Promise<void>
}) {
  const { accessToken } = useAuth()
  const [photoUrls, setPhotoUrls] = useState<Record<number, string>>({})
  const [busyPosition, setBusyPosition] = useState<number | null>(null)
  const [error, setError] = useState('')

  useEffect(() => {
    let active = true
    const urls: string[] = []
    async function loadPhotos() {
      if (!accessToken) return
      const loaded = await Promise.all(
        profile.photos.map(async (photo) => {
          const url = await getProfilePhoto(accessToken, photo)
          urls.push(url)
          return [photo.id, url] as const
        }),
      )
      if (active) setPhotoUrls(Object.fromEntries(loaded))
      else urls.forEach((url) => URL.revokeObjectURL(url))
    }
    void loadPhotos().catch(() => {
      if (active) setError('Une photo n’a pas pu être chargée.')
    })
    return () => {
      active = false
      urls.forEach((url) => URL.revokeObjectURL(url))
    }
  }, [accessToken, profile.photos])

  async function handleFile(
    event: ChangeEvent<HTMLInputElement>,
    position: number,
    existing?: ProfilePhoto,
  ) {
    const file = event.target.files?.[0]
    event.target.value = ''
    if (!file || !accessToken) return
    setError('')
    setBusyPosition(position)
    try {
      if (existing)
        await replaceProfilePhoto(accessToken, existing.id, file)
      else
        await uploadProfilePhoto(accessToken, position, file)
      await onChanged()
    } catch (uploadError) {
      setError(describeApiError(uploadError))
    } finally {
      setBusyPosition(null)
    }
  }

  async function handleDelete(photo: ProfilePhoto) {
    if (!accessToken) return
    setError('')
    setBusyPosition(photo.position)
    try {
      await deleteProfilePhoto(accessToken, photo.id)
      await onChanged()
    } catch (deleteError) {
      setError(describeApiError(deleteError))
    } finally {
      setBusyPosition(null)
    }
  }

  async function handlePrimary(photo: ProfilePhoto) {
    if (!accessToken) return
    setError('')
    setBusyPosition(photo.position)
    try {
      await setPrimaryProfilePhoto(accessToken, photo.id)
      await onChanged()
    } catch (primaryError) {
      setError(describeApiError(primaryError))
    } finally {
      setBusyPosition(null)
    }
  }

  return (
    <section className="profile-photos">
      <div className="section-heading">
        <div>
          <p className="eyebrow">VOTRE GALERIE</p>
          <h2>Ajoutez vos photos.</h2>
        </div>
        <span>1 à 5 images</span>
      </div>
      <p className="section-description">
        La première photo est utilisée comme photo principale. Les fichiers sont
        contrôlés et convertis en WebP par le serveur.
      </p>
      <div className="photo-grid">
        {Array.from({ length: 5 }, (_, index) => {
          const position = index + 1
          const photo = profile.photos.find((item) => item.position === position)
          const inputId = `profile-photo-${position}`
          return (
            <div className={`photo-slot ${photo?.is_primary ? 'primary-photo' : ''}`} key={position}>
              {photo && photoUrls[photo.id] ? (
                <img src={photoUrls[photo.id]} alt={`Photo ${position}`} />
              ) : (
                <span className="photo-placeholder">+</span>
              )}
              <span className="photo-position">{position}</span>
              <label className="photo-upload" htmlFor={inputId}>
                {busyPosition === position ? '…' : photo ? 'Remplacer' : 'Ajouter'}
              </label>
              <input
                id={inputId}
                type="file"
                accept="image/jpeg,image/png,image/webp"
                onChange={(event) => void handleFile(event, position, photo)}
                disabled={busyPosition !== null}
              />
              {photo && (
                <div className="photo-actions">
                  {!photo.is_primary && (
                    <button type="button" onClick={() => void handlePrimary(photo)}>
                      Principale
                    </button>
                  )}
                  <button type="button" onClick={() => void handleDelete(photo)}>
                    Supprimer
                  </button>
                </div>
              )}
            </div>
          )
        })}
      </div>
      {error && <p className="form-message error-message">{error}</p>}
    </section>
  )
}

function ProfileCard({ profile }: { profile: Profile }) {
  const { accessToken } = useAuth()
  const [primaryUrl, setPrimaryUrl] = useState('')
  const primaryPhoto = profile.photos.find((photo) => photo.is_primary)
  const age = getAge(profile.date_of_birth)

  useEffect(() => {
    let active = true
    let url = ''
    if (accessToken && primaryPhoto) {
      void getProfilePhoto(accessToken, primaryPhoto)
        .then((loadedUrl) => {
          url = loadedUrl
          if (active) setPrimaryUrl(loadedUrl)
          else URL.revokeObjectURL(loadedUrl)
        })
        .catch(() => {
          if (active) setPrimaryUrl('')
        })
    }
    return () => {
      active = false
      if (url) URL.revokeObjectURL(url)
    }
  }, [accessToken, primaryPhoto])

  return (
    <section className="profile-card-section">
      <div className="section-heading">
        <div>
          <p className="eyebrow">VOTRE CARTE</p>
          <h2>Voici comment on vous voit.</h2>
        </div>
        <span className="profile-ready-badge">Profil complet</span>
      </div>
      <div className="profile-card">
        <div className="profile-card-photo">
          {primaryUrl ? (
            <img src={primaryUrl} alt={`Portrait de ${profile.first_name}`} />
          ) : (
            <span aria-hidden="true">{profile.first_name.charAt(0)}</span>
          )}
        </div>
        <div className="profile-card-body">
          <div className="profile-card-title">
            <div>
              <p className="eyebrow">MATCHA MEMBER</p>
              <h3>{profile.first_name}{age !== null ? `, ${age}` : ''}</h3>
              <span className="profile-card-city">{profile.city}</span>
            </div>
            <span className="profile-card-status" aria-label="Profil visible">●</span>
          </div>
          <p className="profile-card-bio">
            {profile.biography}
          </p>
          <div className="profile-card-tags">
            <span>{profile.sexual_orientation}</span>
            <span>{profile.gender}</span>
            <span>{profile.photos.length} photo{profile.photos.length > 1 ? 's' : ''}</span>
          </div>
        </div>
      </div>
    </section>
  )
}

function DiscoveryCard({ profile, initiallyLiked, incomingLike, matched, onLike }: {
  profile: Profile
  initiallyLiked: boolean
  incomingLike: boolean
  matched: boolean
  onLike: (profile: Profile, liked: boolean) => Promise<void>
}) {
  const { accessToken } = useAuth()
  const [photoUrl, setPhotoUrl] = useState('')
  const [liked, setLiked] = useState(initiallyLiked)
  const [isUpdating, setIsUpdating] = useState(false)
  const age = getAge(profile.date_of_birth)
  const cardStatus = matched ? 'match' : initiallyLiked ? 'liked' : incomingLike ? 'incoming' : ''

  useEffect(() => {
    let active = true
    let url = ''
    if (accessToken && profile.photo_url) {
      void getImageUrl(accessToken, profile.photo_url)
        .then((loadedUrl) => {
          url = loadedUrl
          if (active) setPhotoUrl(loadedUrl)
          else URL.revokeObjectURL(loadedUrl)
        })
        .catch(() => undefined)
    }
    return () => {
      active = false
      if (url) URL.revokeObjectURL(url)
    }
  }, [accessToken, profile.photo_url])

  return (
    <article className={`discovery-card ${cardStatus ? `discovery-card-${cardStatus}` : ''}`}>
      <div className="discovery-photo">
        {photoUrl ? <img src={photoUrl} alt={`Portrait de ${profile.first_name}`} /> : (
          <span aria-hidden="true">{profile.first_name.charAt(0)}</span>
        )}
      </div>
      <div className="discovery-card-body">
        <div className="discovery-card-heading">
          <h3>{profile.first_name}{age !== null ? `, ${age}` : ''}</h3>
          <span>{profile.city}</span>
        </div>
        <p>{profile.biography}</p>
        <div className="profile-card-tags">
          <span>{profile.gender}</span>
          <span>{profile.sexual_orientation}</span>
        </div>
        <p className="discovery-status">
          {matched ? 'Match en cours' : initiallyLiked ? 'J’aime envoyé' : incomingLike ? 'Vous a aimé' : 'Nouveau profil'}
        </p>
        <button
          className="button button-secondary"
          type="button"
          disabled={isUpdating}
          onClick={() => {
            setIsUpdating(true)
            void onLike(profile, !liked)
              .then(() => setLiked((current) => !current))
              .finally(() => setIsUpdating(false))
          }}
        >
          {isUpdating ? '…' : liked ? 'Retirer le like' : 'J’aime'}
        </button>
      </div>
    </article>
  )
}

function DiscoveryList({ profiles, likedIds, incomingIds, matchIds, onLike }: {
  profiles: Profile[]
  likedIds: Set<number>
  incomingIds: Set<number>
  matchIds: Set<number>
  onLike: (profile: Profile, liked: boolean) => Promise<void>
}) {
  return (
    <section className="discovery-section">
      <div className="section-heading">
        <div>
          <p className="eyebrow">DÉCOUVERTE</p>
          <h2>Les profils à découvrir.</h2>
        </div>
        <span>{profiles.length} profil{profiles.length > 1 ? 's' : ''}</span>
      </div>
      <div className="discovery-grid">
        {profiles.map((profile) => (
          <DiscoveryCard
            key={profile.account_id}
            profile={profile}
            initiallyLiked={likedIds.has(profile.account_id)}
            incomingLike={incomingIds.has(profile.account_id)}
            matched={matchIds.has(profile.account_id)}
            onLike={onLike}
          />
        ))}
      </div>
    </section>
  )
}

function MessagingPanel({
  profiles,
  matchIds,
  accessToken,
  userId,
}: {
  profiles: Profile[]
  matchIds: Set<number>
  accessToken: string
  userId: number
}) {
  const [conversations, setConversations] = useState<MessagingConversation[]>([])
  const [selectedId, setSelectedId] = useState<number | null>(null)
  const [messages, setMessages] = useState<MessagingMessage[]>([])
  const [draft, setDraft] = useState('')
  const [error, setError] = useState('')
  const [isLoading, setIsLoading] = useState(true)
  const socketRef = useRef<WebSocket | null>(null)
  const selectedIdRef = useRef<number | null>(null)

  const matchedProfiles = profiles.filter((candidate) =>
    matchIds.has(candidate.account_id))

  useEffect(() => {
    selectedIdRef.current = selectedId
  }, [selectedId])

  useEffect(() => {
    let active = true
    void getConversations(accessToken)
      .then((loaded) => {
        if (!active) return
        setConversations(loaded)
        if (loaded.length > 0)
          setSelectedId((current) => current ?? loaded[0].id)
      })
      .catch((loadError) => {
        if (active) setError(describeApiError(loadError))
      })
      .finally(() => {
        if (active) setIsLoading(false)
      })
    return () => {
      active = false
    }
  }, [accessToken])

  useEffect(() => {
    let cancelled = false
    let socket: WebSocket | null = null
    const connectionTimer = window.setTimeout(() => {
      if (cancelled) return
      socket = openMessagingSocket(
        accessToken,
        (message: MessagingSocketMessage) => {
          const incoming: MessagingMessage = { ...message, read: false }
          setMessages((current) => current.some((item) => item.id === incoming.id)
            ? current
            : [...current, incoming])
          setConversations((current) => current.map((conversation) =>
            conversation.id === incoming.conversation_id &&
            incoming.conversation_id !== selectedIdRef.current &&
            incoming.sender_account_id !== userId
              ? { ...conversation, unread_count: conversation.unread_count + 1 }
              : conversation))
        },
        () => setError('La connexion temps réel est indisponible.'),
      )
      socketRef.current = socket
    }, 0)
    return () => {
      cancelled = true
      window.clearTimeout(connectionTimer)
      socket?.close()
      if (socketRef.current === socket)
        socketRef.current = null
    }
  }, [accessToken, userId])

  useEffect(() => {
    if (selectedId === null) {
      return
    }
    let active = true
    void getConversationMessages(accessToken, selectedId)
      .then((loaded) => {
        if (active) setMessages(loaded)
      })
      .then(() => markConversationRead(accessToken, selectedId))
      .catch((loadError) => {
        if (active) setError(describeApiError(loadError))
      })
    return () => {
      active = false
    }
  }, [accessToken, selectedId])

  async function handleSend(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    const body = draft.trim()
    if (!body || selectedId === null) return
    setError('')
    try {
      const socket = socketRef.current
      if (socket?.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ conversation_id: selectedId, body }))
      } else {
        const message = await sendConversationMessage(accessToken, selectedId, body)
        setMessages((current) => [...current, message])
      }
      setDraft('')
    } catch (sendError) {
      setError(describeApiError(sendError))
    }
  }

  return (
    <section className="messaging-section">
      <div className="section-heading">
        <div>
          <p className="eyebrow">MESSAGERIE</p>
          <h2>Échangez avec vos matchs.</h2>
        </div>
        <span>{matchedProfiles.length} match{matchedProfiles.length > 1 ? 's' : ''}</span>
      </div>
      {isLoading ? (
        <p className="form-message">Chargement des conversations…</p>
      ) : matchedProfiles.length === 0 ? (
        <p className="form-message">Vos matchs apparaîtront ici.</p>
      ) : (
        <div className="messaging-layout">
          <div className="conversation-list">
            {matchedProfiles.map((profile) => {
              const conversation = conversations.find((item) =>
                item.other_account_id === profile.account_id)
              return (
                <button
                  className={conversation?.id === selectedId ? 'conversation-item active' : 'conversation-item'}
                  type="button"
                  key={profile.account_id}
                  disabled={!conversation}
                  onClick={() => conversation && setSelectedId(conversation.id)}
                >
                  <strong>{profile.first_name}</strong>
                  {!conversation ? <small>Conversation indisponible</small> : conversation.unread_count > 0
                    ? <small>{conversation.unread_count} nouveau{conversation.unread_count > 1 ? 'x' : ''}</small>
                    : <small>Ouvrir la conversation</small>}
                </button>
              )
            })}
          </div>
          <div className="conversation-panel">
            {selectedId === null ? (
              <p className="form-message">Sélectionnez un match.</p>
            ) : (
              <>
                <div className="message-list" aria-live="polite">
                  {messages.length === 0
                    ? <p className="form-message">Aucun message. Lancez la conversation.</p>
                    : messages.map((message) => (
                      <p className={message.sender_account_id === userId ? 'message-bubble own' : 'message-bubble'} key={message.id}>
                        {message.body}
                      </p>
                    ))}
                </div>
                <form className="message-form" onSubmit={(event) => void handleSend(event)}>
                  <input
                    value={draft}
                    onChange={(event) => setDraft(event.target.value)}
                    maxLength={2000}
                    placeholder="Écrivez un message…"
                    aria-label="Message"
                  />
                  <button className="button button-primary" type="submit" disabled={!draft.trim()}>
                    Envoyer
                  </button>
                </form>
              </>
            )}
          </div>
        </div>
      )}
      {error && <p className="form-message error-message">{error}</p>}
    </section>
  )
}

function CompletionGate({ profile }: { profile: Profile }) {
  const checks = [
    { label: 'Date de naissance majeure', complete: (getAge(profile.date_of_birth) ?? 0) >= 18 },
    { label: 'Ville valide', complete: Boolean(profile.city && /^[A-Za-zÀ-ÖØ-öø-ÿ][A-Za-zÀ-ÖØ-öø-ÿ .'-]*$/.test(profile.city)) },
    { label: 'Genre renseigné', complete: Boolean(profile.gender) },
    { label: 'Description renseignée', complete: Boolean(profile.biography.trim()) },
    { label: 'Préférences de rencontre', complete: Boolean(profile.sexual_orientation) },
    { label: 'Photo principale', complete: profile.photos.some((photo) => photo.is_primary) },
  ]

  return (
    <section className="completion-gate" aria-labelledby="completion-gate-title">
      <div className="lock-icon" aria-hidden="true">⌁</div>
      <div>
        <p className="eyebrow">CARTE VERROUILLÉE</p>
        <h2 id="completion-gate-title">Votre carte se révèle bientôt.</h2>
        <p>
          Elle sera accessible dès que les éléments essentiels de votre profil
          seront renseignés. Prenez le temps de vous présenter à votre rythme.
        </p>
      </div>
      <ul className="completion-checklist">
        {checks.map((check) => (
          <li className={check.complete ? 'check-complete' : ''} key={check.label}>
            <span aria-hidden="true">{check.complete ? '✓' : '○'}</span>
            {check.label}
          </li>
        ))}
      </ul>
    </section>
  )
}

export function ProfilePage() {
  const { accessToken, user } = useAuth()
  const [profile, setProfile] = useState<Profile>(emptyProfile)
  const [profiles, setProfiles] = useState<Profile[]>([])
  const [likedIds, setLikedIds] = useState<Set<number>>(new Set())
  const [incomingIds, setIncomingIds] = useState<Set<number>>(new Set())
  const [matchIds, setMatchIds] = useState<Set<number>>(new Set())
  const [activeView, setActiveView] = useState<'all' | 'received' | 'matches'>('all')
  const [isLoading, setIsLoading] = useState(true)
  const [isSaving, setIsSaving] = useState(false)
  const [isEditing, setIsEditing] = useState(false)
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')

  const loadProfile = useCallback(async () => {
    if (!accessToken) return
    const [loaded, availableProfiles, liked, incoming, matches] = await Promise.all([
      getProfile(accessToken),
      getProfiles(accessToken),
      getLikedProfileIds(accessToken),
      getIncomingLikeProfileIds(accessToken),
      getMatchProfileIds(accessToken),
    ])
    setProfile(loaded)
    setProfiles(availableProfiles)
    setLikedIds(new Set(liked))
    setIncomingIds(new Set(incoming))
    setMatchIds(new Set(matches))
  }, [accessToken])

  async function handleLike(profileToLike: Profile, liked: boolean) {
    if (!accessToken) return
    if (liked) {
      const result = await likeProfile(accessToken, profileToLike.account_id)
      setLikedIds((current) => new Set(current).add(profileToLike.account_id))
      if (result.matched) {
        setMatchIds((current) => new Set(current).add(profileToLike.account_id))
        setSuccess(`Nouveau match avec ${profileToLike.first_name} !`)
      }
      return
    }
    await unlikeProfile(accessToken, profileToLike.account_id)
    setLikedIds((current) => {
      const updated = new Set(current)
      updated.delete(profileToLike.account_id)
      return updated
    })
    setMatchIds((current) => {
      const updated = new Set(current)
      updated.delete(profileToLike.account_id)
      return updated
    })
  }

  useEffect(() => {
    // The request synchronizes this screen with the authenticated API session.
    // eslint-disable-next-line react-hooks/set-state-in-effect
    void loadProfile()
      .catch((loadError) => setError(describeApiError(loadError)))
      .finally(() => setIsLoading(false))
  }, [loadProfile])

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    if (!accessToken) return
    setIsSaving(true)
    setError('')
    setSuccess('')
    try {
      const updated = await updateProfile(accessToken, profile)
      setProfile(updated)
      setSuccess(
        updated.completed
          ? 'Votre profil est prêt.'
          : 'Vos informations sont enregistrées. Ajoutez une photo principale pour terminer.',
      )
    } catch (saveError) {
      setError(describeApiError(saveError))
    } finally {
      setIsSaving(false)
    }
  }

  function updateField(field: keyof Profile, value: string) {
    setProfile((current) => ({ ...current, [field]: value }))
  }

  if (isLoading)
    return <p className="session-loading">On prépare votre profil…</p>
  if (error && !profile.account_id)
    return <p className="form-message error-message">{error}</p>

  return (
    <motion.section
      className="profile-page"
      initial={{ opacity: 0, y: 16 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ duration: 0.3 }}
    >
      <div className="profile-intro">
        <p className="eyebrow">{profile.completed ? 'VOTRE ESPACE' : 'PREMIÈRE ÉTAPE'}</p>
        <h1>{profile.completed ? `Bonjour ${profile.first_name}.` : 'Construisons votre profil.'}</h1>
        <p>{profile.completed
          ? 'Votre carte personnelle est prête. Vous pourrez enrichir ces informations plus tard.'
          : 'Complétez ces quelques informations pour débloquer votre carte personnelle.'}</p>
        {user?.email && <span className="profile-email">{user.email}</span>}
      </div>
      {!profile.completed && <CompletionGate profile={profile} />}
      {profile.completed && !isEditing && (
        <>
          <ProfileCard profile={profile} />
          <button className="button button-secondary edit-profile-button" type="button" onClick={() => setIsEditing(true)}>
            Modifier mon profil
          </button>
        </>
      )}
      {(!profile.completed || isEditing) && (
        <form className="profile-form" onSubmit={handleSubmit}>
        <div className="profile-form-grid">
          <label>
            Date de naissance
            <input
              type="date"
              value={profile.date_of_birth}
              onChange={(event) => updateField('date_of_birth', event.target.value)}
              required
            />
          </label>
          <label>
            Ville
            <input
              value={profile.city}
              onChange={(event) => updateField('city', event.target.value)}
              maxLength={120}
              pattern="[A-Za-zÀ-ÖØ-öø-ÿ][A-Za-zÀ-ÖØ-öø-ÿ .'-]*"
              placeholder="Paris"
              required
            />
          </label>
          <label>
            Genre
            <select
              value={profile.gender}
              onChange={(event) => updateField('gender', event.target.value)}
              required
            >
              <option value="">Je préfère ne pas préciser</option>
              <option value="woman">Femme</option>
              <option value="man">Homme</option>
              <option value="non_binary">Non-binaire</option>
              <option value="other">Autre</option>
            </select>
          </label>
          <label>
            Orientation
            <select
              value={profile.sexual_orientation}
              onChange={(event) => updateField('sexual_orientation', event.target.value)}
              required
            >
              <option value="bisexual">Bisexuel·le</option>
              <option value="heterosexual">Hétérosexuel·le</option>
              <option value="homosexual">Homosexuel·le</option>
              <option value="pansexual">Pansexuel·le</option>
              <option value="asexual">Asexuel·le</option>
            </select>
          </label>
        </div>
        <label>
          Quelques mots sur vous
          <textarea
            value={profile.biography}
            onChange={(event) => updateField('biography', event.target.value)}
            maxLength={1000}
            rows={4}
            placeholder="Ce qui vous anime, vous fait rire ou vous donne envie de discuter…"
            required
          />
        </label>
        {error && <p className="form-message error-message">{error}</p>}
        {success && <p className="form-message success-message">{success}</p>}
        <button className="button button-primary" disabled={isSaving}>
          {isSaving ? 'Enregistrement…' : 'Enregistrer mes informations'}
        </button>
        </form>
      )}
      {(!profile.completed || isEditing) && <PhotoSlots profile={profile} onChanged={loadProfile} />}
      {profile.completed && isEditing && (
        <button className="button button-secondary" type="button" onClick={() => setIsEditing(false)}>
          Annuler les modifications
        </button>
      )}
      <nav className="discovery-tabs" aria-label="Filtres de découverte">
        {([
          ['all', 'Tous les profils'],
          ['received', 'Ils vous aiment'],
          ['matches', 'Matchs'],
        ] as const).map(([view, label]) => (
          <button
            className={activeView === view ? 'discovery-tab active' : 'discovery-tab'}
            key={view}
            type="button"
            onClick={() => setActiveView(view)}
          >
            {label}
          </button>
        ))}
      </nav>
      <DiscoveryList
        profiles={profiles.filter((candidate) =>
          activeView === 'all'
            ? true
            : activeView === 'received'
              ? incomingIds.has(candidate.account_id)
              : matchIds.has(candidate.account_id))}
        likedIds={likedIds}
        incomingIds={incomingIds}
        matchIds={matchIds}
        onLike={handleLike}
      />
      {profile.completed && activeView === 'matches' && accessToken && user && (
        <MessagingPanel
          profiles={profiles}
          matchIds={matchIds}
          accessToken={accessToken}
          userId={Number(user.id)}
        />
      )}
    </motion.section>
  )
}
