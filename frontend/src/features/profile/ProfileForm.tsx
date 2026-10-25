import { useState } from 'react'
import type { FormEvent } from 'react'
import { describeApiError, updateProfile, type Profile } from '../../api/auth'
import { useAuth } from '../../context/useAuth'
import { useSocial } from '../../context/useSocial'
import { GENDER_OPTIONS, ORIENTATION_OPTIONS } from '../../shared/profile/profileFormat'
import { Feedback } from '../../shared/ui/Feedback'

type ProfileFields = Pick<Profile, 'date_of_birth' | 'gender' | 'sexual_orientation' | 'biography' | 'city'>

function fieldsOf(profile: Profile): ProfileFields {
  return {
    date_of_birth: profile.date_of_birth,
    gender: profile.gender,
    sexual_orientation: profile.sexual_orientation || 'bisexual',
    biography: profile.biography,
    city: profile.city,
  }
}

export function ProfileForm({ profile }: { profile: Profile }) {
  const { accessToken } = useAuth()
  const { setMyProfile } = useSocial()
  const [fields, setFields] = useState<ProfileFields>(() => fieldsOf(profile))
  const [isSaving, setIsSaving] = useState(false)
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')
  const today = new Date().toISOString().slice(0, 10)

  function update(field: keyof ProfileFields, value: string) {
    setFields((current) => ({ ...current, [field]: value }))
    setSuccess('')
  }

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    if (!accessToken) return
    setIsSaving(true)
    setError('')
    setSuccess('')
    try {
      const updated = await updateProfile(accessToken, { ...fields, city: fields.city.trim() })
      setMyProfile(updated)
      setSuccess(updated.completed
        ? 'Profil enregistré. Il est visible par les autres membres.'
        : 'Informations enregistrées. Il manque encore quelques éléments pour être visible.')
    } catch (saveError) {
      setError(describeApiError(saveError))
    } finally {
      setIsSaving(false)
    }
  }

  return (
    <section className="section">
      <h2>Vos informations</h2>
      <form className="form" onSubmit={(event) => void handleSubmit(event)}>
        <div className="form__row">
          <label className="field">
            Date de naissance
            <input
              type="date"
              value={fields.date_of_birth}
              max={today}
              onChange={(event) => update('date_of_birth', event.target.value)}
              required
            />
          </label>
          <label className="field">
            Ville
            <input
              type="text"
              value={fields.city}
              onChange={(event) => update('city', event.target.value)}
              maxLength={120}
              pattern="[A-Za-zÀ-ÖØ-öø-ÿ][A-Za-zÀ-ÖØ-öø-ÿ .'\-]*"
              placeholder="Paris"
              required
            />
          </label>
          <label className="field">
            Genre
            <select value={fields.gender} onChange={(event) => update('gender', event.target.value)} required>
              <option value="" disabled>Choisir…</option>
              {GENDER_OPTIONS.map((option) => (
                <option key={option.value} value={option.value}>{option.label}</option>
              ))}
            </select>
          </label>
          <label className="field">
            Orientation
            <select
              value={fields.sexual_orientation}
              onChange={(event) => update('sexual_orientation', event.target.value)}
              required
            >
              {ORIENTATION_OPTIONS.map((option) => (
                <option key={option.value} value={option.value}>{option.label}</option>
              ))}
            </select>
          </label>
        </div>
        <label className="field">
          Quelques mots sur vous
          <textarea
            value={fields.biography}
            onChange={(event) => update('biography', event.target.value)}
            maxLength={1000}
            rows={4}
            placeholder="Ce qui vous anime, vous fait rire ou vous donne envie de discuter…"
            required
          />
          <span className="form__hint">{fields.biography.length}/1000</span>
        </label>
        <Feedback message={error} />
        <Feedback message={success} kind="success" />
        <div className="button-row">
          <button className="button button--primary" disabled={isSaving}>
            {isSaving ? 'Enregistrement…' : 'Enregistrer'}
          </button>
        </div>
      </form>
    </section>
  )
}
