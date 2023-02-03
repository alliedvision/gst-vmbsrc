from ubuntu:18.04 as gst-vmbsrc_builder
RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        build-essential \
        wget \
        ca-certificates \
        libgstreamer1.0-dev \
        libgstreamer-plugins-base1.0-dev \
    && rm -rf /var/lib/apt/lists/*

# Add more up-to-date version of CMake than apt-get would provide
RUN wget https://github.com/Kitware/CMake/releases/download/v3.25.2/cmake-3.25.2-linux-x86_64.sh \
    && bash cmake-3.25.2-linux-x86_64.sh --skip-license --exclude-subdir \
    && rm cmake-3.25.2-linux-x86_64.sh

# mount the Vimba X installation directory into this volume
VOLUME ["/vimbax"]

# mount the checked out repository into this volume
VOLUME ["/gst-vmbsrc"]
WORKDIR /gst-vmbsrc

# Override Vmb_DIR path from preset so it is correct inside docker container
CMD cmake --preset linux64 -D Vmb_DIR=/vimbax/api/lib/cmake/vmb && cmake --build build-linux64
# Plugin will be located at build-linux64/libgstvimba.so