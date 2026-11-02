FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake libpq-dev libssl-dev ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build --parallel 2

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpq5 libssl3 ca-certificates && rm -rf /var/lib/apt/lists/*
COPY --from=build /app/build/gommanna_server /usr/local/bin/gommanna_server
WORKDIR /app
USER nobody
ENV HOST=0.0.0.0 PORT=8080
EXPOSE 8080
CMD ["gommanna_server"]
