#!/bin/sh
# Prepare tout ce qui doit exister avant `docker compose up` :
# .env (copie de .env.example) et les secrets manquants dans ./secrets.
# Idempotent : ne regenere jamais un secret deja present.
set -eu

cd "$(dirname "$0")/.."
umask 077

if [ ! -f .env ]; then
	cp .env.example .env
	echo "[init-secrets] .env cree depuis .env.example"
fi

# Ajoute a .env les variables de .env.example qui y manquent.
grep -E '^[A-Z_]+=' .env.example | while IFS= read -r line; do
	key=${line%%=*}
	if ! grep -q "^${key}=" .env; then
		printf '%s\n' "$line" >> .env
		echo "[init-secrets] ${key} ajoute a .env"
	fi
done

mkdir -p secrets
chmod 700 secrets

if [ ! -s secrets/db-password ]; then
	openssl rand -hex 32 > secrets/db-password
	echo "[init-secrets] secrets/db-password genere"
fi

if [ ! -s secrets/redis-password ]; then
	openssl rand -hex 32 > secrets/redis-password
	echo "[init-secrets] secrets/redis-password genere"
fi

if [ ! -s secrets/jwt-private.pem ]; then
	openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 \
		-out secrets/jwt-private.pem 2>/dev/null
	echo "[init-secrets] secrets/jwt-private.pem genere"
fi

chmod 600 secrets/*
