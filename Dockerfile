FROM debian:bookworm-slim

LABEL maintainer="Stephane Perez"
LABEL description="Build environment for cross-compilation m68k-atari-mint"

# Variables d'environnement
ENV DEBIAN_FRONTEND=noninteractive \
    ROOT=/workspace/atari-tools \
    TOOLS=/workspace/.tools

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
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash builder && \
    mkdir -p ${ROOT} ${TOOLS} && \
    chown builder:builder ${ROOT} ${TOOLS}

WORKDIR ${ROOT}
COPY scripts/get-cross-mint.sh scripts/get-libcmini.sh ./

RUN chmod +x get-cross-mint.sh get-libcmini.sh

RUN ./get-cross-mint.sh ${TOOLS}/cross-mint/debs && \
    ./get-libcmini.sh ${TOOLS}/libcmini

RUN m68k-atari-mint-gcc --version && \
    ls -l /usr/m68k-atari-mint/lib/libgem.a

USER builder

CMD ["make", "-j$(nproc)"]
