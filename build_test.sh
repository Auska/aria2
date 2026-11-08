#!/bin/sh

#exec 2>&1

BUILDDIR=/tmp/aria2buildtest

if [ ! -d "$BUILDDIR" ]; then
    mkdir "$BUILDDIR" \
	|| { echo "Failed to create directory $BUILDDIR" && exit 1; }
fi

echo -n "Starting build test "
echo "$(date)"

# build CONFIGURE_OPTS BIN_SUFFIX DESC
build()
{
    echo -n "new build() started at "
    echo "$(date)"
    echo "*** configure opts=$1"
    BIN_NAME="aria2c_$2"
    if [ -f "$BUILDDIR/$BIN_NAME" ]; then
	echo "$BIN_NAME exists, skipping"
	return
    fi
    ./configure $1 2>&1 | tee "$BUILDDIR/configure_$2.log" \
	&& cp config.log "$BUILDDIR/config.log_$2" \
	&& LANG=C make clean \
	&& LANG=C make -j2 check 2>&1 | tee "$BUILDDIR/aria2c_$2.log" \
	&& cp src/aria2c "$BUILDDIR/aria2c_$2"

    if [ -f "test/aria2c.log" ]; then
	cat "test/aria2c.log" >> "$BUILDDIR/aria2c_$2.log"
    fi
}

clear()
{
    for file in $(ls "$BUILDDIR"); do
	rm -f "$BUILDDIR/$file"
    done
}

case "$1" in
    clear)
	clear
	;;
    *)
	# Library combinations
	build "--without-openssl" "internal"
	build "--disable-ssl" "nossl"
	build "--without-libcares" "nocares"
	build "--without-libz" "nozlib"
	build "--without-sqlite3" "nosqlite3"
	# Feature combinations
	build "--disable-bittorrent" "nobt"
	build "--enable-libaria2" "libaria2"
	;;
esac
