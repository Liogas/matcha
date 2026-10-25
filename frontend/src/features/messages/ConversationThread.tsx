import { useEffect, useMemo, useRef, useState } from 'react'
import { describeApiError, getConversationMessages, type MessagingMessage } from '../../api/auth'
import { useAuth } from '../../context/useAuth'
import { useSocial } from '../../context/useSocial'
import { Feedback } from '../../shared/ui/Feedback'
import { MessageComposer, MessageThread, type ChatMessage } from '../../ui-kit/messaging'

/** Charge une conversation, suit les nouveaux messages et les marque comme lus. */
export function ConversationThread({ conversationId, otherName }: { conversationId: number; otherName: string }) {
  const { accessToken, user } = useAuth()
  const { subscribeMessages, sendMessage, setActiveConversation, markConversationRead } = useSocial()
  const userId = user ? Number(user.id) : 0
  const [messages, setMessages] = useState<MessagingMessage[]>([])
  const [isLoading, setIsLoading] = useState(true)
  const [error, setError] = useState('')
  const tokenRef = useRef(accessToken)

  useEffect(() => {
    tokenRef.current = accessToken
  }, [accessToken])

  useEffect(() => {
    setActiveConversation(conversationId)
    return () => setActiveConversation(null)
  }, [conversationId, setActiveConversation])

  useEffect(() => {
    const token = tokenRef.current
    if (!token) return
    let active = true
    void getConversationMessages(token, conversationId)
      .then((loaded) => {
        if (!active) return
        // Des messages ont pu arriver par le WebSocket pendant le chargement
        setMessages((current) => {
          const known = new Set(loaded.map((message) => message.id))
          return [...loaded, ...current.filter((message) => !known.has(message.id))]
        })
        return markConversationRead(conversationId)
      })
      .catch((loadError) => {
        if (active) setError(describeApiError(loadError))
      })
      .finally(() => {
        if (active) setIsLoading(false)
      })
    return () => {
      active = false
    }
  }, [conversationId, markConversationRead])

  useEffect(() => subscribeMessages((message) => {
    if (message.conversation_id !== conversationId) return
    setMessages((current) => current.some((item) => item.id === message.id) ? current : [...current, message])
    if (message.sender_account_id !== userId)
      void markConversationRead(conversationId).catch(() => undefined)
  }), [conversationId, markConversationRead, subscribeMessages, userId])

  const chatMessages = useMemo<ChatMessage[]>(() => messages.map((message) => ({
    id: message.id,
    body: message.body,
    createdAt: message.created_at,
    own: message.sender_account_id === userId,
  })), [messages, userId])

  async function handleSend(body: string) {
    setError('')
    try {
      await sendMessage(conversationId, body)
    } catch (sendError) {
      setError(describeApiError(sendError))
      throw sendError
    }
  }

  return (
    <>
      <MessageThread
        locale="fr-FR"
        messages={chatMessages}
        loading={isLoading}
        emptyState={<>Aucun message. Dites bonjour à {otherName} !</>}
      />
      {error && <div className="messages__error"><Feedback message={error} /></div>}
      <MessageComposer onSend={handleSend} placeholder={`Écrire à ${otherName}…`} />
    </>
  )
}
