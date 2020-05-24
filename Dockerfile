# FIXME: Currently includes way too much to the container image
# Also maybe vendorize the external requirements not in the apt repository?

FROM gcc:9

EXPOSE 5555

RUN set -ex;                                                                       \
    echo deb http://deb.debian.org/debian bullseye main >> /etc/apt/sources.list;  \
    apt-get update;                                                                \
    apt-get -y install -t buster cmake libboost-dev libzmq3-dev liblua5.3-dev;     \
    apt-get -y install -t bullseye nlohmann-json3-dev;                             \
    wget https://raw.githubusercontent.com/zeromq/cppzmq/v4.6.0/zmq.hpp --output-document=/usr/local/include/zmq.hpp;

COPY . /usr/src/bridge

RUN set -ex;                                    \
    mkdir -p /usr/src/bridge/build;             \
    cd /usr/src/bridge/build;                   \
    cmake -D CMAKE_BUILD_TYPE=Release           \
          -D BRIDGE_BUILD_CARD_SERVER:BOOL=OFF  \
          -D BRIDGE_BUILD_TESTS:BOOL=OFF        \
          -D BRIDGE_BUILD_DOCS:BOOL=OFF         \
          ..;      \
    make;          \
    make install;  \
    make clean

ENTRYPOINT ["bridge"]
