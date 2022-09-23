#!/usr/bin/env bash

set -ex

ROOT_DIR="${PWD}"

TARGET_BUILD="${ROOT_DIR}/build-target/"
TARGET_INSTALL='/opt/llvm-release/'

if [[ "$OSTYPE" == "darwin"* ]]; then
    if [[ -z ${CI_RUNNING+x} ]]; then
        brew update
        brew install cmake
    fi

    cmake \
        -S 'llvm' \
        -B "${TARGET_BUILD}" \
        -G 'Ninja' \
        -DPACKAGE_VENDOR='Matter Labs' \
        -DCLANG_VENDOR='Matter Labs' \
        -DCLANG_REPOSITORY_STRING='origin' \
        -DCMAKE_INSTALL_PREFIX="${TARGET_INSTALL}" \
        -DCMAKE_BUILD_TYPE='Release' \
        -DLLVM_TARGETS_TO_BUILD='SyncVM' \
        -DLLVM_OPTIMIZED_TABLEGEN='On' \
        -DLLVM_BUILD_TESTS='Off' \
        -DLLVM_BUILD_DOCS='Off' \
        -DLLVM_INCLUDE_DOCS='Off' \
        -DLLVM_INCLUDE_TESTS='Off' \
        -DLLVM_ENABLE_ASSERTIONS='On' \
        -DLLVM_ENABLE_TERMINFO='Off' \
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_ZLIB='Off' \
        -DLLVM_ENABLE_LIBXML2='Off' \
        -DLLVM_ENABLE_BINDINGS='Off' \
        -DCMAKE_OSX_DEPLOYMENT_TARGET='10.9'
elif [[ -f '/etc/arch-release' ]]; then
    if [[ -z ${CI_RUNNING+x} ]]; then
        sudo pacman --sync --refresh --sysupgrade --noconfirm \
            cmake \
            clang \
            lld \
            ninja
    fi

    cmake \
        -S 'llvm' \
        -B "${TARGET_BUILD}" \
        -G 'Ninja' \
        -DPACKAGE_VENDOR='Matter Labs' \
        -DCLANG_VENDOR='Matter Labs' \
        -DCLANG_REPOSITORY_STRING='origin' \
        -DCMAKE_INSTALL_PREFIX="${TARGET_INSTALL}" \
        -DCMAKE_BUILD_TYPE='Release' \
        -DCMAKE_C_COMPILER='clang' \
        -DCMAKE_CXX_COMPILER='clang++' \
        -DLLVM_TARGETS_TO_BUILD='SyncVM' \
        -DLLVM_OPTIMIZED_TABLEGEN='On' \
        -DLLVM_USE_LINKER='lld' \
        -DLLVM_BUILD_TESTS='Off' \
        -DLLVM_BUILD_DOCS='Off' \
        -DLLVM_INCLUDE_DOCS='Off' \
        -DLLVM_INCLUDE_TESTS='Off' \
        -DLLVM_ENABLE_ASSERTIONS='On' \
        -DLLVM_ENABLE_TERMINFO='Off' \
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_ZLIB='Off' \
        -DLLVM_ENABLE_LIBXML2='Off' \
        -DLLVM_ENABLE_BINDINGS='Off'
elif [[ "$OSTYPE" == "linux-gnu" ]]; then
    if [[ -z ${CI_RUNNING+x} ]]; then
        sudo apt --yes update
        sudo apt --yes install \
            cmake \
            clang-11 \
            lld-11 \
            ninja-build
        export LLVM_VERSION=11
    fi

    cmake \
        -S 'llvm' \
        -B "${TARGET_BUILD}" \
        -G 'Ninja' \
        -DPACKAGE_VENDOR='Matter Labs' \
        -DCLANG_VENDOR='Matter Labs' \
        -DCLANG_REPOSITORY_STRING='origin' \
        -DCMAKE_INSTALL_PREFIX="${TARGET_INSTALL}" \
        -DCMAKE_BUILD_TYPE='Release' \
        -DCMAKE_C_COMPILER="clang-${LLVM_VERSION}" \
        -DCMAKE_CXX_COMPILER="clang++-${LLVM_VERSION}" \
        -DLLVM_TARGETS_TO_BUILD='SyncVM' \
        -DLLVM_OPTIMIZED_TABLEGEN='On' \
        -DLLVM_USE_LINKER="lld-${LLVM_VERSION}" \
        -DLLVM_BUILD_TESTS='Off' \
        -DLLVM_BUILD_DOCS='Off' \
        -DLLVM_INCLUDE_DOCS='Off' \
        -DLLVM_INCLUDE_TESTS='Off' \
        -DLLVM_ENABLE_ASSERTIONS='On' \
        -DLLVM_ENABLE_TERMINFO='Off' \
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_ZLIB='Off' \
        -DLLVM_ENABLE_LIBXML2='Off' \
        -DLLVM_ENABLE_BINDINGS='Off' \
        -DPython3_EXECUTABLE="$(which python3)"
fi

if [[ "$OSTYPE" == "darwin"* ]]; then
    sudo ninja -C "${TARGET_BUILD}" install
else
    ninja -C "${TARGET_BUILD}" install
fi
