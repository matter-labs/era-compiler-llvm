//==--------- LLDAsLibraryC.h - LLD Public C Interface --------*- C++ -*----==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This header declares the following EVM C interfaces:
//  - 'LLVMLinkEVM'
//    The interface to the LLD linker functionality (via lld-as-a-library)
//
//  - 'LLVMIsELF'
//    Checks if the given memory buffer contains an ELF EVM object file
//
//  - 'LLVMGetUndefinedSymbolsEVM'
//    Returns an array of undefined linker symbols
//
//  - 'LLVMDisposeUndefinedSymbolsEVM'
//    Disposes an array returned by the 'LLVMGetUndefinedSymbolsEVM'
//
// These functions use a notion of the 'Linker Symbol' which is generalization
// of a usual ELF global symbol. The main difference is that 'Linker Symbol'
// has a 20-byte value, whereas the maximum value width of a usual ELF symbol
// is just 8 bytes. In order to represent a 20-byte symbol value with its
// relocation, initial 'Linker Symbol' is split into five sub-symbols
// which are usual 32-bit ELF symbols. This split is performed by the LLVM MC
// layer. For example, if the codegen needs to represent a 20-byte relocatable
// value associated with the symbol 'symbol_id', the MC layer sequentially
// (in the binary layout) emits the following undefined symbols:
//
//   '__linker_symbol_id_0'
//   '__linker_symbol_id_1'
//   '__linker_symbol_id_2'
//   '__linker_symbol_id_3'
//   '__linker_symbol_id_4'
//
//  with associated 32-bit relocations. Each sub-symbol name is formed by
//  prepending '__linker' and appending '_[0-4]'. MC layer also sets the
//  ELF::STO_EVM_LINKER_SYMBOL flag in the 'st_other' field in the symbol
//  table entry to distinguish such symbols from all others.
//  In EVM, only these symbols are allowed to be undefined in an object
//  code. All other cases must be treated as unreachable and denote a bug
//  in the FE/LLVM codegen/Linker implementation.
//  'Linker Symbols' are resolved, i.e they receive their final 20-byte
//  values, at the linkage stage when calling LLVMLinkEVM.
//  For this, the 'LLVMLinkEVM' has parameters:
//   - \p linkerSymbolNames, array of null-terminated linker symbol names
//   - \p linkerSymbolValues, array of symbol values
//
//  For example, if linkerSymbolNames[0] points to a string 'symbol_id',
//  it takes the linkerSymbolValues[0] value which is 20-byte array
//  0xAAAAAAAABB.....EEEEEEEE) and creates five symbol definitions in
//  a linker script:
//
//   "__linker_symbol_id_0" = 0xAAAAAAAA
//   "__linker_symbol_id_1" = 0xBBBBBBBB
//   "__linker_symbol_id_2" = 0xCCCCCCCC
//   "__linker_symbol_id_3" = 0xDDDDDDDD
//   "__linker_symbol_id_4" = 0xEEEEEEEE
//
//===----------------------------------------------------------------------===//

#ifndef LLD_C_LLDASLIBRARYC_H
#define LLD_C_LLDASLIBRARYC_H

#include "llvm-c/ExternC.h"
#include "llvm-c/Types.h"

LLVM_C_EXTERN_C_BEGIN

// Currently, the size of a linker symbol is limited to 20 bytes, as its the
// only usage is to represent Ethereum addresses which are of 160 bit width.
#define LINKER_SYMBOL_SIZE 20

/** Returns true if the \p inBuffer contains an EVM ELF object file. */
LLVMBool LLVMIsELFEVM(LLVMMemoryBufferRef inBuffer);

/**
 * Performs the following steps, which are based on Ethereum's
 * Assembly::assemble() logic:
 *   - Concatenates the .text sections of input ELF files referenced
 *     by __dataoffset* symbols from the first file.
 *   - Resolves undefined __dataoffset* and __datasize* symbols.
 *   - Gathers all undefined linker symbols (library references) from
 *     all files.
 *   - Ensures that the first file does not load and set
 *     immutables simultaneously.
 *
 * \p codeSegment, 0 means the first file has a deploy code,
 *                 1 - runtime code;
 * \p inBuffers - relocatable ELF files to be assembled
 * \p inBuffersIDs - their IDs
 * \p outBuffer - resulting relocatable object file */
LLVMBool LLVMAssembleEVM(uint64_t codeSegment,
                         const LLVMMemoryBufferRef inBuffers[],
                         const char *const inBuffersIDs[],
                         uint64_t inBuffersNum, LLVMMemoryBufferRef *outBuffer,
                         char **errorMessage);

/** Returns an array of undefined linker symbol names
 *  from the ELF object provided in \p inBuffer. */
void LLVMGetUndefinedReferencesEVM(LLVMMemoryBufferRef inBuffer,
                                   char ***linkerSymbols,
                                   uint64_t *numLinkerSymbols);

/** Disposes of the array of symbols returned by the
 *  'LLVMGetUndefinedReferences' function. */
void LLVMDisposeUndefinedReferences(char *symbolNames[], uint64_t numSymbols);

/** Resolves undefined linker symbols in the ELF object file \p inBuffer.
 *  Returns the ELF object file if any linker symbols remain unresolved;
 *  otherwise, returns the bytecode. */
LLVMBool LLVMLinkEVM(LLVMMemoryBufferRef inBuffer,
                     LLVMMemoryBufferRef *outBuffer,
                     const char *const *linkerSymbolNames,
                     const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
                     uint64_t numLinkerSymbols, char **errorMessage);

/** Returns the immutable symbol names and their offsets from the ELF
 *  object file provided in \p inBuffer. */
uint64_t LLVMGetImmutablesEVM(LLVMMemoryBufferRef inBuffer,
                              char ***immutableIDs,
                              uint64_t **immutableOffsets);

/** Disposes of the immutable names and their offsets returned by
 *  'LLVMGetImmutablesEVM'. */
void LLVMDisposeImmutablesEVM(char **immutableIDs, uint64_t *immutableOffsets,
                              uint64_t numOfImmutables);
LLVM_C_EXTERN_C_END

#endif // LLD_C_LLDASLIBRARYC_H
