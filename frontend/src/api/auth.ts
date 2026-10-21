const API_BASE_URL = (import.meta.env.VITE_AUTH_API_URL ?? 'http://localhost:18080')
  .replace(/\/+$/, '')

export interface MessagingSocketMessage {
  type: 'message'
  id: number
  conversation_id: number
  sender_account_id: number
  body: string
  created_at: string
}

export interface MessagingConversation {
  id: number
  other_account_id: number
  unread_count: number
}

export interface MessagingMessage {
  id: number
  conversation_id: number
  sender_account_id: number
  body: string
  created_at: string
  read: boolean
}

export async function getConversations(accessToken: string) {
  const { body } = await request<{ conversations?: MessagingConversation[] }>(
    '/api/conversations',
    { headers: { Authorization: `Bearer ${accessToken}` } },
  )
  return Array.isArray(body.conversations) ? body.conversations : []
}

export async function getConversationMessages(
  accessToken: string,
  conversationId: number,
) {
  const { body } = await request<{ messages?: MessagingMessage[] }>(
    `/api/conversations/${conversationId}/messages?limit=100`,
    { headers: { Authorization: `Bearer ${accessToken}` } },
  )
  return Array.isArray(body.messages) ? body.messages.reverse() : []
}

export async function sendConversationMessage(
  accessToken: string,
  conversationId: number,
  body: string,
) {
  const { body: response } = await request<MessagingMessage>(
    `/api/conversations/${conversationId}/messages`,
    {
      method: 'POST',
      headers: { Authorization: `Bearer ${accessToken}` },
      body: JSON.stringify({ body }),
    },
  )
  return response
}

