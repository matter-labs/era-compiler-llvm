//===-- SyncVMCodegenPrepare.cpp - Revert unsupported instcombine changes -===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsHexagon.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"

#include "SyncVM.h"

using namespace llvm;

namespace llvm {
FunctionPass *createSyncVMCodegenPrepare();
void initializeSyncVMCodegenPreparePass(PassRegistry &);
} // namespace llvm

namespace {
struct SyncVMCodegenPrepare : public FunctionPass {
public:
  static char ID;
  SyncVMCodegenPrepare() : FunctionPass(ID) {
    initializeSyncVMCodegenPreparePass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override {
    return "Revert usopported instcombine changes";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    FunctionPass::getAnalysisUsage(AU);
  }
};
} // namespace

char SyncVMCodegenPrepare::ID = 0;

INITIALIZE_PASS(SyncVMCodegenPrepare, "syncvm-codegen-prepare",
                "Revert usopported instcombine changes", false, false)

bool SyncVMCodegenPrepare::runOnFunction(Function &F) {
  bool Changed = false;
  std::vector<Instruction *> Replaced;
  for (auto &BB : F)
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      auto &I = *II;
      switch (I.getOpcode()) {
      default:
        break;
      case Instruction::ICmp: {
        auto &Cmp = cast<ICmpInst>(I);
        IRBuilder<> Builder(&I);
        // unsigned cmp is ok
        if (Cmp.isUnsigned())
          break;
        CmpInst::Predicate P = Cmp.getPredicate();
        auto *CmpVal = dyn_cast<ConstantInt>(I.getOperand(1));
        if (CmpVal && (CmpVal->getValue().isNullValue() ||
                       CmpVal->getValue().isOneValue())) {
          unsigned NumBits = CmpVal->getType()->getIntegerBitWidth();
          APInt Val = APInt(NumBits, -1, true).lshr(1);
          if (P == CmpInst::ICMP_SLT)
            P = CmpInst::ICMP_UGT;
          else
            break;

          if (P == CmpInst::ICMP_UGT) {
            auto Val256 =
                Builder.CreateZExt(Builder.getInt(Val), Builder.getIntNTy(256));
            auto Op256 =
                Builder.CreateZExt(I.getOperand(0), Builder.getIntNTy(256));
            auto *NewCmp = Builder.CreateICmp(P, Op256, Val256);
            if (CmpVal->getValue().isOneValue()) {
              auto *Cmp0 =
                  Builder.CreateICmp(CmpInst::ICMP_EQ, Op256,
                                     Builder.getInt(APInt(256, 0, false)));
              NewCmp = Builder.CreateOr(NewCmp, Cmp0);
            }
            I.replaceAllUsesWith(NewCmp);
            Replaced.push_back(&I);
            Changed = true;
          }
        }
        break;
      }
      // SyncVM has different semantic for shifts:
      // x SHL y == x shl (y % 256),
      // x LSHR y == x lshr (y % 256)
      // So lshr is to be replaced with:
      // (y >= 256) ? 0 : x lshr y
      case Instruction::LShr:
      case Instruction::Shl: {
        IRBuilder<> Builder(&BB, std::next(II));
        unsigned Size = II->getType()->getIntegerBitWidth();
        auto *Const0 = Builder.getInt(APInt(Size, 0, false));
        auto *Const255 = Builder.getInt(APInt(Size, 255, false));
        auto *Icmp = Builder.CreateICmpUGT(I.getOperand(1), Const255);
        auto *Select = Builder.CreateSelect(Icmp, Const0, &I);
        I.replaceUsesWithIf(Select,
                            [Select](Use &U) { return U.getUser() != Select; });
        Changed = true;
        break;
      }
      }
    }
  for (auto *I : Replaced)
    I->eraseFromParent();
  return Changed;
}

FunctionPass *llvm::createSyncVMCodegenPreparePass() {
  return new SyncVMCodegenPrepare();
}
