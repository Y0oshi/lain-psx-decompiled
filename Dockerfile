# Reproducible build environment for lain-psx-decompiled.
# Build/run as linux/amd64 (tools/docker.sh does this): the PsyQ-era GCC builds
# only exist as x86-64 Linux binaries.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mips-linux-gnu make git ca-certificates curl \
        python3 python3-venv python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Candidate original compilers (cc1 builds matching PsyQ's CCPSX), from decompals/old-gcc.
ARG OLD_GCC=https://github.com/decompals/old-gcc/releases/download/0.17
RUN for v in 2.6.3-psx 2.7.2-psx 2.7.2-cdk 2.8.0-psx 2.8.1-psx 2.91.66-psx; do \
        mkdir -p /opt/gcc/$v && curl -sL $OLD_GCC/gcc-$v.tar.gz | tar xz -C /opt/gcc/$v; \
    done

# maspsx: reproduces PsyQ ASPSX quirks (load delay nops, $at usage, etc.) on GNU as.
ARG MASPSX_COMMIT=7686f845a181700534c83c0419183e38aeb3e49c
RUN git clone -q https://github.com/mkst/maspsx /opt/maspsx \
    && git -C /opt/maspsx checkout -q $MASPSX_COMMIT

COPY requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/venv && /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements.txt
ENV PATH=/opt/venv/bin:$PATH

# m2c (MIPS -> C first-pass decompiler). Not the unrelated "m2c" package on PyPI.
ARG M2C_COMMIT=708d2d2cb2698f091a92492b328f73b24209f72d
RUN git clone -q https://github.com/matt-kempster/m2c /opt/m2c \
    && git -C /opt/m2c checkout -q $M2C_COMMIT \
    && /opt/venv/bin/pip install --no-cache-dir pycparser graphviz \
    && printf '#!/bin/sh\nexec python3 /opt/m2c/m2c.py "$@"\n' > /usr/local/bin/m2c && chmod +x /usr/local/bin/m2c

# decomp-permuter (random C permutations scored against the target); see tools/permute.sh.
# Its plain `cpp` call on the already-preprocessed base.c goes to PsyQ's cpp.
ARG PERMUTER_COMMIT=059609d4aec73eb0650726772954e1ad575825f8
RUN git clone -q https://github.com/simonlindholm/decomp-permuter /opt/permuter \
    && git -C /opt/permuter checkout -q $PERMUTER_COMMIT \
    && /opt/venv/bin/pip install --no-cache-dir toml Levenshtein \
    && ln -s /opt/gcc/2.8.1-psx/cpp /usr/local/bin/cpp

WORKDIR /lain
