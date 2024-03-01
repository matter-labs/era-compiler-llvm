//===- lld/Common/Driver.h - Linker Driver Emulator -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLD_COMMON_DRIVER_H
#define LLD_COMMON_DRIVER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/raw_ostream.h"

namespace lld {
enum Flavor {
  Invalid,
  Gnu,     // -flavor gnu
  MinGW,   // -flavor gnu MinGW
  WinLink, // -flavor link
  Darwin,  // -flavor darwin
  Wasm,    // -flavor wasm
};

using Driver = bool (*)(llvm::ArrayRef<const char *>, llvm::raw_ostream &,
                        llvm::raw_ostream &, bool, bool);

// EraVM local begin
using DriverMemBuf = bool (*)(llvm::ArrayRef<llvm::MemoryBufferRef> inBuffers,
                              llvm::raw_pwrite_stream *outBuffer,
                              llvm::ArrayRef<const char *>, llvm::raw_ostream &,
                              llvm::raw_ostream &, bool, bool);
// EraVM local end

struct DriverDef {
  Flavor f;
  Driver d;
};

// EraVM local begin
struct DriverDefMemBuf {
  Flavor f;
  DriverMemBuf d;
};
// EraVM local begin
struct Result {
  int retCode;
  bool canRunAgain;
};

// Generic entry point when using LLD as a library, safe for re-entry, supports
// crash recovery. Returns a general completion code and a boolean telling
// whether it can be called again. In some cases, a crash could corrupt memory
// and re-entry would not be possible anymore. Use exitLld() in that case to
// properly exit your application and avoid intermittent crashes on exit caused
// by cleanup.
Result lldMain(llvm::ArrayRef<const char *> args, llvm::raw_ostream &stdoutOS,
               llvm::raw_ostream &stderrOS, llvm::ArrayRef<DriverDef> drivers);
// EraVM local begin
Result lldMainMemBuf(llvm::ArrayRef<llvm::MemoryBufferRef> inBuffers,
                     llvm::raw_pwrite_stream *outBuffer,
                     llvm::ArrayRef<const char *> args,
                     llvm::raw_ostream &stdoutOS, llvm::raw_ostream &stderrOS,
                     llvm::ArrayRef<DriverDefMemBuf> drivers);
// EraVM local end
} // namespace lld

// With this macro, library users must specify which drivers they use, provide
// that information to lldMain() in the `drivers` param, and link the
// corresponding driver library in their executable.
#define LLD_HAS_DRIVER(name)                                                   \
  namespace lld {                                                              \
  namespace name {                                                             \
  bool link(llvm::ArrayRef<const char *> args, llvm::raw_ostream &stdoutOS,    \
            llvm::raw_ostream &stderrOS, bool exitEarly, bool disableOutput);  \
  }                                                                            \
  }

// EraVM local begin
#define LLD_HAS_DRIVER_MEM_BUF(name)                                           \
  namespace lld {                                                              \
  namespace name {                                                             \
  bool linkMemBuf(llvm::ArrayRef<llvm::MemoryBufferRef> inBuffers,             \
                  llvm::raw_pwrite_stream *outBuffer,                          \
                  llvm::ArrayRef<const char *> args,                           \
                  llvm::raw_ostream &stdoutOS, llvm::raw_ostream &stderrOS,    \
                  bool exitEarly, bool disableOutput);                         \
  }                                                                            \
  }
// EraVM local end

// An array which declares that all LLD drivers are linked in your executable.
// Must be used along with LLD_HAS_DRIVERS. See examples in LLD unittests.
#define LLD_ALL_DRIVERS                                                        \
  {                                                                            \
    {lld::WinLink, &lld::coff::link}, {lld::Gnu, &lld::elf::link},             \
        {lld::MinGW, &lld::mingw::link}, {lld::Darwin, &lld::macho::link}, {   \
      lld::Wasm, &lld::wasm::link                                              \
    }                                                                          \
  }

// EraVM local begin
#define LLD_ALL_DRIVERS_MEM_BUF                                                \
  {                                                                            \
    { lld::Gnu, &lld::elf::linkMemBuf }                                        \
  }
// EraVM local end
#endif
