#!/bin/sh

set -e

# Variables
export ROOT="${ROOT:-$PWD/../..}"
export TARGET=`$ROOT/default-host.sh`

unset CC
unset CXX
unset AS

if [ ! -e gcc-9.2.0 ]; then
    # Download tar.gz
    curl http://ftp.gnu.org/gnu/gcc/gcc-9.2.0/gcc-9.2.0.tar.gz --output gcc-os_2-9.2.0.tar.gz

    # Extract contents
    tar -xzvf gcc-os_2-9.2.0.tar.gz

    # Apply patch
    cd gcc-9.2.0
    git init
    git apply ../gcc-os_2-9.2.0.patch
    git apply ../gcc-9.2.0-customizations.patch
    git apply ../gcc-9.2.0-shlib.patch
    git apply ../gcc-9.2.0-libstdc++.patch
    cd ..
fi

if [ $ONLY_DOWNLOAD_AND_EXTRACT ]; then 
    exit
fi

if [ ! -d "$ROOT/sysroot/usr/include" ];
then
    echo "Installing libc headers..."
    mkdir -p "$ROOT/sysroot/usr"
    cp -R -p "$ROOT/libs/libc/include" "$ROOT/sysroot/usr"
    echo "Done"
fi

if [ ! -d build-gcc ];
then
    mkdir -p build-gcc
    cd build-gcc
    export gcc_cv_initfini_array=yes
    ../gcc-9.2.0/configure \
        --target="$TARGET" \
        --prefix="$ROOT/toolchain/cross" \
        --disable-nls \
        --disable-lto \
        --enable-shared \
        --with-sysroot="$ROOT/sysroot" \
        --enable-languages=c,c++ \
        --enable-threads=posix \
        --with-build-time-tools=$ROOT/toolchain/cross/bin
    cd ..
fi

# Build
cd build-gcc
make all-gcc -j5
make all-target-libgcc -j5

# Install
mkdir -p ../../cross
make install-gcc install-target-libgcc 

cp "$ROOT/toolchain/cross/$TARGET/lib/libgcc_s.so.1" "$ROOT/base/usr/lib"

cd ..
