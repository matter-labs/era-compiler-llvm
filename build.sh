#!/bin/bash -Cex

cmake \
    -S 'llvm' \
    -B 'build/' \
    -G 'Unix Makefiles' \
    -DCMAKE_INSTALL_PREFIX="${HOME}/opt/llvm" \
    -DCMAKE_BUILD_TYPE='Debug' \
    -DLLVM_ENABLE_ASSERTIONS='On' \
    -DLLVM_ENABLE_PROJECTS='clang;libcxx;libcxxabi'
cd 'build/'
make -j8
make install
