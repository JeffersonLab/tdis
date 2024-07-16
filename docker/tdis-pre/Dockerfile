#
# docker build -t eicdev/tdis-pre:latest .
# docker build -t eicdev/tdis-pre:latest --build-arg BUILD_THREADS=24 .
# docker push eicdev/tdis-pre:latest
#
# docker run --rm -it --init eicdev/tdis-pre:latest
# docker run --rm -it --init -p8888:8888 eicdev/tdis-pre:latest

FROM rootproject/root:6.32.02-ubuntu24.04

# Number of build threads
ARG BUILD_THREADS=8

ENV EIC_ROOT /eic
ENV CONTAINER_ROOT /container
ENV APP_ROOT /container/app

SHELL ["/bin/bash", "-c"]

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update &&\
    apt-get install -y python3-pip python3-dev sudo git cmake gdb ninja-build apt-utils bash ccache clang valgrind &&\
    apt-get install -y dialog apt-utils&&\
    apt-get install -y tzdata &&\
    apt-get install -y libmsgsl-dev libftgl-dev libxml2-dev libldap2-dev libgsl0-dev libfftw3-dev libmysqlclient-dev libavahi-compat-libdnssd-dev libssl-dev python3-dev libcfitsio-dev xlibmesa-glu-dev libkrb5-dev gfortran graphviz-dev libpcre3-dev libglew-dev libxmu-dev libexpat-dev python3-jinja2 libtbb-dev libxerces-c-dev cmake libboost-filesystem-dev dpkg-dev libspdlog-dev libxext-dev qtbase5-dev libqt5opengl5-dev libx11-dev libboost-test-dev libocct-draw-dev libeigen3-dev libocct-data-exchange-dev python3-dev libocct-foundation-dev libfmt-dev libxft-dev libboost-program-options-dev libboost-dev binutils libxpm-dev curl libxerces-c3-dev occt-misc python3-yaml &&\
    rm -rf /var/lib/apt/lists/* &&\
    python3 -m pip install --upgrade pip


RUN useradd -m -G sudo eicuser
RUN echo "eicuser ALL=(ALL) NOPASSWD:ALL">>/etc/sudoers

RUN install -d -o eicuser -g eicuser ${CONTAINER_ROOT} && \
    install -d -o eicuser -g eicuser ${APP_ROOT} && \
    install -d -o eicuser -g eicuser /eic

USER eicuser
WORKDIR /home/eicuser
ENV PATH "$PATH:/home/eicuser/.local/bin"

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8


RUN python3 -m pip install --user --upgrade --force-reinstall  click &&\
    python3 -m pip install --user --upgrade --force-reinstall  appdirs &&\
    python3 -m pip install --user --upgrade --force-reinstall  edpm &&\
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
# eicrecon stack
RUN edpm config global cxx_standard=17 &&\
    #edpm config root branch="v6-28-02" &&\
    edpm set root /opt/root &&\
    #edpm install -j${BUILD_THREADS} root &&\
    echo "!!!ROOT INSTALLATION DONE!!!"


# aa
RUN python3 -m pip install --user --upgrade --force-reinstall  edpm &&\
    edpm config jana2 branch="v2.1.2" &&\
    edpm config jana2 cmake_flags="-DUSE_PODIO=Off -DUSE_ROOT=On" &&\
    edpm install jana4tdis --deps-only

    # jupyterab
RUN python3 -m pip install jupyterlab

USER root

# Create the entrypoint script
RUN echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'set -e' >> /entrypoint.sh && \
    echo '' >> /entrypoint.sh && \
    echo '# Source the environment file' >> /entrypoint.sh && \
    echo 'source /home/eicuser/.local/share/edpm/env.sh' >> /entrypoint.sh && \
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



#CMD jupyter lab --ip=0.0.0.0 --no-browser \
#    --NotebookApp.custom_display_url=http://127.0.0.1:8888 \
#    --NotebookApp.token=''