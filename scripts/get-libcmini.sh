#!/bin/sh
# get-libcmini.sh [prefix]
# Fetches libcmini (small C library for TOS, https://github.com/freemint/libcmini)
# at a pinned commit, builds the 68000 version and installs it into prefix
# (default: .tools/libcmini), which is where the Makefile looks for it.
# Requires the m68k-atari-mint cross toolchain.
set -e
LIBCMINI_REPO=https://github.com/freemint/libcmini
LIBCMINI_COMMIT=${LIBCMINI_COMMIT:-77eb5e1e0129c5dad932e08e50d893cf792a5347}	# 2026-09-06

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PREFIX=${1:-$ROOT/.tools/libcmini}
case "$PREFIX" in /*) ;; *) PREFIX="$(pwd)/$PREFIX" ;; esac
SRC=$ROOT/.tools/libcmini-src

if [ -f "$PREFIX/lib/libcmini.a" ] && [ "$(cat "$PREFIX/COMMIT" 2>/dev/null)" = "$LIBCMINI_COMMIT" ]; then
  echo "libcmini $LIBCMINI_COMMIT already installed in $PREFIX"
  exit 0
fi

rm -rf "$SRC" "$PREFIX"
git init -q "$SRC"
git -C "$SRC" fetch -q --depth 1 "$LIBCMINI_REPO" "$LIBCMINI_COMMIT"
git -C "$SRC" checkout -q FETCH_HEAD
echo "libcmini at $(git -C "$SRC" log -1 --format='%h %ci %s')"

make -C "$SRC" -j"$(nproc 2>/dev/null || echo 2)" ONLY_68K=Y
make -C "$SRC" ONLY_68K=Y install PREFIX="$PREFIX" > /dev/null
echo "$LIBCMINI_COMMIT" > "$PREFIX/COMMIT"
ls -l "$PREFIX/lib"
