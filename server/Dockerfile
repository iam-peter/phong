# phong-server in a container, built with the Qt of Debian. It listens on
# $PORT, 45460 by default, and keeps the high scores in /data.
#   docker build -t phong-server server
#   docker run -p 45460:45460 -v phong-data:/data phong-server
FROM debian:bookworm-slim AS build
RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake g++ make qt6-base-dev qt6-websockets-dev \
    && rm -rf /var/lib/apt/lists/*
COPY . /src
RUN cmake -S /src -B /build -DCMAKE_BUILD_TYPE=Release && cmake --build /build --parallel

FROM debian:bookworm-slim
RUN apt-get update \
    && apt-get install -y --no-install-recommends libqt6core6 libqt6network6 libqt6websockets6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --home-dir /data phong \
    && mkdir /data && chown phong /data
COPY --from=build /build/phong-server /usr/local/bin/phong-server
USER phong
EXPOSE 45460
CMD ["phong-server", "--scores", "/data/scores.json"]