export async function markConversationRead(
  accessToken: string,
  conversationId: number,
) {
  await request<ApiResponse>(`/api/conversations/${conversationId}/read`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${accessToken}` },
  })
}

export function openMessagingSocket(
  accessToken: string,
  onMessage: (message: MessagingSocketMessage) => void,
  onError?: () => void,
): WebSocket {
  const socketUrl = API_BASE_URL.replace(/^http/, 'ws') +
    `/api/messaging/ws?access_token=${encodeURIComponent(accessToken)}`
  const socket = new WebSocket(socketUrl)
  let opened = false
  socket.addEventListener('open', () => {
    opened = true
  })
  socket.addEventListener('message', (event) => {
    try {
      const message = JSON.parse(event.data) as MessagingSocketMessage
      if (message.type === 'message')
        onMessage(message)
    } catch {
      onError?.()
    }
  })
  socket.addEventListener('error', () => {
    // React StrictMode closes the first development connection intentionally.
    // Do not report that pre-open cleanup as a messaging outage.
    if (opened) onError?.()
  })
  return socket
}

export interface AuthUser {
  id: string
  email?: string
}

export interface ProfilePhoto {
  id: number
  position: number
  is_primary: boolean
  content_type: string
  file_size: number
  width: number
  height: number
  url: string
}

export interface Profile {
  account_id: number
  username: string
  first_name: string
  last_name: string
  date_of_birth: string
  gender: string
  sexual_orientation: string
  biography: string
  city: string
  completed: boolean
  photos: ProfilePhoto[]
  photo_url?: string
}

interface ApiResponse {
  message?: string
  error?: string
}

interface LoginResponse extends ApiResponse {
  access_token: string
  user: {
    id: string | number
    email: string
  }
}

interface TokenResponse extends ApiResponse {
  access_token: string
}

export class ApiError extends Error {
  readonly status: number
  readonly retryAfterSeconds?: number

  constructor(message: string, status: number, retryAfterSeconds?: number) {
    super(message)
    this.name = 'ApiError'
    this.status = status
    this.retryAfterSeconds = retryAfterSeconds
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null
}

async function request<T>(
  path: string,
  options: RequestInit = {},
): Promise<{ body: T; response: Response }> {
  const headers = new Headers(options.headers)
  if (options.body && !headers.has('Content-Type')) {
    headers.set('Content-Type', 'application/json')
  }

  const response = await fetch(`${API_BASE_URL}${path}`, {
    ...options,
    headers,
    credentials: 'include',
  })

  let body: unknown
  if (response.headers.get('Content-Type')?.includes('application/json')) {
    try {
      body = await response.json()
    } catch {
      throw new Error('Le serveur a renvoyé une réponse illisible.')
    }
  } else {
    body = await response.text()
  }

  if (!response.ok) {
    const message = isRecord(body) && typeof body.error === 'string'
      ? body.error
      : 'La requête a échoué.'
    const retryAfter = response.headers.get('Retry-After')
    throw new ApiError(
      message,
      response.status,
      retryAfter ? Number(retryAfter) : undefined,
    )
  }

  return { body: body as T, response }
}

export function describeApiError(error: unknown): string {
  if (!(error instanceof ApiError)) {
    return 'Une erreur inattendue est survenue. Vérifiez votre connexion puis réessayez.'
  }

  if (error.status === 401)
    return 'Email ou mot de passe incorrect, ou lien expiré.'
  if (error.status === 403)
    return 'Cette requête n’est pas autorisée depuis cette origine.'
  if (error.status === 409)
    return 'Cette adresse e-mail ou ce nom d’utilisateur est déjà utilisé.'
  if (error.status === 429) {
    return error.retryAfterSeconds
      ? `Trop de tentatives. Réessayez dans ${error.retryAfterSeconds} secondes.`
      : 'Trop de tentatives. Réessayez un peu plus tard.'
  }
  if (/account created, but verification email delivery failed/i.test(error.message)) {
    return 'Votre compte est créé, mais l’e-mail n’a pas pu être envoyé. Vous pourrez demander un nouveau lien.'
  }
  if (error.status >= 500)
    return 'Le service est momentanément indisponible. Réessayez plus tard.'
  if (/password must/i.test(error.message))
    return 'Le mot de passe doit contenir entre 12 et 128 caractères et ne pas être trop courant.'
  if (/invalid|expired|already used/i.test(error.message))
    return 'Ce lien est invalide, expiré ou a déjà été utilisé.'
  if (/already in use/i.test(error.message))
    return 'Cette adresse e-mail ou ce nom d’utilisateur est déjà utilisé.'
  if (/valid email/i.test(error.message))
    return 'Saisissez une adresse e-mail valide.'

  return error.message
}

export async function registerAccount(input: {
  email: string
  username: string
  first_name: string
  last_name: string
  password: string
}) {
  const { body } = await request<ApiResponse>('/api/auth/register', {
    method: 'POST',
    body: JSON.stringify(input),
  })
  return body
}

export async function resendVerification(email: string) {
  const { body } = await request<ApiResponse>('/api/auth/email/resend', {
    method: 'POST',
    body: JSON.stringify({ email }),
  })
  return body
}

export async function verifyEmail(token: string) {
  const { body } = await request<ApiResponse>('/api/auth/email/verify', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `token=${encodeURIComponent(token)}`,
  })
  return body
}

export async function requestPasswordReset(email: string) {
  const { body } = await request<ApiResponse>('/api/auth/password-reset/request', {
    method: 'POST',
    body: JSON.stringify({ email }),
  })
  return body
}

export async function confirmPasswordReset(token: string, password: string) {
  const { body } = await request<ApiResponse>('/api/auth/password-reset/confirm', {
    method: 'POST',
    body: JSON.stringify({ token, password }),
  })
  return body
}

export async function signIn(email: string, password: string) {
  const { body } = await request<LoginResponse>('/api/auth/login', {
    method: 'POST',
    body: JSON.stringify({ email, password }),
  })

  if (
    typeof body.access_token !== 'string' ||
    !body.user ||
    (typeof body.user.id !== 'string' &&
      typeof body.user.id !== 'number') ||
    typeof body.user.email !== 'string'
  ) {
    throw new Error('La réponse de connexion est incomplète.')
  }

  return {
    accessToken: body.access_token,
    user: { id: String(body.user.id), email: body.user.email },
  }
}

export async function refreshSession() {
  const { body } = await request<TokenResponse>('/api/auth/refresh', {
    method: 'POST',
  })
  if (typeof body.access_token !== 'string')
    throw new Error('La réponse de renouvellement est incomplète.')
  return body.access_token
}

export async function getCurrentUser(accessToken: string): Promise<AuthUser> {
  const { body } = await request<{
    user?: { id?: string | number }
  }>('/api/auth/me', {
    headers: { Authorization: `Bearer ${accessToken}` },
  })

  if (body.user?.id === undefined)
    throw new Error('La réponse du compte connecté est incomplète.')

  return { id: String(body.user.id) }
}

export async function signOut() {
  await request<ApiResponse>('/api/auth/logout', { method: 'POST' })
}

export async function signOutAll(accessToken: string) {
  await request<ApiResponse>('/api/auth/logout-all', {
    method: 'POST',
    headers: { Authorization: `Bearer ${accessToken}` },
  })
}

export async function getProfile(accessToken: string): Promise<Profile> {
  const { body } = await request<{ profile: Omit<Profile, 'photos'>; photos?: ProfilePhoto[] }>(
    '/api/profile',
    { headers: { Authorization: `Bearer ${accessToken}` } },
  )
  return { ...body.profile, photos: body.photos ?? [] }
}

export async function getProfiles(accessToken: string): Promise<Profile[]> {
  const { body } = await request<{ profiles: Profile[] }>('/api/profiles', {
    headers: { Authorization: `Bearer ${accessToken}` },
  })
  return body.profiles
}

export async function getLikedProfileIds(accessToken: string): Promise<number[]> {
  const { body } = await request<{ liked_account_ids?: number[] }>('/api/likes', {
    headers: { Authorization: `Bearer ${accessToken}` },
  })
  return Array.isArray(body.liked_account_ids) ? body.liked_account_ids : []
}

export async function getIncomingLikeProfileIds(accessToken: string): Promise<number[]> {
  const { body } = await request<{ liker_account_ids?: number[] }>('/api/likes/received', {
    headers: { Authorization: `Bearer ${accessToken}` },
  })
  return Array.isArray(body.liker_account_ids) ? body.liker_account_ids : []
}

export async function getMatchProfileIds(accessToken: string): Promise<number[]> {
  const { body } = await request<{ matches?: { other_account_id?: number }[] }>('/api/matches', {
    headers: { Authorization: `Bearer ${accessToken}` },
  })
  return Array.isArray(body.matches)
    ? body.matches.flatMap((match) =>
      typeof match.other_account_id === 'number' ? [match.other_account_id] : [])
    : []
}

export async function likeProfile(accessToken: string, accountId: number) {
  const { body } = await request<{ liked: boolean; matched: boolean; match_id?: number }>(
    `/api/profiles/${accountId}/like`,
    { method: 'POST', headers: { Authorization: `Bearer ${accessToken}` } },
  )
  return body
}

export async function unlikeProfile(accessToken: string, accountId: number) {
  await request<ApiResponse>(`/api/profiles/${accountId}/like`, {
    method: 'DELETE',
    headers: { Authorization: `Bearer ${accessToken}` },
  })
}

export async function updateProfile(
  accessToken: string,
  profile: Pick<
    Profile,
    'date_of_birth' | 'gender' | 'sexual_orientation' | 'biography' | 'city'
  >,
) {
  const { body } = await request<{ profile: Omit<Profile, 'photos'>; photos?: ProfilePhoto[] }>(
    '/api/profile',
    {
      method: 'PATCH',
      headers: { Authorization: `Bearer ${accessToken}` },
      body: JSON.stringify(profile),
    },
  )
  return { ...body.profile, photos: body.photos ?? [] }
}

export async function uploadProfilePhoto(
  accessToken: string,
  position: number,
  file: File,
) {
  const { body } = await request<ApiResponse>('/api/profile/photos?position=' + position, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${accessToken}`,
      'Content-Type': file.type,
    },
    body: await file.arrayBuffer(),
  })
  return body
}

