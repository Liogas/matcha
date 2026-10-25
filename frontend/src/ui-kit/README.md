# ui-kit

Composants autonomes pour les notifications et la messagerie. Ils ne
dépendent ni de l'API, ni du routeur, ni des contextes de Matcha : toutes les
données et actions passent par les props. Pour les réutiliser ailleurs,
copiez le dossier `ui-kit/` (React 18+ requis).

## Notifications

```tsx
import { NotificationCenter, Toaster, useToasts } from './ui-kit/notifications'

const { toasts, push, dismiss } = useToasts()

<NotificationCenter
  items={[{ id: 1, title: 'Nouveau message', unread: true, timestamp: new Date() }]}
  onOpenChange={(open) => { if (!open) markAllRead() }}
/>
<Toaster toasts={toasts} onDismiss={dismiss} position="bottom-left" />
push({ title: 'C’est un match !', tone: 'highlight' })
```

## Messagerie

```tsx
import { ChatLayout, ConversationList, MessageComposer, MessageThread } from './ui-kit/messaging'

<ChatLayout hasSelection={Boolean(selected)} onBack={() => select(null)}
  sidebar={<ConversationList conversations={items} selectedId={selected} onSelect={select} />}>
  <MessageThread messages={messages} />
  <MessageComposer onSend={send} />
</ChatLayout>
```

## Thème

Les styles sont dans la couche CSS `ui-kit` et se règlent par variables,
à définir sur n'importe quel ancêtre :

| Variable | Rôle |
| --- | --- |
| `--uik-surface`, `--uik-surface-sunken`, `--uik-surface-hover` | Fonds |
| `--uik-ink`, `--uik-ink-muted` | Textes |
| `--uik-line` | Bordures |
| `--uik-accent`, `--uik-accent-ink` | Action principale, bulles envoyées |
| `--uik-highlight` | Pastilles de non-lus |
| `--uik-bubble`, `--uik-bubble-ink` | Bulles reçues |
| `--uik-radius`, `--uik-shadow`, `--uik-font` | Forme et typographie |
| `--uik-thread-height` | Hauteur du fil de messages |
