//===-- SyncVMMCExpr.h - SyncVM specific MC expression classes --*- C++ -*-===//
//
/// \file
/// Define SyncVM specific MC expression classes
//
//===----------------------------------------------------------------------===//

#ifndef SYNCVM_LIB_TARGET_TVM_TVMMVEXPR_H
#define SYNCVM_LIB_TARGET_TVM_TVMMVEXPR_H

#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCExpr.h"

namespace llvm {

class SyncVMCImmMCExpr : public MCTargetExpr {
private:
  StringRef Data;

  explicit SyncVMCImmMCExpr(const StringRef &Data) : Data(Data) {}

public:
  static const SyncVMCImmMCExpr *create(const StringRef &Data, MCContext &Ctx);

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

  static bool classof(const MCExpr *E) {
    return E->getKind() == MCExpr::Target;
  }
};

} // end namespace llvm

#endif // SYNCVM_LIB_TARGET_TVM_TVMMVEXPR_H
