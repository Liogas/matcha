# Etapes :
#   deps    : compilateur + vcpkg + dependances C++ deja compilees (long, mis en cache)
#   dev     : deps + scripts de dev ; le code est monte depuis l'hote
#   build   : deps + code copie, compile en Release
#   runtime : image legere avec seulement le binaire, les migrations et les libs systeme
# Choisir une etape : `target:` dans docker compose, ou `docker build --target <etape>`.

FROM debian:bookworm AS deps

RUN echo 'APT::Sandbox::User "root";' > /etc/apt/apt.conf.d/99no-sandbox

RUN apt-get update && apt-get install -y --no-install-recommends \
	build-essential \
	cmake \
	git \
	tar \
	zip \
	curl \
	unzip \
	ca-certificates \
	pkg-config \
	python3 \
	bison \
	flex \
	linux-libc-dev \
	autoconf \
	autoconf-archive \
	automake \
	libtool \
	valgrind \
	libvips-dev \
	&& update-ca-certificates \
	&& rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
	&& /opt/vcpkg/bootstrap-vcpkg.sh

# Seul le manifeste est copie : tant que vcpkg.json ne change pas, cette
# couche (plusieurs minutes de compilation) reste en cache.
# Les dependances sont installees hors de /app pour ne pas etre masquees
# par le montage du code source en dev.
ENV VCPKG_ROOT=/opt/vcpkg \
	VCPKG_INSTALLED_DIR=/opt/vcpkg_installed
WORKDIR /opt/manifest
COPY vcpkg.json ./
RUN /opt/vcpkg/vcpkg install --x-install-root="$VCPKG_INSTALLED_DIR"


FROM deps AS dev

COPY dev/ /usr/local/bin/
RUN chmod +x /usr/local/bin/matcha-*

WORKDIR /app
EXPOSE 18080
CMD ["matcha-dev-server"]


FROM deps AS build

WORKDIR /app
COPY . .
RUN cmake --preset release \
	&& cmake --build --preset release --target matcha_backend --parallel "$(nproc)"


FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
	ca-certificates \
	&& rm -rf /var/lib/apt/lists/*

RUN useradd --system --no-create-home --uid 10001 matcha \
	&& mkdir -p /var/lib/matcha/uploads \
	&& chown matcha:matcha /var/lib/matcha/uploads

WORKDIR /app
COPY --from=build /app/build/release/matcha_backend ./matcha_backend
COPY migrations ./migrations

USER matcha
EXPOSE 18080
CMD ["./matcha_backend"]
