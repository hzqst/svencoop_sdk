FROM registry.gitlab.steamos.cloud/steamrt/scout/sdk/i386:latest

RUN apt-get -y install \
        build-essential \
        make \
        libgl1-mesa-dev:i386 \
        libcurl4-openssl-dev:i386 \
    && rm -rf /var/lib/apt/lists/*

# Bootstrap on Scout so CMake itself can run against Scout's libc.
ADD --checksum=sha256:cc995701d590ca6debc4245e9989939099ca52827dd46b5d3592f093afe1901c https://github.com/Kitware/CMake/releases/download/v3.25.3/cmake-3.25.3.tar.gz /tmp/cmake.tar.gz
RUN tar -xzf /tmp/cmake.tar.gz -C /tmp \
    && cd /tmp/cmake-3.25.3 \
    && CC=gcc-9 CXX=g++-9 ./bootstrap --prefix=/usr/local --parallel=6 -- -DCMAKE_USE_OPENSSL=OFF \
    && make -j6 \
    && make install \
    && cd /tmp \
    && rm -rf cmake-3.25.3 cmake.tar.gz
