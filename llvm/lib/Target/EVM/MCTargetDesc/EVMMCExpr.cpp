//===-------- EVMMCExpr.cpp - EVM specific MC expression classes ----------===//
//
// Define EVM-specific MC classes.
//
//===----------------------------------------------------------------------===//

#include "EVMMCExpr.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
using namespace llvm;

#define DEBUG_TYPE "evm-mcexpr"

const EVMCImmMCExpr *EVMCImmMCExpr::create(const StringRef &Data,
                                           MCContext &Ctx) {
  return new (Ctx) EVMCImmMCExpr(Data);
}

void EVMCImmMCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  OS << Data;
}
