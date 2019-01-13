#!/bin/bash

set -e

build() {
  local ARCH="$1"
  shift
  local OUT="android/$ARCH/libmodpdfium.so"
  [ -e "$OUT" ] && return
  echo "building $OUT"
  gn gen out --args="\"$@\" target_os=\"android\" pdf_bundle_freetype=true pdf_is_standalone=false is_component_build=false pdf_enable_xfa=false pdf_enable_v8=false is_debug=false is_official_build=true"
  ninja -C out modpdfium
  mkdir -p $(dirname "$OUT") && cp out/libmodpdfium.so "$OUT"
}

#build arm armeabi arm_arch=\"armv5\" arm_use_neon=false arm_use_thumb=false arm_fpu=\"vfp\" arm_float_abi=\"softfp\" treat_warnings_as_errors=false # ld.lld: error: undefined symbol: __atomic_store_4
build armeabi target_cpu=\"arm\" arm_arch=\"armv6k\" arm_use_neon=false arm_use_thumb=false arm_fpu=\"vfp\" arm_float_abi=\"softfp\"
build armeabi-v7a target_cpu=\"arm\" arm_version=7
build arm64-v8a target_cpu=\"arm64\"
build x86 target_cpu=\"x86\"
build x86_64 target_cpu=\"x64\"
#build mipsel mips # ld.lld: error: found local symbol '_bss_end__' in global part of symbol table
#build mips64el mips64

