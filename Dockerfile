# syntax=docker/dockerfile:1.7

FROM debian:trixie-slim AS build

ARG TARGETARCH

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        curl \
        file \
        git \
        procps \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /bin/bash linuxbrew

USER linuxbrew

ENV HOMEBREW_NO_ANALYTICS=1 \
    HOMEBREW_NO_AUTO_UPDATE=1 \
    HOMEBREW_NO_ENV_HINTS=1 \
    PATH=/home/linuxbrew/.linuxbrew/opt/llvm/bin:/home/linuxbrew/.linuxbrew/bin:/home/linuxbrew/.linuxbrew/sbin:$PATH

RUN case "$TARGETARCH:$(dpkg --print-architecture)" in \
        amd64:amd64|arm64:arm64) ;; \
        *) echo "unsupported target architecture: $TARGETARCH" >&2; exit 1 ;; \
    esac \
    && curl --proto '=https' --tlsv1.2 -fsSLo /tmp/homebrew-install.sh \
        https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh \
    && NONINTERACTIVE=1 CI=1 /bin/bash /tmp/homebrew-install.sh \
    && rm /tmp/homebrew-install.sh

RUN brew install llvm libuv \
    && test -x "$(brew --prefix llvm)/bin/clang" \
    && test -f "$(brew --prefix libuv)/lib/libuv.a"

WORKDIR /src
COPY --chown=linuxbrew:linuxbrew noroi.c noroi.h ./

RUN set -eux; \
    llvm_prefix="$(brew --prefix llvm)"; \
    libuv_prefix="$(brew --prefix libuv)"; \
    mkdir -p /home/linuxbrew/out; \
    "$llvm_prefix/bin/clang" \
        -std=c23 \
        -O3 \
        -DNDEBUG \
        -D_POSIX_C_SOURCE=200809L \
        -fPIC \
        -pthread \
        -Wall \
        -Wextra \
        -I"$libuv_prefix/include" \
        -c noroi.c \
        -o /tmp/noroi.o; \
    printf 'CREATE %s\nADDMOD %s\nADDLIB %s\nSAVE\nEND\n' \
        /home/linuxbrew/out/libnoroi.a \
        /tmp/noroi.o \
        "$libuv_prefix/lib/libuv.a" \
        | "$llvm_prefix/bin/llvm-ar" -M; \
    "$llvm_prefix/bin/llvm-ranlib" /home/linuxbrew/out/libnoroi.a; \
    install -m 0644 noroi.h /home/linuxbrew/out/noroi.h; \
    "$llvm_prefix/bin/llvm-ar" t /home/linuxbrew/out/libnoroi.a \
        | grep -qx noroi.o; \
    "$llvm_prefix/bin/llvm-nm" --defined-only /home/linuxbrew/out/libnoroi.a \
        | grep -Eq '[[:space:]]noroi_run$'; \
    "$llvm_prefix/bin/llvm-nm" --defined-only /home/linuxbrew/out/libnoroi.a \
        | grep -Eq '[[:space:]]uv_run$'

FROM debian:trixie-slim

WORKDIR /app
COPY --from=build --chown=root:root /home/linuxbrew/out/ ./
