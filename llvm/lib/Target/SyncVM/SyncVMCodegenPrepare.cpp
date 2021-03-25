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

/// Replace a & b with a * b
static void replaceAnd(Instruction &I) {
  Value *LHS = I.getOperand(0);
  Value *RHS = I.getOperand(1);
  IRBuilder<> Builder(&I);
  auto *NewI = Builder.CreateMul(LHS, RHS);
  I.replaceAllUsesWith(NewI);
}

/// Replace a | b with a + b - a * b
static void replaceOr(Instruction &I) {
  Value *LHS = I.getOperand(0);
  Value *RHS = I.getOperand(1);
  IRBuilder<> Builder(&I);
  auto *Mul = Builder.CreateMul(LHS, RHS);
  auto *Add = Builder.CreateAdd(LHS, RHS);
  auto *NewI = Builder.CreateSub(Add, Mul);
  I.replaceAllUsesWith(NewI);
}

/// Replace a ^ b with a + b - 2 * a * b
static void replaceXor(Instruction &I) {
  Value *LHS = I.getOperand(0);
  Value *RHS = I.getOperand(1);
  IRBuilder<> Builder(&I);
  auto *Mul = Builder.CreateMul(LHS, RHS);
  Mul = Builder.CreateMul(Mul, Builder.getIntN(256, 2));
  auto *Add = Builder.CreateAdd(LHS, RHS);
  auto *NewI = Builder.CreateSub(Add, Mul);
  I.replaceAllUsesWith(NewI);
}

static Value* pow2Const(ConstantInt *PowValue, IRBuilder<> &Builder) {
  uint64_t Power = PowValue->getZExtValue();
  APInt Pow2(PowValue->getType()->getIntegerBitWidth(),
             1, false /* IsSigned */);
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
        replaceAnd(I);
        Replaced.push_back(&I);
        break;
      case Instruction::Or:
        replaceOr(I);
        Replaced.push_back(&I);
        break;
      case Instruction::Xor:
        replaceXor(I);
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
      }
    }
  for (auto *I : Replaced)
    I->eraseFromParent();
  return !Replaced.empty();
}

FunctionPass *llvm::createSyncVMCodegenPreparePass() {
  return new SyncVMCodegenPrepare();
}
