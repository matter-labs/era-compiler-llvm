//===-- SyncVMMCExpr.cpp - SyncVM specific MC expression classes ----------===//
//
/// \file
/// Define SyncVM-specific MC classes.
//
//===----------------------------------------------------------------------===//

#include "SyncVMMCExpr.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
using namespace llvm;

#define DEBUG_TYPE "tvm-mcexpr"

const SyncVMCImmMCExpr *SyncVMCImmMCExpr::create(const StringRef &Data,
                                                 MCContext &Ctx) {
  return new (Ctx) SyncVMCImmMCExpr(Data);
}

void SyncVMCImmMCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  OS << Data;
}
