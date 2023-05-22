//===-------- EVMMCExpr.h - EVM specific MC expression classes --*- C++ -*-===//
//
// Define EVM specific MC expression classes
//
//===----------------------------------------------------------------------===//

#ifndef EVM_LIB_TARGET_TVM_TVMMVEXPR_H
#define EVM_LIB_TARGET_TVM_TVMMVEXPR_H

#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCExpr.h"

namespace llvm {

class EVMCImmMCExpr final : public MCTargetExpr {
private:
  StringRef Data;

  explicit EVMCImmMCExpr(const StringRef &Data) : Data(Data) {}

public:
  static const EVMCImmMCExpr *create(const StringRef &Data, MCContext &Ctx);

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

#endif // EVM_LIB_TARGET_TVM_TVMMVEXPR_H
