FROM debian AS builder

LABEL project="cs"

RUN set -eux; \
    apt-get update -y; \
    apt-get install apt-utils bash build-essential cmake cmake-data g++ gcc libcurl4-openssl-dev libssl-dev make pkg-config sudo wget git lsb-release -y;

RUN set -eux; \
    git clone https://github.com/apostoldevel/apostol-cs.git /opt/cs;

WORKDIR /opt/cs

COPY . .

RUN set -eux; \
    git config --global pull.rebase false; \
    ./configure; \
    cd cmake-build-release; \
    make install;

EXPOSE 9220

CMD ["/usr/sbin/cs"]
