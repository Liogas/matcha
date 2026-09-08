FROM debian:bookworm

RUN echo 'APT::Sandbox::User "root";' > /etc/apt/apt.conf.d/99no-sandbox

RUN apt-get update && apt-get install -y --no-install-recommends \
	build-essential \
	cmake \
	git \
	libasio-dev \
	ca-certificates \
	&& update-ca-certificates \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /app

RUN git clone --depth 1 https://github.com/CrowCpp/Crow.git /tmp/crow \
    && mkdir -p /usr/local/include \
    && cp -r /tmp/crow/include/. /usr/local/include/

COPY CMakeLists.txt .
COPY src ./src
RUN cmake -B build -S . \
	&& cmake --build build

EXPOSE 18080

CMD ["./build/matcha_backend"]