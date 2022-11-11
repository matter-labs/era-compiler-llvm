//===------ SyncVMExpandUMA.cpp - Make all loads and stores aligned ------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "SyncVM.h"

using namespace llvm;

namespace llvm {
ModulePass *createExpandUMAPass();
void initializeSyncVMExpandUMAPass(PassRegistry &);
} // namespace llvm

namespace {

/// Calculate properties of a memory operation
class MemoryOperation {
private:
  bool HasStaticAddress = false;
  APInt Address = APInt(256, 0, false);
  Type *PtrType = nullptr;
  Value *StoreVal = nullptr;
  Value *AddressVal = nullptr;
  unsigned Size = 0;

public:
  MemoryOperation(Instruction *I);
  bool isStaticallyExpandable() const { return HasStaticAddress; }
  unsigned leadingZeroBytes() const {
    assert(isStaticallyExpandable());
    return Address.urem(32);
  }
  unsigned leadingZeroBits() const { return leadingZeroBytes() * 8; }
  unsigned trailingZeroBytes() const {
    assert(isStaticallyExpandable());
    return 31 - (Address + Size - 1).urem(32);
  }
  unsigned trailingZeroBits() const { return trailingZeroBytes() * 8; }
  bool fitsOneCell() const {
    return Address.udiv(32) == (Address + Size - 1).udiv(32);
  }
  APInt baseAddress() const { return Address - Address.urem(32); }
  Value *addressVal() const { return AddressVal; }
  Value *storeVal() const { return StoreVal; }
  Type *pointerType(LLVMContext &C) const {
    return PointerType::getInt256PtrTy(C, addrspace());
  }
  unsigned addrspace() const {
    return cast<PointerType>(PtrType)->getAddressSpace();
  }
  unsigned size() const { return Size; }
};

MemoryOperation::MemoryOperation(Instruction *I) {
  if (auto *Load = dyn_cast<LoadInst>(I)) {
    AddressVal = Load->getPointerOperand();
    Size = Load->getType()->getIntegerBitWidth();
  } else if (auto *Store = dyn_cast<StoreInst>(I)) {
    AddressVal = Store->getPointerOperand();
    StoreVal = Store->getValueOperand();
    Size = Store->getValueOperand()->getType()->getIntegerBitWidth();
  } else
    llvm_unreachable("Unsupported instruction");

  PtrType = AddressVal->getType();
  if (auto *AddressConst = dyn_cast<ConstantInt>(AddressVal)) {
    HasStaticAddress = true;
    Address = AddressConst->getValue();
  }
}

/// SyncVM only support i256 loads and stores aligned by 32 bytes.
/// The pass replaces all iN, N < 256 loads and stores with i256 bits
/// loads and stores and bitwise operation. If a memory operation crosses
/// 32-byte border, it splits in two. Memcpy expansion strives to maximize
/// number of aligned i256 store operation since an unaligned store is more
/// expensive than an unaligned store.
struct SyncVMExpandUMA : public ModulePass {
public:
  static char ID;
  SyncVMExpandUMA() : ModulePass(ID) {
    initializeSyncVMExpandUMAPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Expand unaligned and non-256 bits wide memory operations";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

private:
  LLVMContext *C;
  Module *Mod;
  Type *Int256Ty;
  Function *LoadFunctions[4] = {nullptr, nullptr, nullptr, nullptr};
  Function *StoreFunctions[4] = {nullptr, nullptr, nullptr, nullptr};
  Value *generateSmallLoadKnownAddr(const MemoryOperation &MO,
                                    IRBuilder<> &Builder);
  Value *generateSmallLoadUnknownAddr(const MemoryOperation &MO,
                                      IRBuilder<> &Builder);
  void expandSmallLoad(LoadInst *LI);
  void generateSmallStoreKnownAddr(const MemoryOperation &MO,
                                   IRBuilder<> &Builder);
  void generateSmallStoreUnknownAddr(const MemoryOperation &MO,
                                     IRBuilder<> &Builder);
  void expandSmallStore(StoreInst *SI);
  void expandUnalignedLoad(LoadInst *LI);
  void expandUnalignedStore(StoreInst *SI);
  void expandMemcpy(MemCpyInst *Mcpy);
};
} // namespace

char SyncVMExpandUMA::ID = 0;
static const std::string UMLoadFunName[3] = {
    "__unaligned_load_as1", "__unaligned_load_as2", "__unaligned_load_as3"};
static const std::string UMStoreFunName[3] = {
    "__unaligned_store_as1", "__unaligned_store_as2", "__unaligned_store_as3"};
std::string SmallLoadFuncName = "__small_load_as0";
std::string SmallStoreFuncName = "__small_store_as0";

INITIALIZE_PASS(SyncVMExpandUMA, "syncvm-expanduma",
                "Expand unaligned and non-256 bits wide memory operations",
                false, false)

Value *SyncVMExpandUMA::generateSmallLoadKnownAddr(const MemoryOperation &MO,
                                                   IRBuilder<> &Builder) {
  Value *HiAddrInt = Builder.getInt(MO.baseAddress());
  Value *HiAddr = Builder.CreateIntToPtr(HiAddrInt, MO.pointerType(*C));
  Value *Val = Builder.CreateAlignedLoad(Int256Ty, HiAddr, Align(32));
  if (MO.fitsOneCell()) {
    Val = Builder.CreateLShr(Val, MO.trailingZeroBits());
    Value *OneCellMask = Builder.getInt(
        APInt(256, -1, true).lshr(MO.leadingZeroBits() + MO.leadingZeroBits()));
    return Builder.CreateAnd(Val, OneCellMask);
  }
  Val = Builder.CreateShl(Val, 256 - MO.trailingZeroBits());
  Value *LoAddrInt = Builder.getInt(MO.baseAddress() + 32);
  Value *LoAddr = Builder.CreateIntToPtr(LoAddrInt, MO.pointerType(*C));
  Value *LoVal = Builder.CreateAlignedLoad(Int256Ty, LoAddr, Align(32));
  LoVal = Builder.CreateLShr(LoVal, MO.trailingZeroBits());
  Val = Builder.CreateOr(Val, LoVal);
  Value *TwoCellsMask =
      Builder.getInt(APInt(256, -1, true).lshr(256 - MO.size()));
  return Builder.CreateAnd(Val, TwoCellsMask);
}

Value *SyncVMExpandUMA::generateSmallLoadUnknownAddr(const MemoryOperation &MO,
                                                     IRBuilder<> &Builder) {
  unsigned AS = MO.addrspace();
  if (!LoadFunctions[AS]) {
    ValueToValueMapTy VMap;
    LoadFunctions[AS] = CloneFunction(LoadFunctions[0], VMap);
    Function &F = *LoadFunctions[AS];
    for (BasicBlock &BB : F)
      for (auto I = BB.begin(), E = BB.end(); I != E; ++I) {
        auto *Load = dyn_cast<LoadInst>(I);
        if (!Load)
          continue;
        Value *MemOperand = Load->getPointerOperand();
        IRBuilder<> FuncBuilder(Load);
        Value *NewMemOp = FuncBuilder.CreateIntToPtr(
            cast<IntToPtrInst>(MemOperand)->getOperand(0), MO.pointerType(*C));
        Value *NewLoad =
            FuncBuilder.CreateAlignedLoad(Int256Ty, NewMemOp, Align(32));
        ++I;
        Load->replaceAllUsesWith(NewLoad);
        Load->eraseFromParent();
        if (MemOperand->hasNUses(0))
          cast<IntToPtrInst>(MemOperand)->eraseFromParent();
      }
  }
  assert(LoadFunctions[AS] && "Function have to be defined by this moment");
  Function *F = LoadFunctions[AS];
  Value *AddrInt = Builder.CreatePtrToInt(MO.addressVal(), Int256Ty);
  return Builder.CreateCall(
      F->getFunctionType(), F,
      {AddrInt, Builder.getInt(APInt(256, MO.size(), false))});
}

void SyncVMExpandUMA::expandSmallLoad(LoadInst *LI) {
  MemoryOperation MO(LI);
  IRBuilder<> Builder(LI);
  Value *NewLoad = nullptr;
  if (MO.isStaticallyExpandable())
    NewLoad = generateSmallLoadKnownAddr(MO, Builder);
  else
    NewLoad = generateSmallLoadUnknownAddr(MO, Builder);
  NewLoad = Builder.CreateTrunc(NewLoad, Builder.getIntNTy(MO.size()));
  LI->replaceAllUsesWith(NewLoad);
}

void SyncVMExpandUMA::generateSmallStoreKnownAddr(const MemoryOperation &MO,
                                                  IRBuilder<> &Builder) {
  Value *HiAddrInt = Builder.getInt(MO.baseAddress());
  Value *HiAddr = Builder.CreateIntToPtr(HiAddrInt, MO.pointerType(*C));
  Value *StoreVal = MO.storeVal();
  Value *OrigVal = Builder.CreateAlignedLoad(Int256Ty, HiAddr, Align(32));
  auto MaskOneCell = APInt(256, -1, true).shl(256 - MO.leadingZeroBits());
  if (MO.fitsOneCell()) {
    unsigned TrailingBits = 256 - MO.leadingZeroBits() - MO.size();
    if (TrailingBits) {
      StoreVal = Builder.CreateShl(StoreVal, TrailingBits);
      MaskOneCell |= APInt(256, -1, true).lshr(256 - TrailingBits);
    }
    OrigVal = Builder.CreateAnd(OrigVal, Builder.getInt(MaskOneCell));
    StoreVal = Builder.CreateOr(OrigVal, StoreVal);
    Builder.CreateAlignedStore(StoreVal, HiAddr, Align(32));
    return;
  }
  OrigVal = Builder.CreateAnd(OrigVal, Builder.getInt(MaskOneCell));
  auto StoreValHi =
      Builder.CreateLShr(StoreVal, MO.size() + MO.leadingZeroBits() - 256);
  StoreValHi = Builder.CreateOr(OrigVal, StoreValHi);
  Builder.CreateAlignedStore(StoreValHi, HiAddr, Align(32));

  Value *LoAddrInt = Builder.getInt(MO.baseAddress() + 32);
  Value *LoAddr = Builder.CreateIntToPtr(LoAddrInt, MO.pointerType(*C));
  OrigVal = Builder.CreateAlignedLoad(Int256Ty, LoAddr, Align(32));
  StoreVal = Builder.CreateShl(StoreVal, MO.trailingZeroBits());
  OrigVal = Builder.CreateAnd(
      OrigVal,
      Builder.getInt(APInt(256, -1, true).lshr(256 - MO.trailingZeroBits())));
  StoreVal = Builder.CreateOr(OrigVal, StoreVal);
  Builder.CreateAlignedStore(StoreVal, LoAddr, Align(32));
}

void SyncVMExpandUMA::generateSmallStoreUnknownAddr(const MemoryOperation &MO,
                                                    IRBuilder<> &Builder) {
  unsigned AS = MO.addrspace();
  if (!StoreFunctions[AS]) {
    ValueToValueMapTy VMap;
    StoreFunctions[AS] = CloneFunction(StoreFunctions[0], VMap);
    Function &F = *StoreFunctions[AS];
    for (BasicBlock &BB : F)
      for (auto I = BB.begin(), E = BB.end(); I != E; ++I) {
        if (auto *Store = dyn_cast<StoreInst>(I)) {
          Value *MemOperand = Store->getPointerOperand();
          Value *StoreVal = Store->getValueOperand();
          IRBuilder<> FuncBuilder(Store);
          Value *NewMemOp = FuncBuilder.CreateIntToPtr(
              cast<IntToPtrInst>(MemOperand)->getOperand(0), MO.pointerType(*C));
          FuncBuilder.CreateAlignedStore(StoreVal, NewMemOp, Align(32));
          ++I;
          Store->eraseFromParent();
          if (MemOperand->hasNUses(0))
            cast<IntToPtrInst>(MemOperand)->eraseFromParent();
        } else if (auto *Load = dyn_cast<LoadInst>(I)) {
          Value *MemOperand = Load->getPointerOperand();
          IRBuilder<> FuncBuilder(Load);
          Value *NewMemOp = FuncBuilder.CreateIntToPtr(
              cast<IntToPtrInst>(MemOperand)->getOperand(0), MO.pointerType(*C));
          Value *NewLoad =
              FuncBuilder.CreateAlignedLoad(Int256Ty, NewMemOp, Align(32));
          ++I;
          Load->replaceAllUsesWith(NewLoad);
          Load->eraseFromParent();
          if (MemOperand->hasNUses(0))
            cast<IntToPtrInst>(MemOperand)->eraseFromParent();
        }
      }
  }
  assert(StoreFunctions[AS] && "Function have to be defined by this moment");
  Function *F = StoreFunctions[AS];
  Value *AddrInt = Builder.CreatePtrToInt(MO.addressVal(), Int256Ty);
  Value *ExtVal = Builder.CreateZExt(MO.storeVal(), Int256Ty);
  Builder.CreateCall(
      F->getFunctionType(), F,
      {AddrInt, Builder.getInt(APInt(256, MO.size(), false)), ExtVal});
}

void SyncVMExpandUMA::expandSmallStore(StoreInst *SI) {
  MemoryOperation MO(SI);
  IRBuilder<> Builder(SI);
  if (MO.isStaticallyExpandable())
    generateSmallStoreKnownAddr(MO, Builder);
  else
    generateSmallStoreUnknownAddr(MO, Builder);
}

bool SyncVMExpandUMA::runOnModule(Module &M) {
  Mod = &M;
  C = &M.getContext();
  Int256Ty = Type::getInt256Ty(*C);
  LoadFunctions[0] = M.getFunction(SmallLoadFuncName);
  StoreFunctions[0] = M.getFunction(SmallStoreFuncName);
  assert(LoadFunctions[0] && StoreFunctions[0]);
  std::vector<Instruction *> Erase;
  for (Function &F : M)
    for (BasicBlock &BB : F)
      for (Instruction &I : BB) {
        if (auto *LI = dyn_cast<LoadInst>(&I)) {
          Type *ElTy = LI->getType();
          if (ElTy->isIntegerTy() && ElTy->getIntegerBitWidth() != 256) {
            expandSmallLoad(LI);
            Erase.push_back(LI);
          }
        } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
          Type *ElTy = SI->getValueOperand()->getType();
          if (ElTy->isIntegerTy() && ElTy->getIntegerBitWidth() != 256) {
            expandSmallStore(SI);
            Erase.push_back(SI);
          }
        }
      }
  for (Instruction *I : Erase)
    I->eraseFromParent();
  return !Erase.empty();
}

ModulePass *llvm::createSyncVMExpandUMAPass() { return new SyncVMExpandUMA(); }
