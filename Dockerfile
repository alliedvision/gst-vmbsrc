from ubuntu:18.04 as build_base
RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        build-essential \
    && rm -rf /var/lib/apt/lists/*

# More modern CMake version needed -> Build and install ourselves
# Alternatively one might download a precompiled tar.gz from Github, but building it
# is platform agnostic
from build_base as cmake_installer
RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        wget \
        ca-certificates \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/Kitware/CMake/releases/download/v3.19.4/cmake-3.19.4.tar.gz && \
    tar xf cmake-3.19.4.tar.gz && \
    rm cmake-3.19.4.tar.gz && \
    cd ./cmake-3.19.4 && \
    ./bootstrap --prefix=/usr/local && \
    make && \
    make install

# This image will perform the actual GStreamer plugin build process
FROM build_base as gst-vimbasrc_builder
RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        libgstreamer1.0-dev \
        libgstreamer-plugins-base1.0-dev \
    && rm -rf /var/lib/apt/lists/*

COPY --from=cmake_installer /usr/local /usr/local

ENV VIMBA_HOME=/vimba

# mount the checked out repository into this volume
VOLUME ["/gst-vimbasrc"]
WORKDIR /gst-vimbasrc

# Follow README
CMD ["sh", "build.sh"]
# Plugin will be located at plugins/.libs/libgstvimba.so