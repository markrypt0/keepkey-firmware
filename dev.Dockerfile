FROM amd64/alpine

RUN apk add gcompat

# # Install python/pip
# ENV PYTHONUNBUFFERED=1
# RUN apk add --update --no-cache python3 && ln -sf python3 /usr/bin/python
# RUN python3 -m ensurepip
# RUN pip3 install --no-cache --upgrade pip setuptools

RUN apk add --update --no-cache \
    bzip2-dev \
    ca-certificates \
    git \
    openssl \
    scons \
    tar \
    w3m \
    unzip \
    py-setuptools \
    make \
    cmake

# RUN pip3 install \
#     "MarkupSafe==1.1.1" \
#     "ecdsa>=0.9" \
#     "protobuf>=3.0.0" \
#     "mnemonic>=0.8" \
#     requests \
#     flask \
#     pytest \
#     semver

RUN apk add py3-ecdsa py3-requests py3-flask py3-pytest py3-semver
RUN apk add --update py3-protobuf
RUN apk add --update py3-build

# Install gcc-arm-none-eabi
WORKDIR /root
RUN wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2
RUN tar xvf gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2
RUN cp -r gcc-arm-none-eabi-10-2020-q4-major/* /usr/local
RUN rm gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2
RUN rm -rf gcc-arm-none-eabi-10-2020-q4-major

# Install protobuf-compiler v3.5.1
WORKDIR /root
RUN mkdir protoc3
RUN wget https://github.com/google/protobuf/releases/download/v3.19.4/protoc-3.19.4-linux-x86_64.zip
RUN unzip protoc-3.19.4-linux-x86_64.zip -d protoc3
RUN mv protoc3/bin/* /usr/local/bin
RUN mv protoc3/include/* /usr/local/include
RUN rm -rf protoc3

# Install protobuf/python3 support
WORKDIR /root
RUN wget https://github.com/google/protobuf/releases/download/v3.19.4/protobuf-python-3.19.4.zip
RUN mkdir protobuf-python
RUN unzip protobuf-python-3.19.4.zip -d protobuf-python

WORKDIR /root/protobuf-python/protobuf-3.19.4/python
RUN python setup.py install

# Install nanopb
WORKDIR /root
RUN git clone --branch v1.0.0 https://github.com/markrypt0/nanopb.git
WORKDIR /root/nanopb/generator/proto
RUN make

RUN rm -rf /root/protobuf-python

# Setup environment
ENV PATH /root/nanopb/generator:$PATH

# Build libopencm3
WORKDIR /root
RUN git clone -b devdebug-1 https://github.com/markrypt0/libopencm3.git
WORKDIR /root/libopencm3
ENV FP_FLAGS="-mfloat-abi=soft"
RUN make TARGETS='stm32/f2 stm32/f4'

RUN apk add --update --no-cache \
    clang \
    gcc \
    g++ \
    binutils
