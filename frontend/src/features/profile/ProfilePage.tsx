import { useSocial } from '../../context/useSocial'
import { PagePanel } from '../../layout/PagePanel'
import { AccountSection } from './AccountSection'
import { CompletionSection } from './CompletionSection'
import { PhotoManager } from './PhotoManager'
import { ProfileForm } from './ProfileForm'
import { ProfilePreview } from './ProfilePreview'
import './profile-page.css'

export function ProfilePage() {
  const { myProfile, isLoaded, loadError } = useSocial()

  return (
    <PagePanel
      eyebrow="Votre profil"
      title="Ce que les autres voient de vous"
      description="Un profil complet devient visible et peut liker les autres membres."
    >
      {loadError && <p className="feedback feedback--error" role="alert">{loadError}</p>}
      {!myProfile ? (
        <p className="loading-text">{isLoaded ? 'Profil introuvable.' : 'Chargement du profil…'}</p>
      ) : (
        <>
          <CompletionSection profile={myProfile} />
          <ProfileForm key={myProfile.account_id} profile={myProfile} />
          <PhotoManager profile={myProfile} />
          <ProfilePreview profile={myProfile} />
          <AccountSection />
        </>
      )}
    </PagePanel>
  )
}
