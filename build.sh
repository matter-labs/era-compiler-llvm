#!/bin/bash -Cex

cmake \
    -S 'llvm' \
    -B 'build/' \
    -G 'Unix Makefiles' \
    -DCMAKE_INSTALL_PREFIX="${HOME}/opt/llvm" \
    -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD='SyncVM' \
    -DCMAKE_C_COMPILER='clang-11' \
    -DCMAKE_CXX_COMPILER='clang++-11' \
    -DLLVM_USE_LINKER='lld-11' \
    -DCMAKE_BUILD_TYPE='Debug' \
    -DLLVM_ENABLE_ASSERTIONS='On' \
    -DBUILD_SHARED_LIBS='On' \
    -DLLVM_OPTIMIZED_TABLEGEN='On'
cd 'build/'
make -j8
make install
