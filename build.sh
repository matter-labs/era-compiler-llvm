#!/usr/bin/env bash

set -ex

ROOT_DIR="${PWD}"

MUSL_NAME='musl-1.2.3'
MUSL_BUILD="${ROOT_DIR}/${MUSL_NAME}/build/"
MUSL_INSTALL="${HOME}/opt/musl/"

CRT_BUILD="${ROOT_DIR}/build-crt/"
CRT_INSTALL="${ROOT_DIR}/opt/crt-musl/"

HOST_BUILD="${ROOT_DIR}/build-host/"
HOST_INSTALL="${HOME}/opt/llvm-musl-host/"

TARGET_BUILD="${ROOT_DIR}/build-target/"
TARGET_INSTALL="${HOME}/opt/llvm-release/"



wget "https://git.musl-libc.org/cgit/musl/snapshot/${MUSL_NAME}.tar.gz"
tar -xvzf "${MUSL_NAME}.tar.gz"

mkdir -pv "${MUSL_BUILD}"
cd "${MUSL_BUILD}"
../configure \
    --prefix="${MUSL_INSTALL}" \
    --syslibdir="${MUSL_INSTALL}/lib/" \
    --enable-wrapper='clang'
cd "${MUSL_BUILD}"
make -j "$(nproc)"
make install
cd "${ROOT_DIR}"

