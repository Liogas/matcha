COMPOSE      = docker compose
COMPOSE_DEV  = docker compose -f docker-compose.yml -f docker-compose.dev.yml

all:
	@echo "Dev        : build / up / down / start / stop / rebuild / test / sanitize / logs / shell"
	@echo "Production : prod / prod-down"
	@echo "Divers     : secrets"


secrets:
	@sh scripts/init-secrets.sh

# --- Developpement : code monte dans le conteneur, recompilation a chaud ---

build: secrets
	$(COMPOSE_DEV) up --build --force-recreate

up: secrets
	$(COMPOSE_DEV) up

down:
	$(COMPOSE_DEV) down

start:
	$(COMPOSE_DEV) start

stop:
	$(COMPOSE_DEV) stop

# Recompile le backend et redemarre le serveur sans redemarrer le conteneur.
rebuild:
	$(COMPOSE_DEV) exec backend matcha-rebuild

# Compile executable + tests puis lance ctest.
test:
	$(COMPOSE_DEV) exec backend sh -c 'matcha-build all && ctest --preset debug'

# Compile et lance tous les tests sous ASan + UBSan (dossier build/sanitize).
sanitize:
	$(COMPOSE_DEV) exec backend sh -c 'cmake --preset sanitize > /dev/null && cmake --build --preset sanitize --parallel "$$(nproc)" && ctest --preset sanitize'

logs:
	$(COMPOSE_DEV) logs -f backend

shell:
	$(COMPOSE_DEV) exec backend bash

# --- Production : image finale legere, sans code source ni compilateur ---

prod: secrets
	$(COMPOSE) up --build --force-recreate

prod-down:
	$(COMPOSE) down

.PHONY: all secrets build up down start stop rebuild test sanitize logs shell prod prod-down
