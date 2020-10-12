FROM debian:buster AS base

RUN set -ex;                               \
    echo deb http://deb.debian.org/debian buster-backports main >> /etc/apt/sources.list;  \
    apt-get update;                        \
    apt-get -y install libzmq5 liblua5.3;  \
    apt-get clean;

FROM base AS builder

# - nlohmann-json3-dev package for buster is broken (doesn't include
#   CMake configs), so import a newer version from the backports
# - build cppzmq from sources, because there is no package for it
# - build rocksdb from sources because the packaged version doesn't come with RTTI

RUN set -ex;                                                                          \
    apt-get -y install g++ curl cmake libboost-dev libzmq3-dev liblua5.3-dev;         \
    apt-get -y install -t buster-backports nlohmann-json3-dev;                        \
    mkdir -p /usr/src;                                                                \
    cd /usr/src;                                                                      \
    curl -L https://github.com/zeromq/cppzmq/archive/v4.6.0.tar.gz | tar -zxf -;      \
    mkdir -p /usr/src/cppzmq-4.6.0/build;                                             \
    cd /usr/src/cppzmq-4.6.0/build;                                                   \
    cmake -D CMAKE_BUILD_TYPE=Release -D CPPZMQ_BUILD_TESTS:BOOL=OFF ..; make; make install;  \
    cd /usr/src;                                                                      \
    curl -L https://github.com/facebook/rocksdb/archive/v6.11.4.tar.gz | tar -zxf -;  \
    mkdir -p /usr/src/rocksdb-6.11.4/build;                                           \
    cd /usr/src/rocksdb-6.11.4/build;                                                 \
    cmake -D CMAKE_BUILD_TYPE=Release -D USE_RTTI=1 -D WITH_BENCHMARK_TOOLS:BOOL=OFF  \
          -D WITH_TOOLS:BOOL=OFF -D WITH_GFLAGS:BOOL=OFF ..; make; make install

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
    make install

FROM base

EXPOSE 5555

COPY --from=builder /usr/lib/x86_64-linux-gnu/librocksdb.so.6 /usr/lib/x86_64-linux-gnu
COPY --from=builder /usr/local/bin/bridge /usr/local/bin

ENTRYPOINT ["bridge"]