export async function replaceProfilePhoto(
  accessToken: string,
  photoId: number,
  file: File,
) {
  const { body } = await request<ApiResponse>(`/api/profile/photos/${photoId}`, {
    method: 'PUT',
    headers: {
      Authorization: `Bearer ${accessToken}`,
      'Content-Type': file.type,
    },
    body: await file.arrayBuffer(),
  })
  return body
}

export async function deleteProfilePhoto(accessToken: string, photoId: number) {
  const { body } = await request<ApiResponse>(`/api/profile/photos/${photoId}`, {
    method: 'DELETE',
    headers: { Authorization: `Bearer ${accessToken}` },
  })
  return body
}

export async function setPrimaryProfilePhoto(
  accessToken: string,
  photoId: number,
) {
  const { body } = await request<ApiResponse>(
    `/api/profile/photos/${photoId}/primary`,
    {
      method: 'POST',
      headers: { Authorization: `Bearer ${accessToken}` },
    },
  )
  return body
}

export async function getProfilePhoto(
  accessToken: string,
  photo: ProfilePhoto,
) {
  const response = await fetch(`${API_BASE_URL}${photo.url}`, {
    headers: { Authorization: `Bearer ${accessToken}` },
    credentials: 'include',
  })
  if (!response.ok)
    throw new ApiError('Unable to load profile photo', response.status)
  return URL.createObjectURL(await response.blob())
}

export async function getImageUrl(accessToken: string, path: string) {
  const response = await fetch(`${API_BASE_URL}${path}`, {
    headers: { Authorization: `Bearer ${accessToken}` },
    credentials: 'include',
  })
  if (!response.ok)
    throw new ApiError('Unable to load profile photo', response.status)
  return URL.createObjectURL(await response.blob())
}
