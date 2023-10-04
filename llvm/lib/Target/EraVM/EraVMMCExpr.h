//===-- EraVMMCExpr.h - EraVM specific MC expression classes ----*- C++ -*-===//
//
/// \file
/// Define EraVM specific MC expression classes
//
//===----------------------------------------------------------------------===//

#ifndef ERAVM_LIB_TARGET_TVM_TVMMVEXPR_H
#define ERAVM_LIB_TARGET_TVM_TVMMVEXPR_H

#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCExpr.h"

namespace llvm {

class EraVMCImmMCExpr : public MCTargetExpr {
private:
  StringRef Data;

  explicit EraVMCImmMCExpr(const StringRef &Data) : Data(Data) {}

public:
  static const EraVMCImmMCExpr *create(const StringRef &Data, MCContext &Ctx);

  /// getOpcode - Get the kind of this expression.
  StringRef getString() const { return Data; }

  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override;
  bool evaluateAsRelocatableImpl(MCValue &Res, const MCAsmLayout *Layout,
                                 const MCFixup *Fixup) const override {
    return false;
  }
  void visitUsedExpr(MCStreamer &Streamer) const override{};
  MCFragment *findAssociatedFragment() const override { return nullptr; }

  void fixELFSymbolsInTLSFixups(MCAssembler &Asm) const override {}
};

} // end namespace llvm

#endif // ERAVM_LIB_TARGET_TVM_TVMMVEXPR_H
