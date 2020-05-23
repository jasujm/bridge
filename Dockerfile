# FIXME: Currently includes way too much to the container image
# Also maybe vendorize the external requirements not in the apt repository?

FROM gcc:9

EXPOSE 5555

RUN set -ex;                                                          \
    mkdir -p /usr/src/bridge/build;                                   \
    apt-get update;                                                   \
    apt-get -y install cmake libboost-dev libzmq3-dev liblua5.3-dev;  \
    wget https://github.com/nlohmann/json/releases/download/v3.7.3/json.hpp --output-document=/usr/local/include/json.hpp; \
    wget https://raw.githubusercontent.com/zeromq/cppzmq/v4.6.0/zmq.hpp --output-document=/usr/local/include/zmq.hpp

COPY . /usr/src/bridge

RUN set -ex;       \
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
