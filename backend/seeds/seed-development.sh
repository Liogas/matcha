#!/bin/sh
set -eu

docker compose exec -T database sh -c \
	'psql -v ON_ERROR_STOP=1 -U "$POSTGRES_USER" -d "$POSTGRES_DB"' \
	< backend/seeds/development_profiles.sql

image='UklGRiIAAABXRUJQVlA4IBQAAAAwAQCdASoBAAEALAsAAP7/+P8AAA=='
for username in aliceb brunom camiller dianep eliasd fatoud gabriell inesm julienf leilag
do
	docker compose exec -T backend sh -c \
		"mkdir -p /var/lib/matcha/uploads/fixtures && \
		printf '%s' '$image' | base64 -d > \
		/var/lib/matcha/uploads/fixtures/profile-$username.webp"
done
