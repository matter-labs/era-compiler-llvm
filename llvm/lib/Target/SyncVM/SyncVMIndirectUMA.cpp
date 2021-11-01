//===------ SyncVMIndirectUMA.cpp - Wrap an UMA into a function call ------===//

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
FunctionPass *createIndirectUMA();
void initializeSyncVMIndirectUMAPass(PassRegistry &);
} // namespace llvm

namespace {
struct SyncVMIndirectUMA : public ModulePass {
public:
  static char ID;
  SyncVMIndirectUMA() : ModulePass(ID) {
    initializeSyncVMIndirectUMAPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Revert usopported instcombine changes";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }
};
} // namespace

char SyncVMIndirectUMA::ID = 0;
static const std::string UMLoadFunName = "__unaligned_load";
static const std::string UMStoreFunName = "__unaligned_store";

INITIALIZE_PASS(SyncVMIndirectUMA, "syncvm-indirectuma",
                "Wrap an UMA into a fuction call", false, false)

bool SyncVMIndirectUMA::runOnModule(Module &M) {
  bool Changed = false;
  std::vector<LoadInst *> UMLoads;
  std::vector<StoreInst *> UMStores;
  for (auto &F : M)
    for (auto &BB : F)
      for (auto &I : BB) {
        if (auto *Load = dyn_cast<LoadInst>(&I)) {
          unsigned Alignment = Load->getAlign().value();
          Value *Ptr = Load->getPointerOperand();
          bool PtrAligned = [Ptr]() {
            if (auto CIPtr = dyn_cast<ConstantInt>(Ptr))
              return CIPtr->getValue().urem(32) == 0;
            return false;
          }();
          if (!PtrAligned && Alignment % 32 != 0) {
            assert(Load->getPointerAddressSpace() == 1 &&
                   "Unexpected unaligned load");
            UMLoads.push_back(Load);
          }
        } else if (auto *Store = dyn_cast<StoreInst>(&I)) {
          unsigned Alignment = Store->getAlign().value();
          Value *Ptr = Store->getPointerOperand();
          bool PtrAligned = [Ptr]() {
            if (auto CIPtr = dyn_cast<ConstantInt>(Ptr))
              return CIPtr->getValue().urem(32) == 0;
            return false;
          }();
          if (!PtrAligned && Alignment % 32 != 0) {
            assert(Store->getPointerAddressSpace() == 1 &&
                   "Unexpected unaligned store");
            UMStores.push_back(Store);
          }
        }
      }

  LLVMContext &C = M.getContext();
  Type *Int256Ty = Type::getInt256Ty(C);
  Type *Int256AS1PtrTy = PointerType::getInt256PtrTy(C, 1);

  if (!UMLoads.empty()) {
    std::vector<Type *> ArgTy = {Int256AS1PtrTy};
    M.getOrInsertFunction(UMLoadFunName,
                          FunctionType::get(Int256Ty, ArgTy, false));
    Function *UMLoadFunc = M.getFunction(UMLoadFunName);
    auto *Entry = BasicBlock::Create(C, "entry", UMLoadFunc);
    IRBuilder<> Builder(Entry);
    auto *LdResult =
        Builder.CreateAlignedLoad(Int256Ty, UMLoadFunc->getArg(0), Align(1));
    Builder.CreateRet(LdResult);
    for (LoadInst *I : UMLoads) {
      Builder.SetInsertPoint(I);
      unsigned LoadBitWidth = I->getType()->getIntegerBitWidth();
      Value *LoadRes = Builder.CreateCall(UMLoadFunc, {I->getPointerOperand()});
      if (LoadBitWidth != 256) {
        assert(LoadBitWidth < 256);
        LoadRes = Builder.CreateIntCast(LoadRes,
                                        Builder.getIntNTy(LoadBitWidth), false);
      }
      I->replaceAllUsesWith(LoadRes);
      I->eraseFromParent();
    }
  }

  if (!UMStores.empty()) {
    std::vector<Type *> ArgTy = {Int256Ty, Int256AS1PtrTy};
    M.getOrInsertFunction(UMStoreFunName,
                          FunctionType::get(Type::getVoidTy(C), ArgTy, false));
    Function *UMStoreFunc = M.getFunction(UMStoreFunName);
    auto *Entry = BasicBlock::Create(C, "entry", UMStoreFunc);
    IRBuilder<> Builder(Entry);
    Builder.CreateAlignedStore(UMStoreFunc->getArg(0), UMStoreFunc->getArg(1),
                               Align(1));
    Builder.CreateRetVoid();
    for (StoreInst *I : UMStores) {
      Builder.SetInsertPoint(I);
      unsigned StoreBitWidth =
          I->getValueOperand()->getType()->getIntegerBitWidth();
      auto *StoreVal = [&]() {
        if (StoreBitWidth != 256) {
          assert(StoreBitWidth < 256);
          Value *CellValue =
              Builder.CreateLoad(Int256Ty, I->getPointerOperand());
          Value *Mask =
              Builder.getInt(APInt(-1, 256, true).lshr(StoreBitWidth));
          CellValue = Builder.CreateAnd(CellValue, Mask);
          auto *StoreValue =
              Builder.CreateIntCast(I->getValueOperand(), Int256Ty, false);
          StoreValue = Builder.CreateShl(StoreValue, 256 - StoreBitWidth);
          StoreValue = Builder.CreateOr(CellValue, StoreValue);
          return StoreValue;
        }
        return I->getValueOperand();
      }();
      Builder.CreateCall(UMStoreFunc, {StoreVal, I->getPointerOperand()});
      I->eraseFromParent();
    }
  }

  return Changed;
}

ModulePass *llvm::createSyncVMIndirectUMAPass() {
  return new SyncVMIndirectUMA();
}
