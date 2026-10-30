# Une règle transversale centralisée
> Une règle qui s'applique partout doit être écrite à un seul endroit, et testée à chaque point d'entrée.

## Mise en situation
Bloquer doit agir sur les suggestions, la recherche, la consultation, les likes, les notifications, les messages HTTP, les messages [WebSocket](../temps-reel/websocket.md) et la liste des conversations. Soit huit points d'entrée. Si un seul est oublié, une personne bloquée peut encore écrire à quelqu'un qui voulait s'en protéger. C'est une faute grave pour un site de rencontre.

## La théorie
### Une préoccupation transversale
Certaines règles traversent tout le code : autorisation, journalisation, blocage. Les recopier est la meilleure façon d'en oublier une.

### Trois niveaux
- **Une définition unique** : une [vue SQL](../sql/vues-sql.md) ou une fonction `is_blocked`.
- **Une application au bon niveau** : dans le moteur de recherche unique, dans `MatchService`, dans `IConversationPolicy`.
- **Des tests par point d'entrée** : la liste des huit devient vérifiable.

### L'interface de politique
La messagerie ne doit rien savoir du blocage. Elle demande :

```cpp
class IConversationPolicy {
public:
    virtual ~IConversationPolicy() = default;
    virtual bool canSend(AccountId sender, ConversationId conversation) = 0;
};
```

Matcha répond avec ses règles : match actif et absence de blocage.

## Exemple déroulé : bloquer
```text
1. POST /api/profiles/7/block
2. Transaction :
     INSERT user_blocks
     DELETE like dans les deux sens
     DELETE match
     conversation désactivée
3. Commit
4. Les routes suivantes voient le blocage
```

Puis les tests :

```cpp
TEST_F(BlockTest, BlockedUserCannotSendMessage) {
    block(alice, bob);
    EXPECT_EQ(sendMessage(bob, alice).code, 403);
}
TEST_F(BlockTest, BlockedUserIsHiddenFromSearch) { ... }
TEST_F(BlockTest, BlockedProfileReturns404) { ... }
```

Un test par entrée, en passant par la route, pas par la vue : c'est le chemin réel qui est vérifié.

### 404 ou 403
Pour un profil bloqué, `404` ne révèle pas le blocage. La personne bloquée ne sait pas qu'elle l'est.

### Le WebSocket
Le contrôle a lieu à chaque message, voir le [WebSocket](../temps-reel/websocket.md).

## Erreurs fréquentes
- Tester seulement la fonction SQL.
- Oublier le sens inverse : A bloque B, B ne doit pas voir A non plus.
- Oublier les notifications déjà en attente.

## À retenir
Écris la règle une fois, applique-la près des données, et fais confiance aux tests, pas à ta mémoire.

## Exercice
Pourquoi le test doit-il passer par la route, et non directement par `is_blocked` ?

## Corrigé
`is_blocked` peut être parfaite, et une route peut oublier de l'appeler. Le risque n'est pas dans la règle, mais dans son application. Seul un test de la route détecte cet oubli.

## Voir aussi

- [Les vues SQL](../sql/vues-sql.md)
- [Notifications stockées et compteurs](../temps-reel/notifications-stockees.md)
- [La pyramide de tests](../tests/pyramide.md)
