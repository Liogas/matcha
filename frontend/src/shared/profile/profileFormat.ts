import type { Profile } from '../../api/auth'

export const GENDER_OPTIONS = [
  { value: 'woman', label: 'Femme' },
  { value: 'man', label: 'Homme' },
  { value: 'non_binary', label: 'Non-binaire' },
  { value: 'other', label: 'Autre' },
] as const

export const ORIENTATION_OPTIONS = [
  { value: 'bisexual', label: 'Bisexuel·le' },
  { value: 'heterosexual', label: 'Hétérosexuel·le' },
  { value: 'homosexual', label: 'Homosexuel·le' },
  { value: 'pansexual', label: 'Pansexuel·le' },
  { value: 'asexual', label: 'Asexuel·le' },
] as const

/** Même règle que le backend (ProfileRoutes.cpp). */
export const CITY_PATTERN = /^[A-Za-zÀ-ÖØ-öø-ÿ][A-Za-zÀ-ÖØ-öø-ÿ .'-]*$/

export function genderLabel(value: string) {
  return GENDER_OPTIONS.find((option) => option.value === value)?.label ?? value
}

export function orientationLabel(value: string) {
  return ORIENTATION_OPTIONS.find((option) => option.value === value)?.label ?? value
}

export function getAge(dateOfBirth: string) {
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

export function displayName(profile: Pick<Profile, 'first_name' | 'date_of_birth'>) {
  const age = getAge(profile.date_of_birth)
  return age !== null ? `${profile.first_name}, ${age}` : profile.first_name
}

/** Les critères vérifiés par le backend pour rendre un profil visible. */
export function completionChecks(profile: Profile) {
  return [
    { label: 'Être majeur·e (date de naissance)', complete: (getAge(profile.date_of_birth) ?? 0) >= 18 },
    { label: 'Ville valide', complete: CITY_PATTERN.test(profile.city) },
    { label: 'Genre renseigné', complete: Boolean(profile.gender) },
    { label: 'Quelques mots sur vous', complete: Boolean(profile.biography.trim()) },
    { label: 'Orientation choisie', complete: Boolean(profile.sexual_orientation) },
    { label: 'Une photo principale', complete: profile.photos.some((photo) => photo.is_primary) },
  ]
}
