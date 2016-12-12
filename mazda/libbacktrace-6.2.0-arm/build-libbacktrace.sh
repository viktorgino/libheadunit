#!/bin/bash
#update the temp paths as required
OUTDIR=/home/lucas/backtracebuild
M3TOOLCHAIN=/home/lucas/m3-toolchain-lmagder

#copy arm version on top, old protoc stays since it doesn't have the target prefix
export CFLAGS=--sysroot=$M3TOOLCHAIN/arm-cortexa9_neon-linux-gnueabi/sysroot
export LFLAGS=--sysroot=$M3TOOLCHAIN/arm-cortexa9_neon-linux-gnueabi/sysroot
./configure --prefix=$OUTDIR --enable-static --disable-shared --target=arm-cortexa9_neon-linux-gnueabi --host=arm-cortexa9_neon-linux-gnueabi PATH=$PATH:$M3TOOLCHAIN/bin
make PATH=$PATH:$M3TOOLCHAIN/bin

mkdir -p $OUTDIR
cp backtrace.h $OUTDIR
cp backtrace-supported.h $OUTDIR
cp .libs/libbacktrace.a $OUTDIR

