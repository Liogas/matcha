# Notifications stockées et compteurs
> La notification vit en base, le [WebSocket](websocket.md) ne fait que la signaler.

## Mise en situation
Quelqu'un te like pendant que tu es hors ligne. À ta connexion, la notification doit être là, avec le compteur de non-lus sur toutes les pages. Le sujet exige cinq événements : like reçu, profil consulté, message reçu, like retourné, unlike par un connecté.

## La théorie
### Le modèle
```sql
notifications(id, recipient_id, actor_id, type, payload JSONB, created_at, read_at)
```

- `recipient_id` : le destinataire.
- `actor_id` : l'auteur, nécessaire pour les règles de blocage.
- `type` : le genre d'événement.
- `payload` : le détail, en [JSONB](../sql/jsonb.md).
- `read_at` : `NULL` si non lue.

L'[index](../sql/index.md) `(recipient_id, read_at, id DESC)` sert à lister et à compter.

### Le flux de NotificationDispatcher
```text
1. Filtre : cette notification est-elle autorisée ?
2. Stockage
3. Après le commit : publication du message et du compteur
```

### Le filtre
`cpp-notify` est générique : la lib ne connaît pas le blocage. Elle reçoit un `INotificationFilter`, fourni par matcha, qui porte les règles :

- rien si l'acteur est bloqué ;
- rien d'un utilisateur qu'on a unliké ;
- pas de doublon sur les visites répétées.

### Le compteur
Deux approches :

- **Compter** : `SELECT COUNT(*) ... WHERE recipient_id = $1 AND read_at IS NULL`. Toujours juste. Un [index](../sql/index.md) partiel le rend rapide.
- **Stocker** : une colonne incrémentée. Rapide, mais elle peut dériver.

Commence par compter.

### Les routes
```text
GET  /api/notifications?cursor=
GET  /api/notifications/unread-count
POST /api/notifications/read
```

## Exemple déroulé : une visite
```text
1. Bob ouvre le profil d'Alice
2. La visite est enregistrée
3. Filtre : Alice a-t-elle bloqué Bob ? Déjà notifiée dans l'heure ?
4. INSERT notification
5. Commit
6. hub.publish("account:alice", notification + unread_count: 4)
7. Le badge passe à 4 sur toutes ses pages
```

### Regrouper
« 3 personnes ont visité votre profil » est plus agréable, mais c'est optionnel.

### Marquer comme lu
`UPDATE ... SET read_at = NOW() WHERE recipient_id = $1 AND id = ANY($2)`. La condition sur `recipient_id` empêche de marquer les notifications d'un autre : c'est un contrôle d'accès.

## Erreurs fréquentes
- Oublier le filtre dans un des cinq chemins.
- Notifier soi-même.
- Ne pas pousser le compteur après un marquage comme lu sur un autre onglet.

## À retenir
La base est la vérité, le push est une accélération, le filtre est la politique.

## Exercice
Pourquoi le filtre est-il une [interface](../cpp/interfaces-injection.md) fournie par matcha ?

## Corrigé
La lib ne doit rien savoir du blocage ni de l'unlike, pour rester réutilisable. Elle définit un contrat, et l'application y met ses règles. On peut ainsi tester la lib avec un faux filtre, et les règles séparément.

## Voir aussi

- [JSONB](../sql/jsonb.md)
- [L'outbox et publier après le commit](outbox.md)
- [Fan-out et Pub/Sub](fan-out.md)
- [Une règle transversale centralisée](../metier/policy-centralisee.md)
