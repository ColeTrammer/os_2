#!/bin/sh

export PORT_NAME=figlet
export SRC_DIR='figlet-2.2.5'
export BUILD_DIR='figlet-2.2.5'

download() {
    # Download tar.gz
    curl ftp://ftp.figlet.org/pub/figlet/program/unix/figlet-2.2.5.tar.gz --output figlet.tar.gz

    # Extract contents
    tar -xzvf figlet.tar.gz
}

patch() {
    git init
    git apply ../figlet-2.2.5.patch
}

build() {
    make install -j5
}

clean() {
    make clean
}

install() {
    cp -r ../figlet/bin $ROOT/sysroot/usr
    cp -r ../figlet/share $ROOT/sysroot
}

. ../.build_include.sh