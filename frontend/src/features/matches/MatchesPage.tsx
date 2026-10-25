import { useCallback, useEffect, useState } from 'react'
import type { ReactNode } from 'react'
import { Link } from 'react-router-dom'
import { useSocial } from '../../context/useSocial'
import { PagePanel } from '../../layout/PagePanel'
import { ProfileDialog } from '../../shared/profile/ProfileDialog'
import { ProfileRow } from '../../shared/profile/ProfileWidgets'

function RelationSection({ title, hint, ids, empty, renderAction, onOpen, highlight = false }: {
  title: string
  hint: string
  ids: number[]
  empty: string
  renderAction?: (accountId: number) => ReactNode
  onOpen: (accountId: number) => void
  highlight?: boolean
}) {
  return (
    <section className="section">
      <div className="section__head">
        <h2 className="section__title">{title}</h2>
        <span className={highlight && ids.length > 0 ? 'count count--hot' : 'count'}>{ids.length}</span>
      </div>
      <p className="section__hint">{hint}</p>
      {ids.length === 0 ? (
        <p className="empty-state">{empty}</p>
      ) : (
        <ul className="profile-list">
          {ids.map((id) => (
            <ProfileRow key={id} accountId={id} onOpen={onOpen} action={renderAction?.(id)} />
          ))}
        </ul>
      )}
    </section>
  )
}

export function MatchesPage() {
  const { matchIds, incomingIds, likedIds, conversations, ensureProfiles, loadError } = useSocial()
  const [openId, setOpenId] = useState<number | null>(null)
  const closeDialog = useCallback(() => setOpenId(null), [])

  const matches = [...matchIds]
  const waitingForMe = [...incomingIds].filter((id) => !matchIds.has(id) && !likedIds.has(id))
  const waitingForThem = [...likedIds].filter((id) => !matchIds.has(id))

  useEffect(() => {
    ensureProfiles([...matchIds, ...incomingIds, ...likedIds])
  }, [matchIds, incomingIds, likedIds, ensureProfiles])

  return (
    <PagePanel
      eyebrow="Matchs"
      title="Les coups de cœur réciproques"
      description="Quand le like est mutuel, une conversation s’ouvre."
    >
      {loadError && <p className="feedback feedback--error" role="alert">{loadError}</p>}
      <RelationSection
        title="Vos matchs"
        hint="Le like est réciproque : la conversation est ouverte."
        ids={matches}
        empty="Pas encore de match. Likez des profils dans Découvrir !"
        onOpen={setOpenId}
        renderAction={(id) => {
          const conversation = conversations.find((item) => item.other_account_id === id)
          return conversation && (
            <Link className="button button--primary button--small" to={`/messages?c=${conversation.id}`}>
              Écrire{conversation.unread_count > 0 ? ` (${conversation.unread_count})` : ''}
            </Link>
          )
        }}
      />
      <RelationSection
        title="Ils vous ont liké"
        hint="Likez en retour pour créer un match."
        ids={waitingForMe}
        empty="Aucun like en attente de réponse."
        onOpen={setOpenId}
        highlight
      />
      <RelationSection
        title="Vos likes envoyés"
        hint="En attente d’un like en retour."
        ids={waitingForThem}
        empty="Vous n’avez liké personne pour l’instant."
        onOpen={setOpenId}
      />
      {openId !== null && <ProfileDialog accountId={openId} onClose={closeDialog} />}
    </PagePanel>
  )
}
