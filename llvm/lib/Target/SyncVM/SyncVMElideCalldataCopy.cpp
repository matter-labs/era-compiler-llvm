//===-- SyncVMElideCalldataCopy.cpp - Remove calldata copy to heap -------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsHexagon.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"

#include "SyncVM.h"
#include <deque>

using namespace llvm;

#define DEBUG_TYPE "syncvm-elide-calldata-copy"
#define SYNCVM_ELIDE_CALLDATA_COPY "SyncVM elide calldata copy"

namespace llvm {
ModulePass *createSyncVMElideCalldataCopy();
void initializeSyncVMElideCalldataCopyPass(PassRegistry &);
} // namespace llvm

namespace {
/// Implement speculative calldata copy elision transformation.
/// If calldata is copied to the heap and then transmitted to a far call, the
/// pass elides it. It's assumed, that the region of heap has no usage
/// otherwise; the frontend shall guarantee it.
/// TODO: Move to MLIR, or frontend.
struct SyncVMElideCalldataCopy : public ModulePass {
public:
  static char ID;
  SyncVMElideCalldataCopy() : ModulePass(ID) {
    initializeSyncVMElideCalldataCopyPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override { return SYNCVM_ELIDE_CALLDATA_COPY; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

private:
  Module *TheModule;
  bool canElideCalldataCopy() const;
};
} // namespace

char SyncVMElideCalldataCopy::ID = 0;

INITIALIZE_PASS(SyncVMElideCalldataCopy, "syncvm-codegen-prepare",
                SYNCVM_ELIDE_CALLDATA_COPY, false, false)

struct ABIData {
  Value *CalldataOffset;
  Value *CalldataLength;
};

/// Look for copying calldata to the heap.
/// Traverse operand 0 use chain for a given instruction and look for pointer to
/// calldata, then find memcpy among its users.
static MemCpyInst *findMemCpy(Instruction &I) {
  if (auto *Load = dyn_cast<LoadInst>(&I)) {
    if (Load->getPointerAddressSpace() != SyncVMAS::AS_CALLDATA)
      return nullptr;
    Value *CalldataPtr = Load->getOperand(0);
    MemCpyInst *MCpy = nullptr;
    for (Value *CalldataUser : CalldataPtr->users())
      if ((MCpy = dyn_cast<MemCpyInst>(CalldataUser)))
        break;
    return MCpy;
  }
  if (I.getOpcode() != Instruction::Or && I.getOpcode() != Instruction::And &&
      I.getOpcode() != Instruction::Shl)
    return nullptr;
  if (I.getNumOperands() < 1 || !isa<Instruction>(I.getOperand(0)))
    return nullptr;
  return findMemCpy(*cast<Instruction>(I.getOperand(0)));
}

/// Extract calldata offset and length from ABI data far call argument.
/// TODO: It's assumed that bitwise operations and shifts are not folded to a
/// constant.
static bool analyzeABIData(Value &V, unsigned ShiftAmount, ABIData &Result) {
  if (auto *ConstInt = dyn_cast<ConstantInt>(&V)) {
    if (ShiftAmount == 0) {
      assert(!Result.CalldataOffset && "Calldata offset is already set");
      Result.CalldataOffset = ConstInt;
      return true;
    } else if (ShiftAmount == 64) {
      assert(!Result.CalldataLength && "Calldata length is already set");
      Result.CalldataLength = ConstInt;
      return true;
    }
    // The higher part of ABI data could be arbitrary
    return false;
  }
  auto *I = dyn_cast<Instruction>(&V);
  if (!I)
    return false;
  if (I->getOpcode() == Instruction::Or)
    return analyzeABIData(*I->getOperand(0), ShiftAmount, Result) &&
           analyzeABIData(*I->getOperand(1), ShiftAmount, Result);
  if (I->getOpcode() == Instruction::And &&
      isa<ConstantInt>(I->getOperand(1))) {
    // TODO: Masks are not expected to apply to calldata offset or length, and
    // the pass doesn't care about the rest.
    if (ShiftAmount == 0)
      return false;
    if (ShiftAmount == 64 &&
        cast<ConstantInt>(I->getOperand(1))->getZExtValue() !=
            0x00000000ffffffff &&
        cast<ConstantInt>(I->getOperand(1))->getZExtValue() !=
            0xffffffff00000000)
      return false;
    return analyzeABIData(*I->getOperand(0), ShiftAmount, Result);
  }
  if (I->getOpcode() == Instruction::Shl && isa<ConstantInt>(I->getOperand(1)))
    return analyzeABIData(
        *I->getOperand(0),
        ShiftAmount + cast<ConstantInt>(I->getOperand(1))->getZExtValue(),
        Result);
  if (auto *Load = dyn_cast<LoadInst>(I)) {
    if (ShiftAmount == 64) {
      assert(!Result.CalldataLength && "Calldata length is already set");
      Result.CalldataLength = I;
      return true;
    }
  }
  // Unexpected instruction, but it's ok if it handles 96 bit onwards.
  return ShiftAmount >= 96;
}

/// BFS of a path between DefVal and UseVal.
static std::vector<Value *> findDefUseChainBetween(Value *DefVal,
                                                   Instruction *UseVal) {
  assert(DefVal);
  assert(UseVal);
  DenseMap<Value *, std::vector<Value *>> Visited;
  std::deque<std::tuple<Value *, Value *>> Worklist = {{DefVal, nullptr}};
  Visited[nullptr] = {};
  Value *CurVal = nullptr;
  do {
    auto CurrentValuePath = Worklist.front();
    Worklist.pop_front();
    CurVal = std::get<0>(CurrentValuePath);
    Value *CurPath = std::get<1>(CurrentValuePath);
    if (!Visited.count(CurVal)) {
      for (User *U : CurVal->users())
        Worklist.emplace_back(U, CurVal);
      Visited[CurVal] = Visited[CurPath];
      if (CurPath)
        Visited[CurVal].push_back(CurPath);
    }
    if (CurVal == UseVal)
      return Visited[CurVal];
  } while (!Worklist.empty());
  return {};
}

static const Value *getUnderlyingAddress(const Value &V) {
  auto *IntToPtrOp = dyn_cast<Operator>(&V);
  if (IntToPtrOp)
    return IntToPtrOp->getOperand(0);
  if (auto *Const = dyn_cast<Constant>(&V))
    return Const;
  auto *IntToPtrInst = dyn_cast<Instruction>(&V);
  if (!IntToPtrInst)
    return nullptr;
  if (IntToPtrInst->getOpcode() != Instruction::IntToPtr)
    return nullptr;
  return IntToPtrInst->getOperand(0);
}

static bool areLoadsFromTheSameAddress(const Value &V1, const Value &V2) {
  auto *Ld1 = dyn_cast<LoadInst>(&V1);
  auto *Ld2 = dyn_cast<LoadInst>(&V2);
  if (!Ld1 || !Ld2)
    return false;
  if (Ld1->getPointerAddressSpace() != Ld2->getPointerAddressSpace())
    return false;
  return getUnderlyingAddress(V1) == getUnderlyingAddress(V2);
}

bool SyncVMElideCalldataCopy::runOnModule(Module &M) {
  bool Changed = false;
  TheModule = &M;
  for (Function &F : M)
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *CB = dyn_cast<CallBase>(&I)) {
          if (CB->getCalledFunction()->getName() == "__farcall") {
            // Ignore constant contract addresses.
            auto *ContractAddressVal = dyn_cast<Instruction>(CB->getOperand(0));
            if (!ContractAddressVal)
              continue;

            // If calldata is not copied, the copy can not be elided.
            MemCpyInst *MemCpy = findMemCpy(*ContractAddressVal);
            if (!MemCpy)
              continue;

            // Try to extract calldata length and offset from ABI data argument.
            ABIData TheABIData = {};
            if (!analyzeABIData(*CB->getOperand(1), 0, TheABIData))
              continue;
            if (!TheABIData.CalldataLength)
              continue;

            // Find the def-use chain between the calldata offset and the far
            // call.
            std::vector<Value *> UseChain =
                TheABIData.CalldataOffset
                    ? findDefUseChainBetween(TheABIData.CalldataOffset, CB)
                    : std::vector<Value *>{};
            if (TheABIData.CalldataOffset && UseChain.size() <= 1)
              continue;

            const Value *MemCpyDstAddr =
                getUnderlyingAddress(*MemCpy->getOperand(0));
            // Check that calldata copy is transmitted to the far call.
            if (TheABIData.CalldataOffset &&
                MemCpyDstAddr != TheABIData.CalldataOffset)
              continue;
            if (!TheABIData.CalldataOffset &&
                !(isa<Constant>(MemCpyDstAddr) &&
                  dyn_cast<Constant>(MemCpyDstAddr)->isZeroValue()))
              continue;
            if (MemCpy->getOperand(2) != TheABIData.CalldataLength &&
                !areLoadsFromTheSameAddress(*MemCpy->getOperand(2),
                                            *TheABIData.CalldataLength))
              continue;

            // Change calldata offset.
            if (TheABIData.CalldataOffset) {
              IRBuilder<> Builder(dyn_cast<Instruction>(UseChain[1]));
              Value *Replacement = Builder.getIntN(256, 0);
              UseChain[0]->replaceUsesWithIf(
                  Replacement, [&UseChain](const Use &U) {
                    return U.getUser() == UseChain[1];
                  });
            }

            // Set forwarding flag.
            IRBuilder<> Builder(CB);
            auto *OrInst =
                Builder.CreateOr(CB->getOperand(1),
                                 Builder.getInt(APInt(256, 1, false).shl(200)));
            CB->getOperand(1)->replaceUsesWithIf(
                OrInst, [CB](Use &U) { return U.getUser() == CB; });

            MemCpy->eraseFromParent();
            Changed = true;
          }
        }
  return Changed;
}

ModulePass *llvm::createSyncVMElideCalldataCopyPass() {
  return new SyncVMElideCalldataCopy();
}
