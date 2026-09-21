FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    make \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    ca-certificates \
    gcc python3 gcovr \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
