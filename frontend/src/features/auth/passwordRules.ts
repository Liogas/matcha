/** Même règle que le backend : 12 à 128 caractères (en points de code). */
export function validatePassword(password: string) {
  const length = Array.from(password).length
  if (length < 12 || length > 128)
    return 'Choisissez un mot de passe entre 12 et 128 caractères.'
  return ''
}
