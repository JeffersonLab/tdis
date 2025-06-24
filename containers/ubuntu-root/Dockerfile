# Main build command - minimal and self-contained
  #BUILDX_GIT_LABELS=full docker buildx build \
  #    --platform linux/amd64,linux/arm64 \
  #    --tag eicdev/ubuntu-root:latest \
  #    --push \
  #    .
# One line:
# BUILDX_GIT_LABELS=full docker buildx build --tag eicdev/ubuntu-root:latest --push .

FROM ubuntu:24.04

# OCI Image Labels - most automatically populated by buildx via BUILDX_GIT_LABELS
LABEL org.opencontainers.image.authors="Dmitry Romanov <romanov@jlab.org>" \
      org.opencontainers.image.url="https://github.com/DarTeots/edpm" \
      org.opencontainers.image.documentation="https://github.com/DarTeots/edpm/blob/main/README.md" \
      org.opencontainers.image.vendor="Dmitry Romanov" \
      org.opencontainers.image.licenses="MIT" \
      org.opencontainers.image.title="EIC Ubuntu Development Environment with ROOT" \
      org.opencontainers.image.description="Ubuntu-based development container with CERN ROOT, scientific computing libraries, and development tools for EIC software development"
# - org.opencontainers.image.source (git repository URL)
# - org.opencontainers.image.revision (git commit hash)
# - com.docker.image.source.entrypoint (Dockerfile location)

# Legacy labels for compatibility
LABEL maintainer.name="Dmitry Romanov" \
      maintainer.email="romanov@jlab.org"

ARG DEBIAN_FRONTEND=noninteractive
ENV APP_ROOT=/app

## With heredocs for multi-line scripts, we want to fail on error and the print failing line.
## Ref: https://docs.docker.com/engine/reference/builder/#example-running-a-multi-line-script
SHELL ["bash", "-ex", "-c"]



# Install OS dependencies
RUN apt-get update &&\
    apt-get install -y python3-dev python3-pip python3-jinja2 python3-yaml python3-numpy  dos2unix \
                       sudo git cmake gdb ninja-build apt-utils bash ccache clang valgrind gfortran g++ gcc curl clang-tools-18 clang-tidy-18 \
                       dialog apt-utils dpkg-dev tzdata binutils unminimize libfmt-dev locales\
                       libftgl-dev libxml2-dev libldap2-dev  \
                       libfftw3-dev libmysqlclient-dev libavahi-compat-libdnssd-dev  \
                       libcfitsio-dev libglu1-mesa-dev libkrb5-dev libpcre3-dev libglew-dev  \
                       libgraphviz-dev \
                       libgsl-dev qtwebengine5-dev nlohmann-json3-dev liblz4-dev \
                       libgl2ps-dev liblzma-dev libxxhash-dev libzstd-dev \
                       libx11-dev libxft-dev libxpm-dev libxext-dev &&\
    apt-get clean && apt-get autoremove -y && rm -rf /var/lib/apt/lists/*


RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    dpkg-reconfigure --frontend=noninteractive locales


ENV CLICOLOR_FORCE=1      \
    LANGUAGE=en_US.UTF-8  \
    LANG=en_US.UTF-8      \
    LC_ALL=en_US.UTF-8

# Install EDPM v3 and set the top_dir
RUN python3 -m pip install --user --upgrade --force-reinstall --break-system-packages ruamel.yaml click appdirs edpm

# Set clang as default
RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-18 60

WORKDIR ${APP_ROOT}
ENV PATH="$PATH:/root/.local/bin"



# ---------------------------------------------------------------------------#
#  Build-time arguments & environment                                        #
# ---------------------------------------------------------------------------#
ARG BUILD_THREADS=8
ARG CXX_STANDARD=20
ARG VERSION_CERN_ROOT=v6-36-00

ENV CERN_ROOT_SRC_PATH=${APP_ROOT}/root-src \
    CERN_ROOT_BLD_PATH=${APP_ROOT}/root-build \
    CERN_ROOT_INS_PATH=${APP_ROOT}/root

### ---------------------------------------------------------------------- ###
###          ROOT build (no EDPM)                                          ###
### ---------------------------------------------------------------------- ###
RUN set -ex && \
    # 1. Clone CERN ROOT (shallow, specific tag / branch)
    git clone --branch ${VERSION_CERN_ROOT} --depth 1 https://github.com/root-project/root.git "${CERN_ROOT_SRC_PATH}" && \
    \
    # 2. Configure with CMake in out-of-source build dir
    mkdir -p "${CERN_ROOT_BLD_PATH}" && cd "${CERN_ROOT_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${CERN_ROOT_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
      -Droot7=ON \
      -Dgdml=ON \
      -Dxrootd=OFF \
      -Dmysql=OFF \
      -Dpythia6=OFF \
      -Dpythia6_nolink=OFF \
      -Dpythia8=OFF \
      -Dhttp=ON \
      -Wno-dev \
      "${CERN_ROOT_SRC_PATH}" && \
    \
    # 3. Build & install
    cmake --build . --target install -j"${BUILD_THREADS}" && \
    \
    # 4. Clean up build tree to slim the image
    rm -rf "${CERN_ROOT_BLD_PATH}" "${CERN_ROOT_SRC_PATH}"

# Export classic ROOT vars. Why like this? This is the most stable way to make environment variables work in any case
ENV ROOTSYS=${CERN_ROOT_INS_PATH} \
    PATH="${CERN_ROOT_INS_PATH}/bin:${PATH}" \
    LD_LIBRARY_PATH="${CERN_ROOT_INS_PATH}/lib:${LD_LIBRARY_PATH}" \
    DYLD_LIBRARY_PATH="${CERN_ROOT_INS_PATH}/lib" \
    SHLIB_PATH="${CERN_ROOT_INS_PATH}/lib" \
    LIBPATH="${CERN_ROOT_INS_PATH}/lib" \
    PYTHONPATH="${CERN_ROOT_INS_PATH}/lib" \
    CMAKE_PREFIX_PATH="${CERN_ROOT_INS_PATH}" \
    JUPYTER_PATH="${CERN_ROOT_INS_PATH}/etc/notebook" \
    JUPYTER_CONFIG_DIR="${CERN_ROOT_INS_PATH}/etc/notebook" \
    CLING_STANDARD_PCH=none


ENV ROOTSYS=${APP_ROOT}/root

ENV PATH="$ROOTSYS/bin:$PATH" \
    LD_LIBRARY_PATH="$ROOTSYS/lib:${LD_LIBRARY_PATH}" \
    DYLD_LIBRARY_PATH="$ROOTSYS/lib:${DYLD_LIBRARY_PATH}" \
    SHLIB_PATH="$ROOTSYS/lib:${SHLIB_PATH}" \
    LIBPATH="$ROOTSYS/lib:${LIBPATH}" \
    PYTHONPATH="$ROOTSYS/lib:${PYTHONPATH}" \
    CMAKE_PREFIX_PATH="$ROOTSYS:${CMAKE_PREFIX_PATH}" \
    JUPYTER_PATH="$ROOTSYS/etc/notebook:${JUPYTER_PATH}" \
    JUPYTER_CONFIG_DIR="$ROOTSYS/etc/notebook:${JUPYTER_CONFIG_DIR}" \
    CLING_STANDARD_PCH=none