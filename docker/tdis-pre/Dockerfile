#
# docker build -t eicdev/tdis-pre:latest .
# docker build -t eicdev/tdis-pre:latest --build-arg BUILD_THREADS=24 .
# docker push eicdev/tdis-pre:latest
#
# docker run --rm -it --init eicdev/tdis-pre:latest
# docker run --rm -it --init -p8888:8888 eicdev/tdis-pre:latest

FROM ubuntu:noble

# Number of build threads
ARG BUILD_THREADS=8

# Software versions

ARG VERSION_CERN_ROOT=v6-32-06
ARG VERSION_ACTS=v37.2.0
ARG VERSION_PODIO=v01-01
ARG VERSION_JANA2=v2.3.2


ENV EIC_ROOT /eic
ENV CONTAINER_ROOT /container
ENV APP_ROOT /container/app

SHELL ["/bin/bash", "-c"]

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update &&\
    apt-get install -y python3-dev python3-pip python3-dev python3-jinja2 python3-yaml python3-numpy   \
                       sudo git cmake gdb ninja-build apt-utils bash ccache clang valgrind  gfortran g++ gcc curl \
                       dialog apt-utils dpkg-dev tzdata binutils \
                       libvtk9-dev libmsgsl-dev libftgl-dev libxml2-dev libldap2-dev  \
                       libgsl0-dev libfftw3-dev libmysqlclient-dev libavahi-compat-libdnssd-dev libssl-dev  \
                       libcfitsio-dev xlibmesa-glu-dev libkrb5-dev graphviz-dev libpcre3-dev libglew-dev  \
                       libxmu-dev libexpat-dev libtbb-dev libboost-filesystem-dev libboost-all-dev \
                       libspdlog-dev libxext-dev qtbase5-dev libqt5opengl5-dev libx11-dev libboost-test-dev  \
                       libocct-draw-dev libeigen3-dev libocct-data-exchange-dev libocct-foundation-dev  \
                       libfmt-dev libxft-dev libboost-program-options-dev libboost-dev libxpm-dev   \
                       libxerces-c3-dev occt-misc libvdt-dev libglu1-mesa-dev libgraphviz-dev \
                       libgsl-dev qtwebengine5-dev nlohmann-json3-dev &&\
    rm -rf /var/lib/apt/lists/*

# Set clang as default
RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-18 60


RUN useradd -m -G sudo tdisuser
RUN echo "tdisuser ALL=(ALL) NOPASSWD:ALL">>/etc/sudoers

RUN install -d -o tdisuser -g tdisuser ${CONTAINER_ROOT} && \
    install -d -o tdisuser -g tdisuser ${APP_ROOT} && \
    install -d -o tdisuser -g tdisuser /eic

USER tdisuser
WORKDIR /home/tdisuser
ENV PATH "$PATH:/home/tdisuser/.local/bin"

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8


RUN python3 -m pip install --user --upgrade --force-reinstall --break-system-packages  click &&\
    python3 -m pip install --user --upgrade --force-reinstall --break-system-packages  appdirs &&\
    python3 -m pip install --user --upgrade --force-reinstall --break-system-packages  edpm &&\
    edpm --top-dir=/container/app &&\
    edpm


# THIS IS NOT WORKING! HuCK debconf
# # https://github.com/moby/moby/issues/27988
#
# # install all packets but not eicrecon
# RUN  sudo apt-get update &&\
#      echo $(edpm req --all ubuntu) &&\
#      sudo echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections &&\
#      sudo chmod 777 /var/cache/debconf/ &&\
#      sudo chmod 777 /var/cache/debconf/passwords.dat &&\
#      sudo apt-get -y -q install $(edpm req --all ubuntu) &&\
#      sudo rm -rf /var/lib/apt/lists/*
# (!) Build with C++20

# EDPM configuration
RUN edpm config global cxx_standard=20 &&\
    echo "*** EDPM CONFIGURED ***"

# Setup and install ROOT
RUN edpm config root branch=${VERSION_CERN_ROOT} &&\
    edpm config root &&\
    edpm config root cmake_custom_flags=" -Dcuda=OFF -Dcudnn=OFF -Dtmva-gpu=OFF " &&\
    edpm install -j${BUILD_THREADS} root && edpm clean root


RUN edpm config acts branch=${VERSION_ACTS} &&\
    edpm config acts cmake_flags='-DACTS_BUILD_PLUGIN_TGEO=ON -DACTS_BUILD_PLUGIN_DD4HEP=OFF -DACTS_BUILD_PLUGIN_JSON=ON -DACTS_BUILD_PLUGIN_ACTSVG=OFF' &&\
    edpm install acts

RUN edpm config podio branch=${VERSION_PODIO} &&\
    edpm config podio cmake_flags="-DBUILD_TESTING=OFF" &&\
    edpm install podio

RUN edpm config jana2 branch=${VERSION_JANA2} &&\
    edpm config jana2 cmake_flags="-DUSE_PODIO=On -DUSE_ROOT=On -DCMAKE_BUILD_TYPE=Debug" &&\
    edpm install jana2

RUN ls && pwd

#    # jupyterab
#RUN python3 -m pip install jupyterlab

USER root


# Create the entrypoint script
RUN echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'set -e' >> /entrypoint.sh && \
    echo '' >> /entrypoint.sh && \
    echo '# Source the environment file' >> /entrypoint.sh && \
    echo 'source /home/tdisuser/scipm_env.sh' >> /entrypoint.sh && \
    echo '' >> /entrypoint.sh && \
    echo '# Execute the command provided as arguments to the script' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh

# Make the entrypoint script executable
RUN chmod +x /entrypoint.sh

# Set the entrypoint script as the ENTRYPOINT
ENTRYPOINT ["/entrypoint.sh"]

# That is for CI
USER root

RUN echo "done"


RUN apt-get update &&\
    apt-get install -y clang-tools-18 clang-tidy &&\
    ln -s /usr/lib/llvm-18/bin/clang-scan-deps /usr/local/bin/clang-scan-deps &&\
    rm -rf /var/lib/apt/lists/*