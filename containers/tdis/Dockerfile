# docker buildx build --tag eicdev/tdis:latest --push .
# docker buildx build --no-cache --build-arg BUILD_THREADS=24 --tag eicdev/tdis:latest --push .
# docker push eicdev/tdis:latest
#
# docker run --rm -it --init eicdev/tdis:latest
# docker run --rm -it --init -p8888:8888 eicdev/tdis:latest

FROM eicdev/tdis-pre:latest

WORKDIR ${APP_ROOT}

# Build-time arguments
ARG BUILD_THREADS=8

### ---------------------------------------------------------------------- ###
###          TDIS build                                                    ###
### ---------------------------------------------------------------------- ###
ENV TDIS_SRC_PATH=${APP_ROOT}/tdis-src \
    TDIS_BLD_PATH=${APP_ROOT}/tdis-build \
    TDIS_INS_PATH=${APP_ROOT}/tdis

# Clone TDIS repository
RUN git clone https://github.com/JeffersonLab/tdis.git "${TDIS_SRC_PATH}"

# Build and install TDIS
RUN set -ex && \
    mkdir -p "${TDIS_BLD_PATH}" && cd "${TDIS_BLD_PATH}" && \
    cmake \
      -DCMAKE_INSTALL_PREFIX="${TDIS_INS_PATH}" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -Dpodio_DIR="${APP_ROOT}/podio/lib/cmake/podio" \
      -DJANA_DIR="${APP_ROOT}/jana2/lib/cmake/JANA" \
      "${TDIS_SRC_PATH}" && \
    cmake --build . -j"${BUILD_THREADS}" && \
    cmake --build . --target install
    # && rm -rf "${TDIS_BLD_PATH}" "${TDIS_SRC_PATH}"

# Set environment variables for TDIS
ENV PATH="${TDIS_INS_PATH}/bin:${PATH}" \
    LD_LIBRARY_PATH="${TDIS_INS_PATH}/lib:${LD_LIBRARY_PATH}"

WORKDIR /root