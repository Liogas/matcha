FROM debian:bookworm

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
	&& update-ca-certificates \
	&& rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh

WORKDIR /app

COPY . .

RUN /opt/vcpkg/vcpkg install

RUN find /app/vcpkg_installed -name "crow.h" -o -name "CrowConfig.cmake"

RUN cmake -B build -S . \
	-DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
	&& cmake --build build

EXPOSE 18080

CMD ["sleep", "infinity"]
# CMD ["./build/matcha_backend"]