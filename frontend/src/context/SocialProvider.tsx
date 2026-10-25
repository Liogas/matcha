import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
} from 'react'
import type { ReactNode } from 'react'
import { useForest } from '../forest-environment'
import {
  describeApiError,
  getConversations,
  getIncomingLikeProfileIds,
  getLikedProfileIds,
  getMatchProfileIds,
  getProfile,
  getPublicProfile,
  likeProfile,
  markConversationRead as markConversationReadRequest,
  openMessagingSocket,
  sendConversationMessage,
  unlikeProfile,
  type MessagingConversation,
  type MessagingMessage,
  type MessagingSocketMessage,
  type Profile,
} from '../api/auth'
import { useAuth } from './useAuth'
import {
  SocialContext,
  type SocialContextValue,
  type SocialEvent,
  type SocketStatus,
} from './SocialContext'

/** Le backend ne pousse pas encore les likes : on les relève régulièrement. */
const POLL_INTERVAL = 20_000
const RECONNECT_DELAY = 5_000
const MAX_EVENTS = 50

function without(set: Set<number>, id: number) {
  const next = new Set(set)
  next.delete(id)
  return next
}

/**
 * État social de la personne connectée, partagé par l'en-tête et toutes les pages :
 * likes, matchs, conversations, WebSocket de messagerie et fil d'événements.
 * Monté avec une clé par utilisateur : une déconnexion repart d'un état vide.
 */
