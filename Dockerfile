FROM debian AS builder

LABEL project="cs"

RUN set -eux; \
    apt-get update -y; \
    apt-get install apt-utils bash build-essential cmake cmake-data g++ gcc libcurl4-openssl-dev libssl-dev make pkg-config sudo wget git mc lsb-release -y;

WORKDIR /opt/cs

COPY . .

RUN set -eux; \
    cp -r docker/CMakeLists.txt .; \
    cp -r docker/conf .; \
    cp -r docker/www .; \
    cmake -DCMAKE_BUILD_TYPE=Release . -B cmake-build-release; \
    cd cmake-build-release; \
    make install;

EXPOSE 9220

CMD ["/usr/sbin/cs"]
