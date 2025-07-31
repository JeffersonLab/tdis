# BUILDX_EXPERIMENTAL=1 docker buildx debug --invoke bash build --progress=plain
# in PShell instead of  BUILDX_EXPERIMENTAL=1 use:
#   $env:BUILDX_EXPERIMENTAL="1"
# Just build using buildx
#   docker buildx build --build-arg BUILD_THREADS=24 --tag eicdev/tdis-pre:latest --push .
#   docker build -t eicdev/tdis-pre:latest .
#   docker build -t eicdev/tdis-pre:latest --build-arg BUILD_THREADS=24 .
#    docker push eicdev/tdis-pre:latest
#
# docker run --rm -it --init eicdev/tdis-pre:latest
# docker run --rm -it --init -p8888:8888 eicdev/tdis-pre:latest

# docker buildx build --tag eicdev/tdis-pre:latest --push .
# docker build -t eicdev/tdis-pre:latest .
# docker build -t eicdev/tdis-pre:latest --build-arg BUILD_THREADS=24 .
# docker push eicdev/tdis-pre:latest
#
# docker run --rm -it --init eicdev/tdis-pre:latest
# docker run --rm -it --init -p8888:8888 eicdev/tdis-pre:latest

FROM eicdev/ubuntu-root:latest

WORKDIR ${APP_ROOT}

# ---------------------------------------------------------------------------#
#  Build-time arguments & environment                                        #
# ---------------------------------------------------------------------------#
# Can be changed with e.g. --build-arg BUILD_THREADS=24
ARG BUILD_THREADS=8
ARG CXX_STANDARD=20

