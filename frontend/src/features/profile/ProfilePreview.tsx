import type { Profile } from '../../api/auth'
import { ProfileImage } from '../../shared/profile/ProfileImage'
import { ProfileTags } from '../../shared/profile/ProfileWidgets'
import { displayName } from '../../shared/profile/profileFormat'

export function ProfilePreview({ profile }: { profile: Profile }) {
  const primary = profile.photos.find((photo) => photo.is_primary)
  return (
    <section className="section">
      <h2>Aperçu</h2>
      <p className="section__hint">Ce que voient les autres membres.</p>
      <div className="profile-preview">
        <ProfileImage className="profile-preview__photo" path={primary?.url} name={profile.first_name} />
        <div className="profile-preview__body">
          <h3>{displayName(profile)}</h3>
          <p className="form__hint">@{profile.username}</p>
          <ProfileTags profile={profile} />
          <p className="profile-preview__bio">{profile.biography || 'Pas encore de description.'}</p>
        </div>
      </div>
    </section>
  )
}
