//===-------- SyncVMLowerBitwise.cpp - Replace bitwise operations ---------===//

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
FunctionPass *createSyncVMLowerBitwise();
void initializeSyncVMLowerBitwisePass(PassRegistry &);
} // namespace llvm

namespace {
struct SyncVMLowerBitwise : public FunctionPass {
public:
  static char ID;
  SyncVMLowerBitwise() : FunctionPass(ID) {
    initializeSyncVMLowerBitwisePass(*PassRegistry::getPassRegistry());
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

char SyncVMLowerBitwise::ID = 0;

INITIALIZE_PASS(SyncVMLowerBitwise, "lower-bitwise",
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

bool SyncVMLowerBitwise::runOnFunction(Function &F) {
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
      }
    }
  for (auto *I : Replaced)
    I->eraseFromParent();
  return !Replaced.empty();
}

FunctionPass *llvm::createSyncVMLowerBitwise() {
  return new SyncVMLowerBitwise();
}
