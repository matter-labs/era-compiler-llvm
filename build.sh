#!/usr/bin/env bash

set -Cex

# Target build: 'debug' | 'release'
case "${1}" in
    debug)
        export DIRECTORY_SUFFIX="debug"
        export BUILD_TYPE="Debug"
        ;;
    release)
        export DIRECTORY_SUFFIX="release"
        export BUILD_TYPE="Release"
        ;;
    *)
        export DIRECTORY_SUFFIX="release"
        export BUILD_TYPE="Release"
        ;;
esac

cmake \
    -S 'llvm' \
    -B 'build/' \
    -G 'Unix Makefiles' \
    -DCMAKE_INSTALL_PREFIX="${HOME}/opt/llvm-${DIRECTORY_SUFFIX}/" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DLLVM_TARGETS_TO_BUILD='X86' \
    -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD='SyncVM' \
    -DLLVM_ENABLE_ASSERTIONS='On' \
    -DLLVM_OPTIMIZED_TABLEGEN='On' \
    -DCMAKE_C_COMPILER='clang-11' \
    -DCMAKE_CXX_COMPILER='clang++-11' \
    -DLLVM_USE_LINKER='lld-11'

cd 'build/'
make -j "$(nproc)"
make install
