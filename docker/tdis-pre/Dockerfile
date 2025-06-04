# BUILDX_EXPERIMENTAL=1 docker buildx debug --invoke bash build --progress=plain
# docker buildx build --tag eicdev/tdis-pre:latest --push .
# docker build -t eicdev/tdis-pre:latest .
# docker build -t eicdev/tdis-pre:latest --build-arg BUILD_THREADS=24 .
# docker push eicdev/tdis-pre:latest
#
# docker run --rm -it --init eicdev/tdis-pre:latest
# docker run --rm -it --init -p8888:8888 eicdev/tdis-pre:latest

FROM eicdev/ubuntu-root:latest

# Number of build threads
ARG APP_DIR
ARG BUILD_THREADS=8
ARG CXX_STANDARD=20
ARG BUILD_TYPE=RelWithDebInfo
ARG VERSION_JANA2=v2.4.0
ENV CMAKE_PREFIX_PATH=""

# Install OS dependencies
RUN apt-get update &&\
    apt-get install -y libfmt-dev &&\
    apt-get clean && apt-get autoremove -y && rm -rf /var/lib/apt/lists/*


WORKDIR ${APP_ROOT}

## For CI or final build steps (optional)
#RUN apt-get update && \
#    apt-get install -y clang-tools-18 clang-tidy dos2unix nlohmann-json3-dev&& \
#    ln -s /usr/lib/llvm-18/bin/clang-scan-deps /usr/local/bin/clang-scan-deps && \
#    rm -rf /var/lib/apt/lists/* \

# Install EDPM v3 and set the top_dir
RUN python3 -m pip install --user --upgrade --force-reinstall --break-system-packages edpm


#RUN edpm add catch2 && \
#    edpm install catch2 && \
#    edpm clean catch2


# ---------------------------------------------------
#   P O D I O
# ---------------------------------------------------
ARG PODIO_BRANCH=v01-03
ARG PODIO_URL="https://github.com/AIDASoft/podio.git"
ARG PODIO_SOURCE_PATH=${APP_ROOT}/podio-source
ARG PODIO_BUILD_PATH=${APP_ROOT}/podio-build
ARG PODIO_INSTALL_PATH=${APP_ROOT}/podio

RUN  git clone --depth 1 -b ${PODIO_BRANCH} ${PODIO_URL} ${PODIO_SOURCE_PATH} \
  && cmake -DCMAKE_INSTALL_PREFIX=${PODIO_INSTALL_PATH} \
          -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
          -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
          -DBUILD_TESTING=OFF \
          -B ${PODIO_BUILD_PATH} \
          -S ${PODIO_SOURCE_PATH}

RUN cmake --build ${PODIO_BUILD_PATH} -- -j ${BUILD_THREADS} \
 && cmake --build ${PODIO_BUILD_PATH} --target install \
 && rm -rf ${PODIO_BUILD_PATH} && rm -rf ${PODIO_SOURCE_PATH}

ENV CMAKE_PREFIX_PATH=${PODIO_INSTALL_PATH}/lib/cmake/podio:$CMAKE_PREFIX_PATH
#
#RUN git clone --depth 1 -b ${PODIO_BRANCH} ${PODIO_URL} ${APP_ROOT}/podio-src
#RUN edpm add podio && \
#   edpm config podio branch=${VERSION_PODIO} && \
#   edpm config podio cmake_flags="" && \
#   edpm install podio \
#
#
##
## ACTS
##
#ARG ACTS_BRANCH=v41.0.0
#ARG ACTS_URL="https://gitlab.cern.ch/acts/acts-core"
#
#RUN git clone --depth 1 -b ${ACTS_BRANCH} ${ACTS_URL} ${APP_ROOT}/acts-src
#RUN edpm add acts && \
#    edpm config acts branch=${VERSION_ACTS} && \
#    edpm config acts cmake_flags="-DACTS_BUILD_PLUGIN_TGEO=ON -DACTS_BUILD_PLUGIN_DD4HEP=ON -DACTS_BUILD_PLUGIN_JSON=ON -DACTS_BUILD_PLUGIN_ACTSVG=OFF -DACTS_BUILD_EXAMPLES_PYTHON_BINDINGS=ON -DACTS_BUILD_PLUGIN_TGEO=ON -DACTS_BUILD_PLUGIN_DD4HEP=OFF -DACTS_BUILD_PLUGIN_JSON=ON -DACTS_BUILD_PLUGIN_ACTSVG=OFF" && \
#    edpm install acts
#
#
#
##
## 4) JANA2
##
#RUN edpm add jana2 && \
#   edpm config jana2 branch=${VERSION_JANA2} && \
#   edpm config jana2 cmake_flags="-DUSE_PODIO=On -DUSE_ROOT=On -DCMAKE_BUILD_TYPE=Debug" && \
#   edpm install jana2
##
### Print a bit of info
##RUN ls && pwd
##
### (Optional) jupyterlab or other python packages
### RUN python3 -m pip install --user jupyterlab
##
##USER root
##
#SHELL ["/bin/bash", "-c"]
#
## Create an entrypoint script that sources environment
#RUN echo '#!/bin/bash' > /entrypoint.sh && \
#    echo 'set -e' >> /entrypoint.sh && \
#    echo '' >> /entrypoint.sh && \
#    echo '# Source the EDPM environment script (env.sh)' >> /entrypoint.sh && \
#    echo 'source /home/tdisuser/env.sh || true' >> /entrypoint.sh && \
#    echo '' >> /entrypoint.sh && \
#    echo 'exec "$@"' >> /entrypoint.sh
#RUN chmod +x /entrypoint.sh
#
#
#
#RUN dos2unix /entrypoint.sh
#ENTRYPOINT ["/entrypoint.sh"]

#RUN echo "done"
