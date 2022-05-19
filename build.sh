#!/usr/bin/env bash

set -Cex

# Target build: 'debug' | 'release'
case "${1}" in
    debug)
        export DIRECTORY_SUFFIX='debug'
        export BUILD_TYPE='Debug'
        ;;
    release)
        export DIRECTORY_SUFFIX='release'
        export BUILD_TYPE='Release'
        ;;
    *)
        export DIRECTORY_SUFFIX='release'
        export BUILD_TYPE='Release'
        ;;
esac

if [[ "$OSTYPE" == "darwin"* ]]; then
    if [[ -z ${CI_RUNNING+x} ]]; then
        brew update
        brew install cmake
    fi

    cmake \
        -S 'llvm' \
        -B "build-${DIRECTORY_SUFFIX}/" \
        -G 'Unix Makefiles' \
        -DCMAKE_INSTALL_PREFIX="${HOME}/opt/llvm-${DIRECTORY_SUFFIX}/" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DLLVM_TARGETS_TO_BUILD='X86' \
        -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD='SyncVM' \
        -DLLVM_OPTIMIZED_TABLEGEN='On' \
        -DLLVM_BUILD_DOCS='Off' \
        -DLLVM_INCLUDE_DOCS='Off' \
        -DLLVM_ENABLE_ASSERTIONS='On' \
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_BINDINGS='Off'

    cd "build-${DIRECTORY_SUFFIX}/"
    make -j "$(sysctl -n hw.logicalcpu)"
elif [[ -f '/etc/arch-release' ]]; then
    if [[ -z ${CI_RUNNING+x} ]]; then
        sudo pacman --sync --refresh --sysupgrade --noconfirm \
            cmake \
            clang \
            lld
    fi

    cmake \
        -S 'llvm' \
        -B "build-${DIRECTORY_SUFFIX}/" \
        -G 'Unix Makefiles' \
        -DCMAKE_INSTALL_PREFIX="${HOME}/opt/llvm-${DIRECTORY_SUFFIX}/" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_C_COMPILER='clang' \
        -DCMAKE_CXX_COMPILER='clang++' \
        -DLLVM_TARGETS_TO_BUILD='X86' \
        -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD='SyncVM' \
        -DLLVM_OPTIMIZED_TABLEGEN='On' \
        -DLLVM_USE_LINKER='lld' \
        -DLLVM_BUILD_DOCS='Off' \
        -DLLVM_INCLUDE_DOCS='Off' \
        -DLLVM_ENABLE_ASSERTIONS='On' \
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_BINDINGS='Off'

    cd "build-${DIRECTORY_SUFFIX}/"
    make -j "$(nproc)"
elif [[ "$OSTYPE" == "linux-gnu" ]]; then
    if [[ -z ${CI_RUNNING+x} ]]; then
        sudo apt --yes update
        sudo apt --yes install \
            cmake \
            clang-11 \
            lld-11
        export LLVM_VERSION=11
    fi

    cmake \
        -S 'llvm' \
        -B "build-${DIRECTORY_SUFFIX}/" \
        -G 'Unix Makefiles' \
        -DCMAKE_INSTALL_PREFIX="${HOME}/opt/llvm-${DIRECTORY_SUFFIX}/" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_C_COMPILER="clang-${LLVM_VERSION}" \
        -DCMAKE_CXX_COMPILER="clang++-${LLVM_VERSION}" \
        -DLLVM_TARGETS_TO_BUILD='X86' \
        -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD='SyncVM' \
        -DLLVM_OPTIMIZED_TABLEGEN='On' \
        -DLLVM_USE_LINKER="lld-${LLVM_VERSION}" \
        -DLLVM_BUILD_DOCS='Off' \
        -DLLVM_INCLUDE_DOCS='Off' \
        -DLLVM_ENABLE_ASSERTIONS='On' \
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_BINDINGS='Off'

    cd "build-${DIRECTORY_SUFFIX}/"
    make -j "$(nproc)"
fi

sudo make install
