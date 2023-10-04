//===-- EraVMMCExpr.cpp - EraVM specific MC expression classes ----------===//
//
/// \file
/// Define EraVM-specific MC classes.
//
//===----------------------------------------------------------------------===//

#include "EraVMMCExpr.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
using namespace llvm;

#define DEBUG_TYPE "tvm-mcexpr"

const EraVMCImmMCExpr *EraVMCImmMCExpr::create(const StringRef &Data,
                                               MCContext &Ctx) {
  return new (Ctx) EraVMCImmMCExpr(Data);
}

void EraVMCImmMCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  OS << Data;
}
