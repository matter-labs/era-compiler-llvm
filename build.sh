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
    brew update
    brew install cmake

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
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_BINDINGS='Off'

    cd "build-${DIRECTORY_SUFFIX}/"
    make -j "$(sysctl -n hw.logicalcpu)"
elif [[ -f '/etc/arch-release' ]]; then
    sudo pacman --sync --refresh --sysupgrade --noconfirm \
        cmake \
	clang \
	lld

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
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_BINDINGS='Off'

    cd "build-${DIRECTORY_SUFFIX}/"
    make -j "$(nproc)"
elif [[ "$OSTYPE" == "linux-gnu" ]]; then
    sudo apt --yes update
    sudo apt --yes install \
        cmake \
        clang-11 \
        lld-11

    cmake \
        -S 'llvm' \
        -B "build-${DIRECTORY_SUFFIX}/" \
        -G 'Unix Makefiles' \
        -DCMAKE_INSTALL_PREFIX="${HOME}/opt/llvm-${DIRECTORY_SUFFIX}/" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_C_COMPILER='clang-11' \
        -DCMAKE_CXX_COMPILER='clang++-11' \
        -DLLVM_TARGETS_TO_BUILD='X86' \
        -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD='SyncVM' \
        -DLLVM_OPTIMIZED_TABLEGEN='On' \
        -DLLVM_USE_LINKER='lld-11' \
        -DLLVM_BUILD_DOCS='Off' \
        -DLLVM_INCLUDE_DOCS='Off' \
        -DLLVM_ENABLE_DOXYGEN='Off' \
        -DLLVM_ENABLE_SPHINX='Off' \
        -DLLVM_ENABLE_OCAMLDOC='Off' \
        -DLLVM_ENABLE_BINDINGS='Off'

    cd "build-${DIRECTORY_SUFFIX}/"
    make -j "$(nproc)"
fi

sudo make install