# Install OS dependencies
RUN apt-get update &&\
    apt-get install -y libfmt-dev libboost-filesystem-dev libboost-all-dev libspdlog-dev libboost-test-dev libxmu-dev libexpat-dev libtbb-dev &&\
    apt-get clean && apt-get autoremove -y && rm -rf /var/lib/apt/lists/*

### ---------------------------------------------------------------------- ###
###          fmt build                                                     ###
### ---------------------------------------------------------------------- ###
ENV FMT_VERSION=11.2.0
ENV FMT_SRC_PATH=${APP_ROOT}/fmt-src \
    FMT_BLD_PATH=${APP_ROOT}/fmt-build \
    FMT_INS_PATH=${APP_ROOT}/fmt

RUN set -ex && \
    git clone --branch ${FMT_VERSION} --depth 1 https://github.com/fmtlib/fmt.git "${FMT_SRC_PATH}" && \
    mkdir -p "${FMT_BLD_PATH}" && cd "${FMT_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${FMT_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
      "${FMT_SRC_PATH}" && \
    cmake --build . --target install -j"${BUILD_THREADS}" && \
    rm -rf "${FMT_BLD_PATH}" "${FMT_SRC_PATH}"

ENV LD_LIBRARY_PATH="${FMT_INS_PATH}/lib:${LD_LIBRARY_PATH}" \
    CMAKE_PREFIX_PATH="${FMT_INS_PATH}/lib/cmake/fmt:${CMAKE_PREFIX_PATH}"

### ---------------------------------------------------------------------- ###
###          Catch2 build                                                  ###
### ---------------------------------------------------------------------- ###
ENV CATCH2_VERSION=v3.8.1
ENV CATCH2_SRC_PATH=${APP_ROOT}/catch2-src \
    CATCH2_BLD_PATH=${APP_ROOT}/catch2-build \
    CATCH2_INS_PATH=${APP_ROOT}/catch2

RUN set -ex && \
    git clone --branch ${CATCH2_VERSION} --depth 1 https://github.com/catchorg/Catch2.git "${CATCH2_SRC_PATH}" && \
    mkdir -p "${CATCH2_BLD_PATH}" && cd "${CATCH2_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${CATCH2_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
      "${CATCH2_SRC_PATH}" && \
    cmake --build . --target install -j"${BUILD_THREADS}" && \
    rm -rf "${CATCH2_BLD_PATH}" "${CATCH2_SRC_PATH}"

ENV LD_LIBRARY_PATH="${CATCH2_INS_PATH}/lib:${LD_LIBRARY_PATH}" \
    CMAKE_PREFIX_PATH="${CATCH2_INS_PATH}/lib/cmake/Catch2:${CMAKE_PREFIX_PATH}"

### ---------------------------------------------------------------------- ###
###          PODIO build                                                   ###
### ---------------------------------------------------------------------- ###
ARG VERSION_PODIO=v01-03
ENV PODIO_SRC_PATH=${APP_ROOT}/podio-src \
    PODIO_BLD_PATH=${APP_ROOT}/podio-build \
    PODIO_INS_PATH=${APP_ROOT}/podio

RUN set -ex && \
    git clone --branch ${VERSION_PODIO} --depth 1 https://github.com/AIDASoft/podio.git "${PODIO_SRC_PATH}" && \
    mkdir -p "${PODIO_BLD_PATH}" && cd "${PODIO_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${PODIO_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
      -DBUILD_TESTING=OFF \
      "${PODIO_SRC_PATH}" && \
    cmake --build . --target install -j"${BUILD_THREADS}" && \
    rm -rf "${PODIO_BLD_PATH}" "${PODIO_SRC_PATH}"

ENV PODIO_ROOT=${PODIO_INS_PATH} \
    LD_LIBRARY_PATH="${PODIO_INS_PATH}/lib:${LD_LIBRARY_PATH}" \
    CMAKE_PREFIX_PATH="${PODIO_INS_PATH}/lib/cmake/podio:${CMAKE_PREFIX_PATH}"

### ---------------------------------------------------------------------- ###
###          Eigen3 build                                                  ###
### ---------------------------------------------------------------------- ###
ENV EIGEN3_VERSION=3.4.0
ENV EIGEN3_SRC_PATH=${APP_ROOT}/eigen3-src \
    EIGEN3_BLD_PATH=${APP_ROOT}/eigen3-build \
    EIGEN3_INS_PATH=${APP_ROOT}/eigen3

RUN set -ex && \
    git clone --branch ${EIGEN3_VERSION} --depth 1 https://gitlab.com/libeigen/eigen.git "${EIGEN3_SRC_PATH}" && \
    mkdir -p "${EIGEN3_BLD_PATH}" && cd "${EIGEN3_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${EIGEN3_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      "${EIGEN3_SRC_PATH}" && \
    cmake --build . --target install -j"${BUILD_THREADS}" && \
    rm -rf "${EIGEN3_BLD_PATH}" "${EIGEN3_SRC_PATH}"

ENV CMAKE_PREFIX_PATH="${EIGEN3_INS_PATH}/share/eigen3/cmake:${CMAKE_PREFIX_PATH}"

### ---------------------------------------------------------------------- ###
###          HepMC3 build                                                  ###
### ---------------------------------------------------------------------- ###
ENV HEPMC3_VERSION=3.3.1
ENV HEPMC3_SRC_PATH=${APP_ROOT}/hepmc3-src \
    HEPMC3_BLD_PATH=${APP_ROOT}/hepmc3-build \
    HEPMC3_INS_PATH=${APP_ROOT}/hepmc3

RUN set -ex && \
    git clone --branch ${HEPMC3_VERSION} --depth 1 https://gitlab.cern.ch/hepmc/HepMC3.git "${HEPMC3_SRC_PATH}" && \
    mkdir -p "${HEPMC3_BLD_PATH}" && cd "${HEPMC3_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${HEPMC3_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
      -DHEPMC3_ENABLE_PYTHON:BOOL=OFF \
      -DPython_LIBRARIES=/usr/lib/python3.12/config-3.12-x86_64-linux-gnu \
      -DPython_EXECUTABLE=/usr/bin/python3 \
      -DPython_INCLUDE_DIRS=/usr/include/python3.12 \
      "${HEPMC3_SRC_PATH}" && \
    cmake --build . --target install -j"${BUILD_THREADS}" && \
    rm -rf "${HEPMC3_BLD_PATH}" "${HEPMC3_SRC_PATH}"

ENV HEPMC3_DIR=${HEPMC3_INS_PATH} \
    PATH="${HEPMC3_INS_PATH}/bin:${PATH}" \
    LD_LIBRARY_PATH="${HEPMC3_INS_PATH}/lib:${LD_LIBRARY_PATH}" \
    CMAKE_PREFIX_PATH="${HEPMC3_INS_PATH}/share/HepMC3/cmake:${CMAKE_PREFIX_PATH}"

### ---------------------------------------------------------------------- ###
###          ACTS build                                                    ###
### ---------------------------------------------------------------------- ###
ARG VERSION_ACTS=v42.1.1
ENV ACTS_SRC_PATH=${APP_ROOT}/acts-src \
    ACTS_BLD_PATH=${APP_ROOT}/acts-build \
    ACTS_INS_PATH=${APP_ROOT}/acts

RUN set -ex && \
    git clone --branch ${VERSION_ACTS} --depth 1 https://github.com/acts-project/acts.git "${ACTS_SRC_PATH}" && \
    mkdir -p "${ACTS_BLD_PATH}" && cd "${ACTS_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${ACTS_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
      -DACTS_BUILD_PLUGIN_TGEO=ON \
      -DACTS_BUILD_PLUGIN_JSON=ON \
      -DACTS_BUILD_PLUGIN_PODIO=ON \
      -DACTS_BUILD_EXAMPLES_PYTHON_BINDINGS=ON \
      -DACTS_BUILD_PLUGIN_TGEO=ON \
      -DACTS_BUILD_PLUGIN_ACTSVG=OFF \
      -DACTS_BUILD_PLUGIN_DD4HEP=OFF \
      "${ACTS_SRC_PATH}" && \
    cmake --build . --target install -j"${BUILD_THREADS}" && \
    rm -rf "${ACTS_BLD_PATH}" "${ACTS_SRC_PATH}"

ENV ACTS_DIR=${ACTS_INS_PATH} \
    LD_LIBRARY_PATH="${ACTS_INS_PATH}/lib:${LD_LIBRARY_PATH}" \
    CMAKE_PREFIX_PATH="${ACTS_INS_PATH}/lib/cmake/Acts:${ACTS_INS_PATH}/lib/cmake/nlohmann_json:${ACTS_INS_PATH}/lib/cmake/ActsDD4hep:${CMAKE_PREFIX_PATH}"

### ---------------------------------------------------------------------- ###
###          JANA2 build                                                   ###
### ---------------------------------------------------------------------- ###
ARG VERSION_JANA2=v2.4.2
ENV JANA2_SRC_PATH=${APP_ROOT}/jana2-src \
    JANA2_BLD_PATH=${APP_ROOT}/jana2-build \
    JANA2_INS_PATH=${APP_ROOT}/jana2

RUN set -ex && \
    git clone --branch ${VERSION_JANA2} --depth 1 https://github.com/JeffersonLab/JANA2.git "${JANA2_SRC_PATH}" && \
    mkdir -p "${JANA2_BLD_PATH}" && cd "${JANA2_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${JANA2_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
      -DUSE_PODIO=On \
      -DUSE_ROOT=On \
      "${JANA2_SRC_PATH}" && \
    cmake --build . --target install -j"${BUILD_THREADS}" && \
    rm -rf "${JANA2_BLD_PATH}" "${JANA2_SRC_PATH}"

ENV JANA_HOME=${JANA2_INS_PATH} \
    JANA_PLUGIN_PATH="${JANA2_INS_PATH}/plugins" \
    PATH="${JANA2_INS_PATH}/bin:${PATH}" \
    LD_LIBRARY_PATH="${JANA2_INS_PATH}/lib:${LD_LIBRARY_PATH}" \
    CMAKE_PREFIX_PATH="${JANA2_INS_PATH}/lib/cmake/JANA:${CMAKE_PREFIX_PATH}"

SHELL ["/bin/bash", "-c"]