export function SocialProvider({ children }: { children: ReactNode }) {
  const { accessToken, user } = useAuth()
  const { react } = useForest()
  const userId = user ? Number(user.id) : 0

  const [isLoaded, setIsLoaded] = useState(false)
  const [loadError, setLoadError] = useState('')
  const [myProfile, setMyProfile] = useState<Profile | null>(null)
  const [likedIds, setLikedIds] = useState<Set<number>>(() => new Set())
  const [incomingIds, setIncomingIds] = useState<Set<number>>(() => new Set())
  const [matchIds, setMatchIds] = useState<Set<number>>(() => new Set())
  const [conversations, setConversations] = useState<MessagingConversation[]>([])
  const [profilesById, setProfilesById] = useState<Record<number, Profile | null>>({})
  const [socketStatus, setSocketStatus] = useState<SocketStatus>('connecting')
  const [events, setEvents] = useState<SocialEvent[]>([])

  const tokenRef = useRef(accessToken)
  const snapshotRef = useRef<{ incoming: Set<number>; matches: Set<number> } | null>(null)
  const conversationsRef = useRef<MessagingConversation[]>([])
  const activeConversationRef = useRef<number | null>(null)
  const listenersRef = useRef(new Set<(message: MessagingMessage) => void>())
  const requestedProfilesRef = useRef(new Set<number>())
  const socketRef = useRef<WebSocket | null>(null)
  const eventIdRef = useRef(0)

  useEffect(() => {
    tokenRef.current = accessToken
  }, [accessToken])

  useEffect(() => {
    conversationsRef.current = conversations
  }, [conversations])

  const pushEvent = useCallback((event: Omit<SocialEvent, 'id' | 'at' | 'read'>) => {
    eventIdRef.current += 1
    const id = eventIdRef.current
    setEvents((current) => [{ ...event, id, at: new Date(), read: false }, ...current].slice(0, MAX_EVENTS))
  }, [])

  const refresh = useCallback(async () => {
    const token = tokenRef.current
    if (!token) return
    try {
      const [liked, incoming, matches, loadedConversations] = await Promise.all([
        getLikedProfileIds(token),
        getIncomingLikeProfileIds(token),
        getMatchProfileIds(token),
        getConversations(token),
      ])

      // Comparaison avec le relevé précédent pour signaler ce qui vient d'arriver
      const previous = snapshotRef.current
      if (previous) {
        const newMatches = matches.filter((id) => !previous.matches.has(id))
        const newLikes = incoming.filter((id) => !previous.incoming.has(id) && !matches.includes(id))
        const lostLikes = [...previous.incoming].filter((id) => !incoming.includes(id))
        newLikes.forEach((accountId) => pushEvent({ kind: 'like', accountId }))
        newMatches.forEach((accountId) => pushEvent({ kind: 'match', accountId }))
        lostLikes.forEach((accountId) => pushEvent({ kind: 'unlike', accountId }))
        if (newMatches.length > 0)
          react('joy')
        else if (newLikes.length > 0)
          react('love')
      }
      snapshotRef.current = { incoming: new Set(incoming), matches: new Set(matches) }

      setLikedIds(new Set(liked))
      setIncomingIds(new Set(incoming))
      setMatchIds(new Set(matches))
      setConversations(loadedConversations.map((conversation) =>
        conversation.id === activeConversationRef.current
          ? { ...conversation, unread_count: 0 }
          : conversation))
      setLoadError('')
    } catch (error) {
      setLoadError(describeApiError(error))
    }
  }, [pushEvent, react])

  const reloadMyProfile = useCallback(async () => {
    const token = tokenRef.current
    if (!token) return
    try {
      setMyProfile(await getProfile(token))
    } catch (error) {
      setLoadError(describeApiError(error))
    }
  }, [])

  useEffect(() => {
    if (!userId) return
    let active = true
    void Promise.all([reloadMyProfile(), refresh()]).finally(() => {
      if (active) setIsLoaded(true)
    })
    const interval = window.setInterval(() => void refresh(), POLL_INTERVAL)
    const onFocus = () => void refresh()
    window.addEventListener('focus', onFocus)
    return () => {
      active = false
      window.clearInterval(interval)
      window.removeEventListener('focus', onFocus)
    }
  }, [userId, refresh, reloadMyProfile])

  const ensureProfiles = useCallback((accountIds: number[]) => {
    const token = tokenRef.current
    if (!token) return
    for (const accountId of accountIds) {
      if (requestedProfilesRef.current.has(accountId)) continue
      requestedProfilesRef.current.add(accountId)
      void getPublicProfile(token, accountId)
        .then((profile) => setProfilesById((current) => ({ ...current, [accountId]: profile })))
        .catch(() => {
          // Un profil devenu invisible (incomplet, supprimé…) ne doit pas être redemandé en boucle
          setProfilesById((current) => ({ ...current, [accountId]: null }))
        })
    }
  }, [])

  const handleSocketMessage = useCallback((socketMessage: MessagingSocketMessage) => {
    const message: MessagingMessage = {
      id: socketMessage.id,
      conversation_id: socketMessage.conversation_id,
      sender_account_id: socketMessage.sender_account_id,
      body: socketMessage.body,
      created_at: socketMessage.created_at,
      read: false,
    }
    listenersRef.current.forEach((listener) => listener(message))

    if (message.sender_account_id === userId)
      return
    const known = conversationsRef.current.some((item) => item.id === message.conversation_id)
    if (!known) {
      void refresh()
    } else if (message.conversation_id !== activeConversationRef.current) {
      setConversations((current) => current.map((conversation) =>
        conversation.id === message.conversation_id
          ? { ...conversation, unread_count: conversation.unread_count + 1 }
          : conversation))
    }
    if (message.conversation_id !== activeConversationRef.current) {
      pushEvent({
        kind: 'message',
        accountId: message.sender_account_id,
        conversationId: message.conversation_id,
        preview: message.body.slice(0, 90),
      })
      react('startle')
    }
  }, [pushEvent, react, refresh, userId])

  useEffect(() => {
    if (!userId) return
    let cancelled = false
    let socket: WebSocket | null = null
    let retryTimer: number | undefined

    const connect = () => {
      const token = tokenRef.current
      if (cancelled || !token) return
      setSocketStatus('connecting')
      socket = openMessagingSocket(token, handleSocketMessage)
      socketRef.current = socket
      socket.addEventListener('open', () => {
        if (!cancelled) setSocketStatus('open')
      })
      socket.addEventListener('close', () => {
        if (cancelled) return
        setSocketStatus('closed')
        retryTimer = window.setTimeout(connect, RECONNECT_DELAY)
      })
    }

    // Différé d'un tick : en StrictMode, le premier montage est aussitôt démonté
    const startTimer = window.setTimeout(connect, 0)
    return () => {
      cancelled = true
      window.clearTimeout(startTimer)
      window.clearTimeout(retryTimer)
      socket?.close()
      if (socketRef.current === socket)
        socketRef.current = null
    }
  }, [userId, handleSocketMessage])

  const like = useCallback(async (accountId: number) => {
    const token = tokenRef.current
    if (!token) return { matched: false }
    const result = await likeProfile(token, accountId)
    setLikedIds((current) => new Set(current).add(accountId))
    if (result.matched) {
      snapshotRef.current?.matches.add(accountId)
      setMatchIds((current) => new Set(current).add(accountId))
      pushEvent({ kind: 'match', accountId })
      react('joy')
      // La conversation du match vient d'être créée côté serveur
      void refresh()
    } else {
      react('love')
    }
    return { matched: result.matched }
  }, [pushEvent, react, refresh])

  const unlike = useCallback(async (accountId: number) => {
    const token = tokenRef.current
    if (!token) return
    await unlikeProfile(token, accountId)
    snapshotRef.current?.matches.delete(accountId)
    setLikedIds((current) => without(current, accountId))
    setMatchIds((current) => without(current, accountId))
    setConversations((current) => current.filter((item) => item.other_account_id !== accountId))
  }, [])

  const subscribeMessages = useCallback((listener: (message: MessagingMessage) => void) => {
    listenersRef.current.add(listener)
    return () => {
      listenersRef.current.delete(listener)
    }
  }, [])

  const sendMessage = useCallback(async (conversationId: number, body: string) => {
    const socket = socketRef.current
    if (socket?.readyState === WebSocket.OPEN) {
      // Le serveur renvoie le message à l'expéditeur : il arrivera par les abonnés
      socket.send(JSON.stringify({ conversation_id: conversationId, body }))
      return
    }
    const token = tokenRef.current
    if (!token) return
    const message = await sendConversationMessage(token, conversationId, body)
    listenersRef.current.forEach((listener) => listener(message))
  }, [])

  const setActiveConversation = useCallback((conversationId: number | null) => {
    activeConversationRef.current = conversationId
    if (conversationId === null) return
    setConversations((current) => current.map((conversation) =>
      conversation.id === conversationId ? { ...conversation, unread_count: 0 } : conversation))
  }, [])

  const markConversationRead = useCallback(async (conversationId: number) => {
    const token = tokenRef.current
    if (!token) return
    await markConversationReadRequest(token, conversationId)
    setConversations((current) => current.map((conversation) =>
      conversation.id === conversationId ? { ...conversation, unread_count: 0 } : conversation))
  }, [])

  const markEventsRead = useCallback(() => {
    setEvents((current) => current.some((event) => !event.read)
      ? current.map((event) => ({ ...event, read: true }))
      : current)
  }, [])

  const value = useMemo<SocialContextValue>(() => ({
    isLoaded,
    loadError,
    myProfile,
    setMyProfile,
    reloadMyProfile,
    likedIds,
    incomingIds,
    matchIds,
    conversations,
    refresh,
    like,
    unlike,
    profilesById,
    ensureProfiles,
    socketStatus,
    subscribeMessages,
    sendMessage,
    setActiveConversation,
    markConversationRead,
    events,
    markEventsRead,
  }), [
    isLoaded,
    loadError,
    myProfile,
    reloadMyProfile,
    likedIds,
    incomingIds,
    matchIds,
    conversations,
    refresh,
    like,
    unlike,
    profilesById,
    ensureProfiles,
    socketStatus,
    subscribeMessages,
    sendMessage,
    setActiveConversation,
    markConversationRead,
    events,
    markEventsRead,
  ])

  return <SocialContext.Provider value={value}>{children}</SocialContext.Provider>
}
