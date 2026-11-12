FROM debian:bookworm-slim

LABEL maintainer="Stephane Perez"
LABEL description="Build environment for cross-compilation m68k-atari-mint"

# Variables d'environnement
# LIBCMINI: where the Makefile finds libcmini (default: .tools/libcmini in the
# repository, which is not where this image installs it)
ENV DEBIAN_FRONTEND=noninteractive \
    ROOT=/workspace/atari-tools \
    TOOLS=/workspace/.tools \
    LIBCMINI=/workspace/.tools/libcmini

# Paquets système requis
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      git \
      ca-certificates \
      wget \
      build-essential \
      bison \
      flex \
      libfl-dev \
      gettext \
      autoconf \
      bzip2 \
      xz-utils \
      libgmp-dev \
      libmpfr-dev \
      libmpc-dev \
      g++ \
      make \
      patch \
      rsync \
      devscripts \
      debhelper \
      fakeroot \
      texinfo \
      zip \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash builder && \
    mkdir -p ${ROOT} ${TOOLS} && \
    chown builder:builder ${ROOT} ${TOOLS}

WORKDIR ${ROOT}
COPY scripts/get-cross-mint.sh scripts/get-libcmini.sh ./

RUN chmod +x get-cross-mint.sh get-libcmini.sh

# the build trees (GCC, binutils, libcmini sources) are removed in the same
# layer: about 3 GB that the image does not need
RUN ./get-cross-mint.sh ${TOOLS}/cross-mint/debs && \
    ./get-libcmini.sh ${TOOLS}/libcmini && \
    rm -rf ${TOOLS}/cross-mint/cmu ${TOOLS}/cross-mint/*.log ${TOOLS}/libcmini-src

RUN m68k-atari-mint-gcc --version && \
    ls -l /usr/m68k-atari-mint/lib/libgem.a

USER builder

# Usage, from the repository:
#   docker build -t obsidian-edge-build .
#   docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/workspace/atari-tools obsidian-edge-build
#   docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/workspace/atari-tools obsidian-edge-build make zip
# (--user: the files written in build/ belong to you, not to "builder")
#
# a shell is needed to expand $(nproc): the exec form passes it to make as is
CMD ["sh", "-c", "make -j\"$(nproc)\""]
