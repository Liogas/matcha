import { useCallback, useEffect, useRef, useState } from 'react'
import { Link } from 'react-router-dom'
import { describeApiError, getProfiles, type Profile } from '../../api/auth'
import { useAuth } from '../../context/useAuth'
import { useSocial } from '../../context/useSocial'
import { PagePanel } from '../../layout/PagePanel'
import { ProfileDialog } from '../../shared/profile/ProfileDialog'
import { ProfileTile } from '../../shared/profile/ProfileWidgets'
import { Feedback } from '../../shared/ui/Feedback'
import './discover.css'

const PAGE_SIZE = 24

type DiscoverFilter = 'all' | 'new' | 'likes-me'

const FILTERS: { value: DiscoverFilter; label: string; empty: string }[] = [
  { value: 'all', label: 'Tous', empty: 'Aucun profil complet à découvrir pour le moment.' },
  { value: 'new', label: 'Pas encore likés', empty: 'Vous avez déjà liké tous les profils chargés.' },
  { value: 'likes-me', label: 'Ils vous aiment', empty: 'Personne parmi les profils chargés ne vous a encore liké.' },
]

export function DiscoverPage() {
  const { accessToken } = useAuth()
  const { myProfile, likedIds, incomingIds, loadError } = useSocial()
  const [profiles, setProfiles] = useState<Profile[]>([])
  const [hasMore, setHasMore] = useState(true)
  const [isLoading, setIsLoading] = useState(true)
  const [error, setError] = useState('')
  const [filter, setFilter] = useState<DiscoverFilter>('all')
  const [openId, setOpenId] = useState<number | null>(null)
  const closeDialog = useCallback(() => setOpenId(null), [])
  const tokenRef = useRef(accessToken)

  useEffect(() => {
    tokenRef.current = accessToken
  }, [accessToken])

  // Le jeton renouvelé ne doit pas relancer le chargement : il est lu via une ref
  const fetchPage = useCallback(async (offset: number) => {
    const token = tokenRef.current
    if (!token) return
    try {
      const page = await getProfiles(token, { limit: PAGE_SIZE, offset })
      setProfiles((current) => offset === 0 ? page : [...current, ...page])
      setHasMore(page.length === PAGE_SIZE)
      setError('')
    } catch (loadPageError) {
      setError(describeApiError(loadPageError))
    } finally {
      setIsLoading(false)
    }
  }, [])

  useEffect(() => {
    void fetchPage(0)
  }, [fetchPage])

  function loadMore() {
    setIsLoading(true)
    void fetchPage(profiles.length)
  }

  const visible = profiles.filter((profile) =>
    filter === 'all'
      ? true
      : filter === 'new'
        ? !likedIds.has(profile.account_id)
        : incomingIds.has(profile.account_id))
  const current = FILTERS.find((item) => item.value === filter) ?? FILTERS[0]

  return (
    <PagePanel
      eyebrow="Découvrir"
      title="Qui se promène par ici ?"
      description="Des profils qui pourraient vous plaire. Ouvrez une fiche pour en savoir plus."
    >
      {loadError && <p className="feedback feedback--error" role="alert">{loadError}</p>}
      {myProfile && !myProfile.completed && (
        <p className="notice">
          Votre profil n’est pas encore visible et vous ne pouvez pas liker.{' '}
          <Link to="/profil">Terminer mon profil</Link>
        </p>
      )}

      <div className="segmented" role="tablist" aria-label="Filtrer les profils">
        {FILTERS.map((item) => (
          <button
            key={item.value}
            type="button"
            role="tab"
            className="segmented__option"
            aria-selected={filter === item.value}
            onClick={() => setFilter(item.value)}
          >
            {item.label}
          </button>
        ))}
      </div>

      <Feedback message={error} />

      {visible.length > 0 ? (
        <div className="profile-grid">
          {visible.map((profile) => (
            <ProfileTile key={profile.account_id} profile={profile} onOpen={setOpenId} />
          ))}
        </div>
      ) : !isLoading && (
        <p className="empty-state">{current.empty}</p>
      )}

      <div className="discover__more">
        {isLoading ? (
          <span className="loading-text">Chargement…</span>
        ) : hasMore ? (
          <button className="button button--ghost" type="button" onClick={loadMore}>
            Voir plus de profils
          </button>
        ) : profiles.length > 0 && (
          <span className="loading-text">{profiles.length} profil{profiles.length > 1 ? 's' : ''} au total</span>
        )}
      </div>

      {openId !== null && <ProfileDialog accountId={openId} onClose={closeDialog} />}
    </PagePanel>
  )
}
