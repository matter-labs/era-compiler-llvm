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
      case Instruction::Call: {
        // TODO: Link the runtime earlier instead of specifying cryptic
        // knowledge here.
        auto &Call = cast<CallInst>(I);
        Function *Callee = Call.getCalledFunction();
        if (!Callee && isa<BitCastOperator>(Call.getCalledOperand()))
          Callee = dyn_cast<Function>(
              cast<BitCastOperator>(Call.getCalledOperand())->getOperand(0));
        if (Callee && Callee->hasName()) {
          if (Callee->getName() == "__memset_uma_as1" &&
              isa<ConstantInt>(Call.getOperand(2)) &&
              (cast<ConstantInt>(Call.getOperand(2))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          } else if (Callee->getName() == "__memset_uma_as2" &&
              isa<ConstantInt>(Call.getOperand(2)) &&
              (cast<ConstantInt>(Call.getOperand(2))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          } else if (Callee->getName() == "__small_store_as1" &&
                     isa<ConstantInt>(Call.getOperand(2)) &&
                     (cast<ConstantInt>(Call.getOperand(2))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          } else if (Callee->getName() == "__small_store_as2" &&
                     isa<ConstantInt>(Call.getOperand(2)) &&
                     (cast<ConstantInt>(Call.getOperand(2))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          } else if (Callee->getName() == "__small_store_as0" &&
                     isa<ConstantInt>(Call.getOperand(1)) &&
                     (cast<ConstantInt>(Call.getOperand(1))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          }
        }
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
