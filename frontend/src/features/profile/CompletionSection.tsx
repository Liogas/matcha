import type { Profile } from '../../api/auth'
import { completionChecks } from '../../shared/profile/profileFormat'

export function CompletionSection({ profile }: { profile: Profile }) {
  const checks = completionChecks(profile)
  const done = checks.filter((check) => check.complete).length

  return (
    <section className="section">
      <div className="section__head">
        <h2>{profile.completed ? 'Profil visible' : 'Profil à compléter'}</h2>
        <span className={profile.completed ? 'chip chip--success' : 'chip'}>{done}/{checks.length}</span>
      </div>
      <div className="progress" aria-hidden="true">
        <span className="progress__bar" style={{ width: `${(done / checks.length) * 100}%` }} />
      </div>
      {!profile.completed && (
        <ul className="checklist">
          {checks.map((check) => (
            <li key={check.label} className="checklist__item" data-done={check.complete ? 'true' : undefined}>
              <span aria-hidden="true">{check.complete ? '✓' : '○'}</span>
              {check.label}
              <span className="visually-hidden">{check.complete ? ' : fait' : ' : à faire'}</span>
            </li>
          ))}
        </ul>
      )}
    </section>
  )
}
