#!/bin/bash

set -e

build() {
  local CPU=$1
  local ARCH=$2
  local ARGS="pdf_bundle_freetype=true pdf_is_standalone=false is_component_build=false pdf_enable_xfa=false pdf_enable_v8=false is_debug=false is_official_build=true"
  local OUT=android/$ARCH/libpdfium.so
  [ -e $OUT ] && return
  echo "building $OUT"
  gn gen out --args="target_os=\"android\" target_cpu=\"$CPU\" $ARGS"
  ninja -C out pdfium
  mkdir -p $(dirname $OUT) && cp out/libpdfium.so $OUT
}

build x86 x86

build x64 x86_64

build arm armeabi

build arm64 arm64-v8a

build mipsel mips

build mips64el mips64

