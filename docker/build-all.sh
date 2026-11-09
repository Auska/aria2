#!/bin/sh
# build-all.sh
#
# Compiles a fully-static aria2c binary inside an Alpine Linux container,
# building every dependency from source with -O2 -flto, all tests and
# debug code disabled.
#
#   docker run --rm -v "$PWD:/src:ro" -v "$PWD/output:/output" \
#       -e ARCH=amd64 alpine:3.22 /src/docker/build-all.sh
#
# Output: /output/aria2-linux-${ARCH}${SUFFIX}
#
# Required environment variables:
#   ARCH          Output filename suffix that identifies the target CPU
#                 (e.g. amd64 or arm64)
# Optional environment variables:
#   SUFFIX        Extra suffix for the output file
#   CONFIGURE_OPTS Extra configure parameters for aria2

set -eux

: "${ARCH:?ARCH must be set (e.g. amd64 or arm64)}"
: "${SUFFIX:=}"
: "${CONFIGURE_OPTS:=}"

NPROC=$(nproc)
BASE_FLAGS="-O2 -flto -pipe"

# ---------------------------------------------------------------------------
# 0. Common curl options
# ---------------------------------------------------------------------------
CURL="curl -fsS --retry 10 --retry-delay 5 --connect-timeout 10"
CURL_DL="curl -fsSLO --retry 10 --retry-delay 5 --connect-timeout 10"

# ---------------------------------------------------------------------------
# 1. System packages (toolchain only, all libs built from source)
# ---------------------------------------------------------------------------
apk add --no-cache \
    autoconf \
    automake \
    build-base \
    cmake \
    curl \
    libtool \
    linux-headers \
    ninja \
    pkgconf \
    python3 \
    unzip

mkdir -p /build
cd /build
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"

# ===========================================================================
# 2. Fetch dependency sources (latest release of each)
# ===========================================================================

ZLIB_VERSION=$(${CURL} "https://api.github.com/repos/madler/zlib/releases/latest" | python3 -c 'import json,sys; print(json.load(sys.stdin)["tag_name"].lstrip("v"))')
OPENSSL_VERSION=$(${CURL} "https://api.github.com/repos/openssl/openssl/releases/latest" | python3 -c 'import json,sys; print(json.load(sys.stdin)["tag_name"].lstrip("openssl-").replace("_",".").replace("-","."))')
CARES_VERSION=$(${CURL} "https://api.github.com/repos/c-ares/c-ares/releases/latest" | python3 -c 'import json,sys; print(json.load(sys.stdin)["tag_name"].lstrip("v"))')
SQLITE_YEAR=$(date +%Y)
echo "zlib ${ZLIB_VERSION} / openssl ${OPENSSL_VERSION} / c-ares ${CARES_VERSION}"

# zlib
${CURL_DL} "https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VERSION}.tar.gz"
tar xf "v${ZLIB_VERSION}.tar.gz"

# OpenSSL: tag format is openssl-3.5.4; use tarball from GitHub
${CURL_DL} "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
tar xf "openssl-${OPENSSL_VERSION}.tar.gz"

# c-ares
${CURL_DL} "https://github.com/c-ares/c-ares/releases/download/v${CARES_VERSION}/c-ares-${CARES_VERSION}.tar.gz"
tar xf "c-ares-${CARES_VERSION}.tar.gz"

# sqlite (amalgamation, current year for source URL)
SQLITE_VERSION=$(${CURL} "https://api.github.com/repos/sqlite/sqlite/tags" | python3 -c 'import json,sys; print(json.load(sys.stdin)[0]["name"].lstrip("version-"))')
SQLITE_URL="https://www.sqlite.org/${SQLITE_YEAR}/sqlite-amalgamation-${SQLITE_VERSION//./_}.zip"
${CURL_DL} "${SQLITE_URL}"
unzip -q "sqlite-amalgamation-${SQLITE_VERSION//./_}.zip"
SQLITE_DIR=$(unzip -l "sqlite-amalgamation-${SQLITE_VERSION//./_}.zip" | head -4 | tail -1 | awk '{print $4}' | cut -d/ -f1)

# ===========================================================================
# 3. Build dependencies: static, -O2 -flto, no tests, no debug
# ===========================================================================

