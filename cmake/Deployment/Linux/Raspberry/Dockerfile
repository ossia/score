FROM schachr/raspbian-stretch

COPY qemu-arm-static /usr/bin/qemu-arm-static

RUN apt -y update && \
    apt -y install make git wget bzip2  chrpath xz-utils libedit-dev procps zlib1g-dev \
    libasound2-dev libdbus-1-dev libssl-dev openssl libgles2-mesa-dev libegl1-mesa-dev

RUN wget https://github.com/ossia/sdk/releases/download/embedded-sdk1/raspberry-pi-2-toolchain.tar.gz -O /usr/raspberry-pi-2-toolchain.tar.gz && \
	cd /usr && \
	rm -rf local && \
	tar xaf raspberry-pi-2-toolchain.tar.gz && \
	rm raspberry-pi-2-toolchain.tar.gz

ADD build.sh /image/build.sh
RUN /image/build.sh