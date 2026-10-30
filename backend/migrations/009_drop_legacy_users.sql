-- La table users (001) servait à l'ancien chemin d'authentification
-- (UserRepository), remplacé par auth_accounts (002). Plus rien ne la lit.
DROP TABLE IF EXISTS users;