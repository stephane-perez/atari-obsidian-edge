#!/bin/sh
# get-cross-mint.sh [debs-folder]
# Installs the m68k-atari-mint cross toolchain (GCC 4.6.4, binutils, MiNTlib, PML,
# mintbin, GEMlib) on Debian/Ubuntu.
# Fallback for networks where Vincent Riviere's PPA is unreachable (the CI uses the
# PPA directly, see .github/workflows/build.yml). Adapted from
# https://github.com/stephane-perez/atari-st-claude-playbook (scripts/get-cross-mint.sh).
#   - with a folder holding the five .deb files of a previous build: installs them;
#   - otherwise: builds the .deb files from Vincent Riviere's packaging repository
#     https://github.com/vinriviere/cross-mint-ubuntu (the source of his PPA), using a
#     temporary C-only GCC ("stage 1") to break the GCC <-> MiNTlib dependency.
#     About 16 minutes on 4 cores. The .deb files are kept in .tools/cross-mint/debs/
#     (attach or copy them for the next session: then this script only runs dpkg -i).
# Environment: CMU_REV=<commit> to build a given revision of cross-mint-ubuntu.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=$ROOT/.tools/cross-mint
DEBS=${1:-$WORK/debs}
case "$DEBS" in /*) ;; *) DEBS="$(pwd)/$DEBS" ;; esac
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
JOBS=$(nproc 2>/dev/null || echo 2)
PKGS="binutils mintbin mintlib pml gcc gemlib"

check() {
  m68k-atari-mint-gcc --version | head -1
  t=$(mktemp -d)
  printf '#include <gem.h>\nint main(void){appl_init();appl_exit();return 0;}\n' > "$t/h.c"
  m68k-atari-mint-gcc -m68000 -O2 -o "$t/h.prg" "$t/h.c" -lgem
  ls -l "$t/h.prg"
  rm -rf "$t"
  echo "OK: m68k-atari-mint toolchain ready"
}

if command -v m68k-atari-mint-gcc >/dev/null 2>&1 && [ -f /usr/m68k-atari-mint/lib/libgem.a ]; then
  echo "m68k-atari-mint-gcc and GEMlib already installed"; check; exit 0
fi

have_debs() {
  for p in $PKGS; do ls "$DEBS"/$p-m68k-atari-mint_*.deb >/dev/null 2>&1 || return 1; done
}

if have_debs; then
  echo "installing the .deb files from $DEBS"
  # order matters only for dpkg's dependency checks: give them all at once
  $SUDO dpkg -i "$DEBS"/*-m68k-atari-mint_*.deb
  check; exit 0
fi

echo "building the .deb files from github.com/vinriviere/cross-mint-ubuntu into $WORK"
$SUDO apt-get update
$SUDO apt-get install -y --no-install-recommends devscripts debhelper fakeroot texinfo \
  bison flex libfl-dev gettext autoconf bzip2 xz-utils libgmp-dev libmpfr-dev libmpc-dev \
  g++ make git patch rsync
mkdir -p "$WORK" "$DEBS"
cd "$WORK"
rm -rf cmu stage1
git clone -q https://github.com/vinriviere/cross-mint-ubuntu cmu
[ -n "$CMU_REV" ] && git -C cmu checkout -q "$CMU_REV"
echo "cross-mint-ubuntu at $(git -C cmu log -1 --format='%h %ci')"
CMU=$WORK/cmu

pkg() {  # name, extra PATH (optional): build one package, keep its .deb
  echo "== $1"
  (cd "$CMU/$1-m68k-atari-mint" && PATH="${2:+$2:}$PATH" \
     dpkg-buildpackage -b -us -uc -d -j"$JOBS" > "$WORK/$1.log" 2>&1) \
    || { echo "FAILED: see $WORK/$1.log"; tail -20 "$WORK/$1.log"; exit 1; }
  cp "$CMU"/$1-m68k-atari-mint_*.deb "$DEBS/"
}

pkg binutils
$SUDO dpkg -i "$DEBS"/binutils-m68k-atari-mint_*.deb
pkg mintbin

# stage 1: C only, no headers, private prefix
S=$WORK/stage1
mkdir -p "$S/src" "$S/build"
echo "== stage 1 gcc"
tar xf "$CMU"/gcc-m68k-atari-mint/gcc-*.tar.xz -C "$S/src"
GSRC=$(ls -d "$S"/src/gcc-*)
xzcat "$CMU"/gcc-m68k-atari-mint/gcc-*.patch.xz | (cd "$GSRC" && patch -p1 -s)
(cd "$S/build" && "$GSRC/configure" --prefix="$S/install" --target=m68k-atari-mint \
   --disable-nls --enable-languages=c --without-headers --disable-libssp \
   --disable-threads --disable-shared > "$WORK/stage1.log" 2>&1 \
 && make -j"$JOBS" all-gcc all-target-libgcc >> "$WORK/stage1.log" 2>&1 \
 && make install-gcc install-target-libgcc >> "$WORK/stage1.log" 2>&1) \
  || { echo "FAILED: see $WORK/stage1.log"; tail -20 "$WORK/stage1.log"; exit 1; }
# without these links stage 1 calls the host 'as' and does not find the MiNTlib headers
mkdir -p "$S/install/m68k-atari-mint/bin"
for t in as ld ar nm ranlib objcopy objdump strip; do
  ln -sf /usr/bin/m68k-atari-mint-$t "$S/install/m68k-atari-mint/bin/$t"
done
ln -sfn /usr/m68k-atari-mint/include "$S/install/m68k-atari-mint/include"
ln -sfn /usr/m68k-atari-mint/lib "$S/install/m68k-atari-mint/lib"

pkg mintlib "$S/install/bin"
$SUDO dpkg -i "$DEBS"/mintlib-m68k-atari-mint_*.deb "$DEBS"/mintbin-m68k-atari-mint_*.deb
pkg pml "$S/install/bin"
$SUDO dpkg -i "$DEBS"/pml-m68k-atari-mint_*.deb
pkg gcc
$SUDO dpkg -i "$DEBS"/gcc-m68k-atari-mint_*.deb
pkg gemlib
$SUDO dpkg -i "$DEBS"/gemlib-m68k-atari-mint_*.deb

rm -rf "$S"
ls -l "$DEBS"
check
