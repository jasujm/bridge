# FIXME: Currently includes way too much to the container image

FROM gcc:9

EXPOSE 5555

# - nlohmann-json3-dev package for buster is broken (doesn't include
#   CMake configs), so import a newer version from the bullseye
#   release.
# - build cppzmq from sources, because there is no package for it

RUN set -ex;                                                                       \
    echo deb http://deb.debian.org/debian bullseye main >> /etc/apt/sources.list;  \
    apt-get update;                                                                \
    apt-get -y install -t buster cmake libboost-dev libzmq3-dev liblua5.3-dev;     \
    apt-get -y install -t bullseye nlohmann-json3-dev;                             \
    mkdir -p /usr/src;                                                             \
    cd /usr/src;                                                                   \
    curl -L https://github.com/zeromq/cppzmq/archive/v4.6.0.tar.gz | tar -zxf -;   \
    mkdir -p /usr/src/cppzmq-4.6.0/build;                                          \
    cd /usr/src/cppzmq-4.6.0/build;                                                \
    cmake -D CPPZMQ_BUILD_TESTS:BOOL=OFF ..;                                       \
    make;                                                                          \
    make install;                                                                  \
    make clean

COPY . /usr/src/bridge

RUN set -ex;                                    \
    mkdir -p /usr/src/bridge/build;             \
    cd /usr/src/bridge/build;                   \
    cmake -D CMAKE_BUILD_TYPE=Release           \
          -D BRIDGE_BUILD_CARD_SERVER:BOOL=OFF  \
          -D BRIDGE_BUILD_TESTS:BOOL=OFF        \
          -D BRIDGE_BUILD_DOCS:BOOL=OFF         \
          ..;                                   \
    make;                                       \
    make install;                               \
    make clean

ENTRYPOINT ["bridge"]
