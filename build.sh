#!/usr/bin/env bash

set -Cex

cmake \
    -S 'llvm' \
    -B 'build/' \
    -G 'Unix Makefiles' \
    -DCMAKE_INSTALL_PREFIX="${HOME}/opt/llvm/" \
    -DCMAKE_BUILD_TYPE='Debug' \
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