# ---------------------------------------------------------------------------
# 3.1 zlib (with AVX2/SSSE3 disabled for baseline portability)
# ---------------------------------------------------------------------------
echo "Building zlib ${ZLIB_VERSION}"
cd /build/zlib-${ZLIB_VERSION}

cmake -B build -G Ninja \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_OPT=ON \
    -DSSSE3_FOUND=OFF \
    -DWITH_GZFILEOP=ON \
    -DCMAKE_C_FLAGS="${BASE_FLAGS}" \
    -DCMAKE_EXE_LINKER_FLAGS="-static"
cmake --build build -j"${NPROC}"
cmake --install build

rm -f /usr/lib/pkgconfig/zlib.pc 2>/dev/null || true

# ---------------------------------------------------------------------------
# 3.2 OpenSSL (LibreSSL-free; no apps, no tests, no docs)
# ---------------------------------------------------------------------------
echo "Building OpenSSL ${OPENSSL_VERSION}"
cd /build/openssl-${OPENSSL_VERSION}

./Configure \
    linux-x86_64 \
    --prefix=/usr/local \
    --libdir=lib \
    no-shared \
    no-tests \
    no-apps \
    no-docs \
    no-asm-dbg \
    no-legacy \
    no-comp \
    no-idea no-mdc2 no-rc5 no-ec2m no-sm2 no-sm3 no-sm4 no-dso \
    ${BASE_FLAGS}

make -j"${NPROC}"
make install_sw

# ---------------------------------------------------------------------------
# 3.3 c-ares
# ---------------------------------------------------------------------------
echo "Building c-ares ${CARES_VERSION}"
cd /build/c-ares-${CARES_VERSION}

cmake -B build -G Ninja \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -DCARES_STATIC=ON \
    -DCARES_SHARED=OFF \
    -DCARES_BUILD_TESTS=OFF \
    -DCARES_BUILD_TOOLS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="${BASE_FLAGS}"
cmake --build build -j"${NPROC}"
cmake --install build

# ---------------------------------------------------------------------------
# 3.4 sqlite3 (amalgamation, hand-rolled: no project-level cmake)
# ---------------------------------------------------------------------------
echo "Building sqlite3 ${SQLITE_VERSION}"
cd /build/${SQLITE_DIR}

${CC:-cc} ${BASE_FLAGS} -static \
    -DSQLITE_ENABLE_FTS5 \
    -DSQLITE_ENABLE_RTREE \
    -DSQLITE_ENABLE_COLUMN_METADATA \
    -DSQLITE_ENABLE_JSON1 \
    -DSQLITE_ENABLE_MATH_FUNCTIONS \
    -DSQLITE_THREADSAFE=1 \
    -DNDEBUG \
    -c sqlite3.c
ar rcs libsqlite3.a sqlite3.o
install -Dm644 sqlite3.h /usr/local/include/sqlite3.h
install -Dm644 libsqlite3.a /usr/local/lib/libsqlite3.a
cat > /usr/local/lib/pkgconfig/sqlite3.pc <<EOF
prefix=/usr/local
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: sqlite3
Description: SQL database engine
Version: ${SQLITE_VERSION}
Libs: -L\${libdir} -lsqlite3
Cflags: -I\${includedir}
EOF

# ===========================================================================
# 4. Build aria2: static, -O2 -flto, werror, no tests
# ===========================================================================
echo "Building aria2"

# Copy to a writable dir: autoreconf needs to write into the source tree.
cp -r /src /build/aria2
cd /build/aria2

autoreconf -i
./configure \
    ${CONFIGURE_OPTS} \
    ARIA2_STATIC=yes \
    --enable-werror \
    --without-jemalloc \
    CFLAGS="${BASE_FLAGS}" \
    CXXFLAGS="${BASE_FLAGS}" \
    LDFLAGS="-static -flto"

# Build deps (wslay) then src only: skipping doc/ and the cppunit test/ dir.
make -j"${NPROC}" -C deps
make -j"${NPROC}" -C src

# ---------------------------------------------------------------------------
# 5. Copy and verify the output binary
# ---------------------------------------------------------------------------
OUTPUT="/output/aria2-linux-${ARCH}${SUFFIX}"

cp src/aria2c "${OUTPUT}"
strip "${OUTPUT}"

echo "=== Build complete ==="
file "${OUTPUT}"
ls -lh "${OUTPUT}"
