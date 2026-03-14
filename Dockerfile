FROM debian:bookworm-slim AS builder

ARG PROJECT_NAME=cs
ARG BUILD_MODE=release
ARG WITH_POSTGRESQL=OFF

LABEL project="${PROJECT_NAME}"

RUN set -eux; \
    apt-get update && apt-get install -y --no-install-recommends \
        bash build-essential cmake g++ \
        libssl-dev zlib1g-dev \
        pkg-config ca-certificates git curl lsb-release && \
    rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    if [ "${WITH_POSTGRESQL}" = "ON" ]; then \
        install -d /usr/share/postgresql-common/pgdg && \
        curl -o /usr/share/postgresql-common/pgdg/apt.postgresql.org.asc --fail \
            https://www.postgresql.org/media/keys/ACCC4CF8.asc && \
        sh -c 'echo "deb [signed-by=/usr/share/postgresql-common/pgdg/apt.postgresql.org.asc] https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list' && \
        apt-get update && apt-get install -y --no-install-recommends libpq-dev && \
        rm -rf /var/lib/apt/lists/*; \
    fi

WORKDIR /opt/${PROJECT_NAME}
COPY . .

RUN set -eux; \
    cp -r docker/conf . && \
    cp -r docker/www . && \
    PG_FLAG="--without-postgresql"; \
    if [ "${WITH_POSTGRESQL}" = "ON" ]; then PG_FLAG="--with-postgresql"; fi; \
    ./configure --${BUILD_MODE} ${PG_FLAG}; \
    cmake --build cmake-build-${BUILD_MODE} --parallel $(nproc); \
    cmake --install cmake-build-${BUILD_MODE}; \
    rm -rf cmake-build-${BUILD_MODE} src

# ─────────────────────────────────────────────────────────────────────────────

FROM debian:bookworm-slim AS app

ARG PROJECT_NAME=cs

ENV PROJECT_NAME="${PROJECT_NAME}"

RUN set -eux; \
    apt-get update && apt-get install -y --no-install-recommends \
        bash ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /var/log/${PROJECT_NAME}

COPY --from=builder /usr/sbin/${PROJECT_NAME}    /usr/sbin/${PROJECT_NAME}
COPY --from=builder /etc/${PROJECT_NAME}         /etc/${PROJECT_NAME}
COPY --from=builder /opt/${PROJECT_NAME}/www     /opt/${PROJECT_NAME}/www

EXPOSE 9220

CMD ["/usr/sbin/cs"]

STOPSIGNAL SIGTERM
