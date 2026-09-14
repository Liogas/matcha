
all:
	@echo "Liste de cmd disponibles : build / up / down / start / stop"

build:
	docker compose up --build

up:
	docker compose up

down:
	docker compose down

start:
	docker compose start

stop:
	docker compose stop

