FROM ubuntu:24.04

ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=1000

ENV DEBIAN_FRONTEND=noninteractive

# Toolchains (GCC 13 + Clang 18), build tools, and the system libraries needed by the adapters:
# Qt 6 (QtCore / QtNetwork / QSslSocket) and OpenSSL (asio::ssl).
# Header-only or source dependencies (asio, Boost, Capy, Boost.Http, llhttp, Catch2) are fetched by CPM at configure time.
RUN apt-get update && apt-get install -y --no-install-recommends \
      g++-13 \
      clang-18 clang-format-18 clang-tidy-18 clangd-18 lld-18 \
      cmake ninja-build ccache gdb \
      git ca-certificates curl sudo \
      qt6-base-dev libssl-dev zlib1g-dev pkg-config \
      python3 \
 && rm -rf /var/lib/apt/lists/* \
 && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 \
 && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100 \
 && update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-18 100

# Ubuntu images ship an "ubuntu" user with uid 1000: replace it so files written in the bind-mounted
# workspace keep the host user's ownership.
RUN userdel -r ubuntu 2>/dev/null || true \
 && groupadd --gid ${USER_GID} ${USERNAME} \
 && useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME} \
 && echo "${USERNAME} ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
 && chmod 0440 /etc/sudoers.d/${USERNAME}

ENV CC=gcc-13 \
    CXX=g++-13 \
    CMAKE_GENERATOR=Ninja \
    CMAKE_C_COMPILER_LAUNCHER=ccache \
    CMAKE_CXX_COMPILER_LAUNCHER=ccache \
    CCACHE_DIR=/home/${USERNAME}/.cache/ccache \
    CPM_SOURCE_CACHE=/home/${USERNAME}/.cache/CPM

USER ${USERNAME}
