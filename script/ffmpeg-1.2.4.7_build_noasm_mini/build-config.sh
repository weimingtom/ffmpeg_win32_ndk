#!/bin/bash

export TMPDIR=C:/tmp
#export PATH=$PATH:${PREBUILT}/bin

ANDROID_API=android-9
NDK=C:/cygwin/home/Administrator/android-ndk-r7b-windows/android-ndk-r7b
SYSROOT=${NDK}/platforms/${ANDROID_API}/arch-arm
PREBUILT=${NDK}/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows
CROSS_PREFIX=${PREBUILT}/bin/arm-linux-androideabi-
ARM_INCLUDE=${SYSROOT}/usr/include
ARM_LIB=${SYSROOT}/usr/lib
PREFIX=../jni/libffmpeg
OPTIMIZE_CFLAGS=" -mthumb "
ADDITIONAL_CONFIGURE_FLAG=

./configure \
 --arch=arm \
 --target-os=linux \
 --enable-cross-compile \
 --cross-prefix=${CROSS_PREFIX} \
 --prefix=${PREFIX} \
 --sysroot=${SYSROOT} \
 --extra-cflags=" -I${ARM_INCLUDE} -DANDROID ${OPTIMIZE_CFLAGS}" \
 --extra-ldflags=" -L${ARM_LIB}" \
 --disable-debug \
 --disable-ffplay \
 --disable-ffprobe \
 --disable-ffserver \
 --enable-avfilter \
 --enable-decoders \
 --enable-demuxers \
 --enable-encoders \
 --enable-filters \
 --enable-indevs \
 --enable-network \
 --enable-parsers \
 --enable-protocols \
 --enable-swscale \
 --enable-gpl \
 --enable-nonfree \
 --enable-thumb \
 --disable-armv5te \
 --disable-armv6 \
 --disable-armv6t2 \
 --disable-vfp \
 --disable-neon \
 --disable-inline-asm \
 --disable-asm \
 --disable-doc \
 ${ADDITIONAL_CONFIGURE_FLAG}