mkdir -pv "${MUSL_INSTALL}/include/asm"
types_h="${MUSL_INSTALL}/include/asm/types.h"
cp -rfv '/usr/include/linux' "${MUSL_INSTALL}/include/"
cp -rfv /usr/include/asm-generic/* "${MUSL_INSTALL}/include/asm/"
sed -i 's/asm-generic/asm/' "${types_h}"



cmake './llvm' \
  -B "${CRT_BUILD}" \
  -G 'Ninja' \
  -DCMAKE_C_COMPILER='clang' \
  -DCMAKE_CXX_COMPILER='clang++' \
  -DCMAKE_INSTALL_PREFIX="${CRT_INSTALL}" \
  -DCMAKE_BUILD_TYPE='Release' \
  -DLLVM_ENABLE_PROJECTS='compiler-rt' \
  -DLLVM_TARGETS_TO_BUILD='X86' \
  -DLLVM_DEFAULT_TARGET_TRIPLE='x86_64-pc-linux-musl' \
  -DLLVM_BUILD_DOCS='Off' \
  -DLLVM_BUILD_TESTS='Off' \
  -DLLVM_INCLUDE_DOCS='Off' \
  -DLLVM_INCLUDE_TESTS='Off' \
  -DLLVM_ENABLE_ASSERTIONS='Off' \
  -DLLVM_ENABLE_DOXYGEN='Off' \
  -DLLVM_ENABLE_SPHINX='Off' \
  -DLLVM_ENABLE_OCAMLDOC='Off' \
  -DLLVM_ENABLE_BINDINGS='Off' \
  -DLLVM_ENABLE_TERMINFO='Off' \
  -DCOMPILER_RT_DEFAULT_TARGET_ARCH='x86_64' \
  -DCOMPILER_RT_BUILD_CRT='On' \
  -DCOMPILER_RT_BUILD_SANITIZERS='Off' \
  -DCOMPILER_RT_BUILD_XRAY='Off' \
  -DCOMPILER_RT_BUILD_LIBFUZZER='Off' \
  -DCOMPILER_RT_BUILD_PROFILE='Off' \
  -DCOMPILER_RT_BUILD_MEMPROF='Off' \
  -DCOMPILER_RT_BUILD_ORC='Off' \
  -DCOMPILER_RT_BUILD_GWP_ASAN='Off'
ninja -C "${CRT_BUILD}" install-crt



cmake './llvm' \
  -B "${HOST_BUILD}" \
  -G 'Ninja' \
  -DDEFAULT_SYSROOT="${MUSL_INSTALL}" \
  -DBUILD_SHARED_LIBS='Off' \
  -DCMAKE_C_COMPILER='clang' \
  -DCMAKE_CXX_COMPILER='clang++' \
  -DCMAKE_INSTALL_PREFIX="${HOST_INSTALL}" \
  -DCMAKE_BUILD_TYPE='Release' \
  -DCLANG_DEFAULT_CXX_STDLIB='libc++' \
  -DCLANG_DEFAULT_RTLIB='compiler-rt' \
  -DLLVM_DEFAULT_TARGET_TRIPLE='x86_64-pc-linux-musl' \
  -DLLVM_TARGETS_TO_BUILD='X86' \
  -DLLVM_BUILD_DOCS='Off' \
  -DLLVM_BUILD_TESTS='Off' \
  -DLLVM_INCLUDE_DOCS='Off' \
  -DLLVM_INCLUDE_TESTS='Off' \
  -DLLVM_ENABLE_PROJECTS='clang;lld' \
  -DLLVM_ENABLE_RUNTIMES='compiler-rt;libcxx;libcxxabi;libunwind' \
  -DLLVM_ENABLE_ASSERTIONS='Off' \
  -DLLVM_ENABLE_DOXYGEN='Off' \
  -DLLVM_ENABLE_SPHINX='Off' \
  -DLLVM_ENABLE_OCAMLDOC='Off' \
  -DLLVM_ENABLE_BINDINGS='Off' \
  -DLLVM_ENABLE_TERMINFO='Off' \
  -DLIBCXX_CXX_ABI='libcxxabi' \
  -DLIBCXX_HAS_MUSL_LIBC='On' \
  -DLIBCXX_ENABLE_SHARED='Off' \
  -DLIBCXX_ENABLE_STATIC='On'  \
  -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY='On' \
  -DLIBCXXABI_ENABLE_SHARED='Off' \
  -DLIBCXXABI_ENABLE_STATIC='On'  \
  -DLIBCXXABI_ENABLE_STATIC_UNWINDER='On' \
  -DLIBCXXABI_USE_LLVM_UNWINDER='On' \
  -DLIBCXXABI_USE_COMPILER_RT='On' \
  -DLIBUNWIND_ENABLE_STATIC='On' \
  -DLIBUNWIND_ENABLE_SHARED='Off' \
  -DCOMPILER_RT_BUILD_CRT='On' \
  -DCOMPILER_RT_BUILD_SANITIZERS='Off' \
  -DCOMPILER_RT_BUILD_XRAY='Off' \
  -DCOMPILER_RT_BUILD_LIBFUZZER='Off' \
  -DCOMPILER_RT_BUILD_PROFILE='Off' \
  -DCOMPILER_RT_BUILD_MEMPROF='Off' \
  -DCOMPILER_RT_BUILD_ORC='Off' \
  -DCOMPILER_RT_DEFAULT_TARGET_ARCH='x86_64' \
  -DCOMPILER_RT_DEFAULT_TARGET_ONLY='On'
cp -rfv "${CRT_INSTALL}/lib/"* "${HOST_BUILD}/lib/"
ninja -C "${HOST_BUILD}" install



cmake './llvm' \
  -B "${TARGET_BUILD}" \
  -G 'Ninja' \
  -DBUILD_SHARED_LIBS='Off' \
  -DCMAKE_C_COMPILER="${HOST_INSTALL}/bin/clang" \
  -DCMAKE_CXX_COMPILER="${HOST_INSTALL}/bin/clang++" \
  -DCMAKE_INSTALL_PREFIX="${TARGET_INSTALL}" \
  -DCMAKE_BUILD_TYPE='Release' \
  -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
  -DCMAKE_EXE_LINKER_FLAGS='-fuse-ld=lld -static' \
  -DLLVM_TARGETS_TO_BUILD='SyncVM' \
  -DLLVM_BUILD_DOCS='Off' \
  -DLLVM_BUILD_TESTS='Off' \
  -DLLVM_INCLUDE_DOCS='Off' \
  -DLLVM_INCLUDE_TESTS='Off' \
  -DLLVM_ENABLE_PROJECTS='llvm' \
  -DLLVM_ENABLE_ASSERTIONS='Off' \
  -DLLVM_ENABLE_DOXYGEN='Off' \
  -DLLVM_ENABLE_SPHINX='Off' \
  -DLLVM_ENABLE_OCAMLDOC='Off' \
  -DLLVM_ENABLE_BINDINGS='Off' \
  -DLLVM_ENABLE_TERMINFO='Off' \
  -DLLVM_ENABLE_PIC='Off'
ninja -C "${TARGET_BUILD}" install
