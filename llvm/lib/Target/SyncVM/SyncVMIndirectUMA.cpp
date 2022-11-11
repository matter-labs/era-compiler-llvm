//===------ SyncVMIndirectUMA.cpp - Wrap an UMA into a function call ------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"

#include "SyncVM.h"

using namespace llvm;

namespace llvm {
ModulePass *createIndirectUMAPass();
void initializeSyncVMIndirectUMAPass(PassRegistry &);
} // namespace llvm

namespace {
/// Reduce the binary size by moving unaligned memory access (UMA) handling to
/// a separate function and replace an UMA with its call.
/// UMA is a very expensive operation in SyncVM, so to prevent code from being
/// bloat, the pass creates a corresponding load function if a module has read
/// access to addrspace(n) memory, where n belongs to 1..3. Note that UMA in
/// stack is prohibited. For UMA store operations, the pass creates both load
/// and store functions for each triggered address space.
struct SyncVMIndirectUMA : public ModulePass {
public:
  static char ID;
  SyncVMIndirectUMA() : ModulePass(ID) {
    initializeSyncVMIndirectUMAPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Wrap UMA in a function call";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }
};
} // namespace

char SyncVMIndirectUMA::ID = 0;
static const std::string UMLoadFunName[3] = {
    "__unaligned_load_as1", "__unaligned_load_as2", "__unaligned_load_as3"};
static const std::string UMStoreFunName[3] = {
    "__unaligned_store_as1", "__unaligned_store_as2", "__unaligned_store_as3"};

INITIALIZE_PASS(SyncVMIndirectUMA, "syncvm-indirectuma",
                "Wrap an UMA into a function call", false, false)

bool SyncVMIndirectUMA::runOnModule(Module &M) {
  bool Changed = false;
  std::vector<LoadInst *> UMLoads[3];
  std::vector<StoreInst *> UMStores[3];
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
            unsigned AS = Load->getPointerAddressSpace();
            assert(AS != 0 && AS < 4 && "Unexpected unaligned load");
            UMLoads[AS - 1].push_back(Load);
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
            unsigned AS = Store->getPointerAddressSpace();
            assert(AS != 0 && AS < 4 && "Unexpected unaligned store");
            UMStores[AS - 1].push_back(Store);
          }
        }
      }

  LLVMContext &C = M.getContext();
  Type *Int256Ty = Type::getInt256Ty(C);
  Type *Int256PtrTy[3] = {PointerType::getInt256PtrTy(C, 1),
                          PointerType::getInt256PtrTy(C, 2),
                          PointerType::getInt256PtrTy(C, 3)};

  for (unsigned i = 0; i < 3; ++i) {
    if (UMLoads[i].empty() && UMStores[i].empty())
      continue;
    std::vector<Type *> ArgTy = {Int256PtrTy[i]};
    M.getOrInsertFunction(UMLoadFunName[i],
                          FunctionType::get(Int256Ty, ArgTy, false));
    Function *UMLoadFunc = M.getFunction(UMLoadFunName[i]);
    auto *Entry = BasicBlock::Create(C, "entry", UMLoadFunc);
    IRBuilder<> Builder(Entry);
    auto *LdResult =
        Builder.CreateAlignedLoad(Int256Ty, UMLoadFunc->getArg(0), Align(1));
    Builder.CreateRet(LdResult);

    for (LoadInst *I : UMLoads[i]) {
      Builder.SetInsertPoint(I);
      unsigned LoadBitWidth = I->getType()->getIntegerBitWidth();
      auto *PtrCasted =
          Builder.CreatePointerCast(I->getPointerOperand(), Int256PtrTy[i]);
      Value *LoadRes = Builder.CreateCall(UMLoadFunc, {PtrCasted});
      if (LoadBitWidth != 256) {
        assert(LoadBitWidth < 256);
        LoadRes = Builder.CreateLShr(LoadRes, LoadBitWidth);
        LoadRes = Builder.CreateIntCast(LoadRes,
                                        Builder.getIntNTy(LoadBitWidth), false);
      }
      I->replaceAllUsesWith(LoadRes);
      I->eraseFromParent();
    }
  }

  for (unsigned i = 0; i < 3; ++i) {
    if (UMStores[i].empty())
      continue;
    std::vector<Type *> ArgTy = {Int256Ty, Int256PtrTy[i]};
    M.getOrInsertFunction(UMStoreFunName[i],
                          FunctionType::get(Type::getVoidTy(C), ArgTy, false));
    Function *UMStoreFunc = M.getFunction(UMStoreFunName[i]);
    Function *UMLoadFunc = M.getFunction(UMLoadFunName[i]);
    auto *Entry = BasicBlock::Create(C, "entry", UMStoreFunc);
    IRBuilder<> Builder(Entry);
    Builder.CreateAlignedStore(UMStoreFunc->getArg(0), UMStoreFunc->getArg(1),
                               Align(1));
    Builder.CreateRetVoid();
    for (StoreInst *I : UMStores[i]) {
      Builder.SetInsertPoint(I);
      unsigned StoreBitWidth =
          I->getValueOperand()->getType()->getIntegerBitWidth();
      auto *PtrCasted =
          Builder.CreatePointerCast(I->getPointerOperand(), Int256PtrTy[i]);
      auto *StoreVal = [&]() {
        if (StoreBitWidth != 256) {
          assert(StoreBitWidth < 256);
          Value *CellValue = Builder.CreateCall(UMLoadFunc, {PtrCasted});
          Value *Mask =
              Builder.getInt(APInt(256, -1, true).lshr(StoreBitWidth));
          CellValue = Builder.CreateAnd(CellValue, Mask);
          auto *StoreValue =
              Builder.CreateIntCast(I->getValueOperand(), Int256Ty, false);
          StoreValue = Builder.CreateShl(StoreValue, 256 - StoreBitWidth);
          StoreValue = Builder.CreateOr(CellValue, StoreValue);
          return StoreValue;
        }
        return I->getValueOperand();
      }();
      Builder.CreateCall(UMStoreFunc, {StoreVal, PtrCasted});
      I->eraseFromParent();
    }
  }

  return Changed;
}

ModulePass *llvm::createSyncVMIndirectUMAPass() {
  return new SyncVMIndirectUMA();
}
