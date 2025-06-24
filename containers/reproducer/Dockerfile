#
# docker build -t eicdev/reproducer:latest .
# docker build -t eicdev/reproducer:latest --build-arg BUILD_THREADS=24 .
# docker push eicdev/reproducer:latest
#
# docker run --rm -it --init eicdev/reproducer:latest
# docker run --rm -it --init -p8888:8888 eicdev/reproducer:latest

FROM rootproject/root:6.32.02-ubuntu24.04

# Number of build threads
ARG BUILD_THREADS=8

ARG DEBIAN_FRONTEND=noninteractive

RUN root-config --cflags

RUN apt-get update &&\
    apt-get install -y python3-pip python3-dev sudo git cmake gdb ninja-build apt-utils bash ccache clang valgrind &&\
    apt-get install -y dialog apt-utils&&\
    apt-get install -y tzdata &&\
    apt-get install -y libmsgsl-dev libftgl-dev libxml2-dev libldap2-dev libgsl0-dev libfftw3-dev libmysqlclient-dev libavahi-compat-libdnssd-dev libssl-dev python3-dev libcfitsio-dev xlibmesa-glu-dev libkrb5-dev gfortran graphviz-dev libpcre3-dev libglew-dev libxmu-dev libexpat-dev python3-jinja2 libtbb-dev libxerces-c-dev cmake libboost-filesystem-dev dpkg-dev libspdlog-dev libxext-dev qtbase5-dev libqt5opengl5-dev libx11-dev libboost-test-dev libocct-draw-dev libeigen3-dev libocct-data-exchange-dev python3-dev libocct-foundation-dev libfmt-dev libxft-dev libboost-program-options-dev libboost-dev binutils libxpm-dev curl libxerces-c3-dev occt-misc python3-yaml &&\
    rm -rf /var/lib/apt/lists/*

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

RUN mkdir -p /container/app/acts/src/v35.2.0 &&\
    git clone --depth 1 -b v35.2.0 https://github.com/acts-project/acts /container/app/acts/src/v35.2.0 &&\
    mkdir -p /container/app/acts/build/v35.2.0 &&\
    cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/container/app/acts/acts-v35.2.0 -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DACTS_BUILD_PLUGIN_TGEO=ON -DACTS_BUILD_PLUGIN_DD4HEP=OFF -DACTS_BUILD_PLUGIN_JSON=ON -DACTS_BUILD_PLUGIN_ACTSVG=OFF  /container/app/acts/src/v35.2.0&& cmake --build . -- -j 4&& cmake --build . --target install
RUN echo "done"



#CMD jupyter lab --ip=0.0.0.0 --no-browser \
#    --NotebookApp.custom_display_url=http://127.0.0.1:8888 \
#    --NotebookApp.token=''