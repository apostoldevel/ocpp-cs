FROM debian:bookworm-slim AS builder

LABEL project="cs"

ENV PROJECT_NAME 'cs'
ENV BUILD_MODE 'release'

RUN set -eux; \
    apt update && apt install -y --no-install-recommends \
    apt-utils bash build-essential cmake cmake-data g++ gcc \
    libcurl4-openssl-dev libssl-dev make pkg-config lsb-release curl ca-certificates git && \
    apt-get clean && rm -rf /var/lib/apt/lists/*;

RUN set -eux; \
    git clone https://github.com/apostoldevel/ocpp-cs.git /opt/$PROJECT_NAME;

WORKDIR /opt/$PROJECT_NAME

RUN set -eux; \
    cp -r docker/CMakeLists.txt .; \
    cp -r docker/conf .; \
    cp -r docker/www .; \
    ./configure --not-pull-self --$BUILD_MODE; \
    cd cmake-build-$BUILD_MODE; \
    make install; \
    cd ..; \
    rm -rf cmake-build-$BUILD_MODE; \
    rm -rf src;

FROM debian:bookworm-slim AS cs

ENV PROJECT_NAME 'cs'

RUN set -eux; \
    apt update && apt install -y --no-install-recommends \
    apt-utils bash ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*;

RUN mkdir -p /var/log/$PROJECT_NAME

COPY --from=builder /usr/sbin/$PROJECT_NAME /usr/sbin/$PROJECT_NAME
COPY --from=builder /etc/$PROJECT_NAME /etc/$PROJECT_NAME
COPY --from=builder /opt/$PROJECT_NAME/www /opt/$PROJECT_NAME/www

EXPOSE 9220

CMD ["/usr/sbin/cs"]

STOPSIGNAL SIGTERM
