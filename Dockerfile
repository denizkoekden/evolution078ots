# syntax=docker/dockerfile:1
###############################################################################
# Dockerized OpenTibia Server "Evolution 0.7.8" (modernized)
# Default storage backend is MySQL (for the docker-compose stack); override with
#   docker build --build-arg STORAGE=sqlite .
###############################################################################

# ---------------------------------------------------------------- build stage
FROM ubuntu:24.04 AS build
ARG STORAGE=mysql
RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake ninja-build g++ ca-certificates \
        libxml2-dev libboost-regex-dev libgmp-dev libsqlite3-dev \
        default-libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSTORAGE=${STORAGE} \
    && cmake --build build --parallel \
    && cmake --install build --prefix /opt/evolution

# -------------------------------------------------------------- runtime stage
FROM ubuntu:24.04 AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
        libxml2 libgmp10 libboost-regex1.83.0 libmysqlclient21 libsqlite3-0 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --home-dir /opt/evolution --shell /usr/sbin/nologin otserv

COPY --from=build --chown=otserv:otserv /opt/evolution /opt/evolution
COPY --chown=otserv:otserv docker/entrypoint.sh /entrypoint.sh
COPY --chown=otserv:otserv docker/healthcheck.sh /healthcheck.sh
RUN chmod +x /entrypoint.sh /healthcheck.sh /opt/evolution/evolutions

WORKDIR /opt/evolution
USER otserv
EXPOSE 7171
# Health = the server answers the OpenTibia status query, not just an open port.
HEALTHCHECK --interval=15s --timeout=5s --start-period=45s --retries=3 CMD /healthcheck.sh
ENTRYPOINT ["/entrypoint.sh"]
