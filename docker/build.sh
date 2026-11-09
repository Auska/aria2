#!/bin/sh
# build.sh
#
# Compiles a fully-static aria2c binary inside an Alpine Linux container.
#
#   docker run --rm -v "$PWD:/out" -v "$PWD/docker:/src:ro" alpine:3.22 /src/build.sh
#
# Output: /out/aria2-linux-amd64${SUFFIX}
#
# Required environment variables:
#   ARCH          Output filename suffix that identifies the target CPU
#                 (e.g. amd64 or arm64)
# Optional environment variables:
#   SUFFIX        Extra suffix for the output file
#   CONFIGURE_OPTS Extra configure parameters for aria2
#
# Static libraries come from Alpine's official -static packages
# (openssl-libs-static, zlib-static, sqlite-static, libuv-static).
# c-ares has no -static package in Alpine, so it is built from source.

set -eux

: "${ARCH:?ARCH must be set (e.g. amd64 or arm64)}"
: "${SUFFIX:=}"
: "${CONFIGURE_OPTS:=}"

# ---------------------------------------------------------------------------
# 0. Common curl options
# ---------------------------------------------------------------------------
CURL="curl -fsS --retry 10 --retry-delay 5 --connect-timeout 10"
CURL_DL="curl -fsSLO --retry 10 --retry-delay 5 --connect-timeout 10"

# ---------------------------------------------------------------------------
# 1. System packages
# ---------------------------------------------------------------------------
apk add --no-cache \
    build-base \
    cmake \
    curl \
    linux-headers \
    ninja \
    pkgconf \
    python3 \
    openssl-libs-static \
    openssl-dev \
    zlib-static \
    zlib-dev \
    sqlite-static \
    sqlite-dev \
    libuv-static \
    libuv-dev

export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"

# ---------------------------------------------------------------------------
# 2. Build c-ares from source (no -static package in Alpine)
# ---------------------------------------------------------------------------
CARES_VERSION=$(${CURL} "https://api.github.com/repos/c-ares/c-ares/releases/latest" | python3 -c 'import json,sys; print(json.load(sys.stdin)["tag_name"].lstrip("v"))')
echo "c-ares version: ${CARES_VERSION}"

mkdir -p /build
cd /build

${CURL_DL} "https://github.com/c-ares/c-ares/releases/download/v${CARES_VERSION}/c-ares-${CARES_VERSION}.tar.gz"
tar xf "c-ares-${CARES_VERSION}.tar.gz"

echo "Building c-ares ${CARES_VERSION}"
cd /build/c-ares-${CARES_VERSION}

cmake -B build \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -DCARES_STATIC=ON \
    -DCARES_SHARED=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_LIBDIR=lib

cmake --build build -j"$(nproc)"
cmake --install build

# c-ares only installs man pages and headers to /usr/local; the aria2
# autotools build below only needs the static lib via pkg-config.

# ---------------------------------------------------------------------------
# 3. Build aria2 (source tree is mounted at /src)
# ---------------------------------------------------------------------------
echo "Building aria2"

# Copy to a writable dir: autoreconf needs to write into the source tree.
cp -r /src /build/aria2
cd /build/aria2

autoreconf -i
./configure \
    ${CONFIGURE_OPTS} \
    ARIA2_STATIC=yes \
    --enable-werror \
    --without-jemalloc

make -j"$(nproc)"

# Skip the aria2 test suite: the bundled cppunit-based tests build a
# dynamically-linked test binary and are not relevant for static output.

# ---------------------------------------------------------------------------
# 4. Copy and verify the output binary
# ---------------------------------------------------------------------------
OUTPUT="/output/aria2-linux-${ARCH}${SUFFIX}"

cp src/aria2c "${OUTPUT}"
strip "${OUTPUT}"

echo "=== Build complete ==="
file "${OUTPUT}"
ls -lh "${OUTPUT}"
