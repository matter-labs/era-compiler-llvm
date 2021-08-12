//===------- SyncVMCodegenPrepare.cpp - Replace bitwise operations --------===//

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
    return "Replace bitwise operations with arithmetic ones";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    FunctionPass::getAnalysisUsage(AU);
  }
};
} // namespace

char SyncVMCodegenPrepare::ID = 0;

INITIALIZE_PASS(SyncVMCodegenPrepare, "lower-bitwise",
                "Replace bitwise operations with arithmetic ones", false, false)

/// Replace a & b with a * b for and i1
static bool replaceAnd(Instruction &I) {
  Value *LHS = I.getOperand(0);
  Value *RHS = I.getOperand(1);
  if (LHS->getType()->getIntegerBitWidth() != 1u)
    return false;
  IRBuilder<> Builder(&I);
  auto *NewI = Builder.CreateMul(LHS, RHS);
  I.replaceAllUsesWith(NewI);
  return true;
}

/// Replace a | b with a + b - a * b for i1
static bool replaceOr(Instruction &I) {
  Value *LHS = I.getOperand(0);
  Value *RHS = I.getOperand(1);
  if (LHS->getType()->getIntegerBitWidth() != 1u)
    return false;
  IRBuilder<> Builder(&I);
  LHS = Builder.CreateZExt(LHS, Builder.getIntNTy(256));
  RHS = Builder.CreateZExt(RHS, Builder.getIntNTy(256));
  auto *Mul = Builder.CreateMul(LHS, RHS);
  auto *Add = Builder.CreateAdd(LHS, RHS);
  auto *NewI = Builder.CreateSub(Add, Mul);
  NewI = Builder.CreateTrunc(NewI, Builder.getInt1Ty());
  I.replaceAllUsesWith(NewI);
  return true;
}

/// Replace a ^ b with a + b for i1
static bool replaceXor(Instruction &I) {
  Value *LHS = I.getOperand(0);
  Value *RHS = I.getOperand(1);
  if (LHS->getType()->getIntegerBitWidth() != 1u)
    return false;
  IRBuilder<> Builder(&I);
  LHS = Builder.CreateZExt(LHS, Builder.getIntNTy(256));
  RHS = Builder.CreateZExt(RHS, Builder.getIntNTy(256));
  auto *Add = Builder.CreateAdd(LHS, RHS);
  Add = Builder.CreateURem(Add, Builder.getIntN(256, 2));
  Add = Builder.CreateTrunc(Add, Builder.getInt1Ty());
  I.replaceAllUsesWith(Add);
  return true;
}

static Value *pow2Const(ConstantInt *PowValue, IRBuilder<> &Builder) {
  uint64_t Power = PowValue->getZExtValue();
  APInt Pow2(PowValue->getType()->getIntegerBitWidth(), 1,
             false /* IsSigned */);
  Pow2 <<= Power;
  return Builder.getInt(Pow2);
}
static void replaceShl(Instruction &I) {
  Value *LHS = I.getOperand(0);
  Value *RHS = I.getOperand(1);
  if (auto *RHSConst = dyn_cast<ConstantInt>(RHS)) {
    IRBuilder<> Builder(&I);
    Value *Pow2 = pow2Const(RHSConst, Builder);
    auto *Mul = Builder.CreateMul(LHS, Pow2);
    I.replaceAllUsesWith(Mul);
    return;
  }
  // TODO: Implement
  llvm_unreachable("Variable shifts are not supported yet");
}

static void replaceLShr(Instruction &I) {
  Value *LHS = I.getOperand(0);
  Value *RHS = I.getOperand(1);
  IRBuilder<> Builder(&I);
  if (auto *RHSConst = dyn_cast<ConstantInt>(RHS)) {
    IRBuilder<> Builder(&I);
    Value *Pow2 = pow2Const(RHSConst, Builder);
    auto *Div = Builder.CreateUDiv(LHS, Pow2);
    I.replaceAllUsesWith(Div);
    return;
  }
  // TODO: Implement
  llvm_unreachable("Variable shifts are not supported yet");
}

bool SyncVMCodegenPrepare::runOnFunction(Function &F) {
  std::vector<Instruction *> Replaced;
  for (auto &BB : F)
    for (auto &I : BB) {
      switch (I.getOpcode()) {
      case Instruction::And:
        if (replaceAnd(I))
          Replaced.push_back(&I);
        break;
      case Instruction::Or:
        if (replaceOr(I))
          Replaced.push_back(&I);
        break;
      case Instruction::Xor:
        if (replaceXor(I))
          Replaced.push_back(&I);
        break;
      case Instruction::Shl:
        replaceShl(I);
        Replaced.push_back(&I);
        break;
      case Instruction::LShr:
        replaceLShr(I);
        Replaced.push_back(&I);
        break;
      case Instruction::ICmp:
        auto &Cmp = cast<ICmpInst>(I);
        // unsigned cmp is ok
        if (Cmp.isUnsigned())
          break;
        CmpInst::Predicate P = Cmp.getPredicate();
        auto *CmpVal = dyn_cast<ConstantInt>(I.getOperand(1));
        if (CmpVal && (CmpVal->getValue().isNullValue()
                       || CmpVal->getValue().isOneValue())) {
          unsigned NumBits = CmpVal->getType()->getIntegerBitWidth();
          APInt Val = APInt(NumBits, -1, true).lshr(1);
          if (P == CmpInst::ICMP_SLT)
            P = CmpInst::ICMP_UGT;
          else
            break;

          if (P == CmpInst::ICMP_UGT) {
            IRBuilder<> Builder(&I);
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
          }
        }
        break;
      }
    }
  for (auto *I : Replaced)
    I->eraseFromParent();
  return !Replaced.empty();
}

FunctionPass *llvm::createSyncVMCodegenPreparePass() {
  return new SyncVMCodegenPrepare();
}
