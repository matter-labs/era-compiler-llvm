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
  bool fitsOneCell() const {
    APInt AddressInBits = Address * 8;
    return AddressInBits.udiv(256) == (AddressInBits + Size - 1).udiv(256);
  }
  unsigned leadingZeroBits() const { return leadingZeroBytes() * 8; }
  unsigned trailingZeroBits() const {
    if (fitsOneCell())
      return 256 - leadingZeroBits() - Size;
    else
      return 512 - leadingZeroBits() - Size;
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
  } else if (auto *Store = dyn_cast<StoreInst>(I)) {
    AddressVal = Store->getPointerOperand();
    StoreVal = Store->getValueOperand();
  } else
    llvm_unreachable("Unsupported instruction");

  PtrType = AddressVal->getType();
  Type *T = cast<PointerType>(PtrType)->getElementType();
  Size = T->getIntegerBitWidth();
  assert(Size <= 256 &&
         "Load and store of more than 256 wide integers is not supported");
  if (auto *AddressIPtr = dyn_cast<IntToPtrInst>(AddressVal))
    if (auto AddressConst = dyn_cast<ConstantInt>(AddressIPtr->getOperand(0))) {
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
  Function *SmallLoadFunctions[4] = {nullptr, nullptr, nullptr, nullptr};
  Function *SmallStoreFunctions[4] = {nullptr, nullptr, nullptr, nullptr};
  Function *UnalignedLoadFunctions[4] = {nullptr, nullptr, nullptr, nullptr};
  Function *UnalignedStoreFunctions[4] = {nullptr, nullptr, nullptr, nullptr};
  /// Walk through \par F and replaces all addresses with the same ones, but in
  /// different address space.
  void replacePtrAs(Function &F, Type *PtrAsTy);
  Value *generateSmallLoadKnownAddr(const MemoryOperation &MO,
                                    IRBuilder<> &Builder);
  Value *generateSmallLoadUnknownAddr(const MemoryOperation &MO,
                                      IRBuilder<> &Builder);
  void expandSmallLoad(LoadInst *LI);
  Value *generateUnalignedLoadKnownAddr(const MemoryOperation &MO,
                                        IRBuilder<> &Builder);
  Value *generateUnalignedLoadUnknownAddr(const MemoryOperation &MO,
                                          IRBuilder<> &Builder);
  void expandUnalignedLoad(LoadInst *LI);
  void generateSmallStoreKnownAddr(const MemoryOperation &MO,
                                   IRBuilder<> &Builder);
  void generateSmallStoreUnknownAddr(const MemoryOperation &MO,
                                     IRBuilder<> &Builder);
  void expandSmallStore(StoreInst *SI);
  void generateUnalignedStoreKnownAddr(const MemoryOperation &MO,
                                       IRBuilder<> &Builder);
  void generateUnalignedStoreUnknownAddr(const MemoryOperation &MO,
                                         IRBuilder<> &Builder);
  void expandUnalignedStore(StoreInst *SI);
  void expandMemcpy(MemCpyInst *Mcpy);
};
} // namespace

char SyncVMExpandUMA::ID = 0;
std::string SmallLoadFuncName = "__small_load_as0";
std::string SmallStoreFuncNames[4] = {"__small_store_as0", "__small_store_as1",
                                      "__small_store_as1", "__small_store_as2"};
std::string UnalignedLoadFuncName = "__unaligned_load_as0";
std::string UnalignedStoreFuncName = "__unaligned_store_as0";

INITIALIZE_PASS(SyncVMExpandUMA, "syncvm-expanduma",
                "Expand unaligned and non-256 bits wide memory operations",
                false, false)

Value *SyncVMExpandUMA::generateSmallLoadKnownAddr(const MemoryOperation &MO,
                                                   IRBuilder<> &Builder) {
  Value *HiAddrInt = Builder.getInt(MO.baseAddress());
  Value *HiAddr = Builder.CreateIntToPtr(HiAddrInt, MO.pointerType(*C));
  Value *Val = Builder.CreateAlignedLoad(Int256Ty, HiAddr, Align(32));
  if (MO.fitsOneCell()) {
    if (MO.trailingZeroBits())
      Val = Builder.CreateLShr(Val, MO.trailingZeroBits());
    Value *OneCellMask =
        Builder.getInt(APInt(256, -1, true).lshr(256 - MO.size()));
    return Builder.CreateAnd(Val, OneCellMask);
  }

  unsigned BitsFirstCell = 256 - MO.leadingZeroBits();
  unsigned BitsSecondCell = MO.size() - BitsFirstCell;

  Val = Builder.CreateShl(Val, BitsSecondCell);
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
  if (AS != SyncVMAS::AS_STACK) {
    auto *LoadPtrTy = Int256Ty->getPointerTo(AS);
    Value *PtrCasted = Builder.CreateBitCast(MO.addressVal(), LoadPtrTy);
    Value *Result = Builder.CreateAlignedLoad(Int256Ty, PtrCasted, Align(1));
    auto ShiftAmount = APInt(256, 256 - MO.size(), false);
    return Builder.CreateLShr(Result, Builder.getInt(ShiftAmount));
  }
  assert(AS == SyncVMAS::AS_STACK && "Only stack address space is expected");
  assert(SmallLoadFunctions[AS] &&
         "Function have to be defined by this moment");
  Function *F = SmallLoadFunctions[AS];
  Value *AddrInt = Builder.CreatePtrToInt(MO.addressVal(), Int256Ty);
  return Builder.CreateCall(
      F->getFunctionType(), F,
      {AddrInt, Builder.getInt(APInt(256, MO.size(), false))});
}

void SyncVMExpandUMA::expandSmallLoad(LoadInst *LI) {
  MemoryOperation MO(LI);
  IRBuilder<> Builder(LI);
  Value *NewLoad = nullptr;
  if (MO.addrspace() == SyncVMAS::AS_STACK && MO.isStaticallyExpandable())
    NewLoad = generateSmallLoadKnownAddr(MO, Builder);
  else
    NewLoad = generateSmallLoadUnknownAddr(MO, Builder);
  NewLoad = Builder.CreateTrunc(NewLoad, Builder.getIntNTy(MO.size()));
  LI->replaceAllUsesWith(NewLoad);
}

Value *
SyncVMExpandUMA::generateUnalignedLoadKnownAddr(const MemoryOperation &MO,
                                                IRBuilder<> &Builder) {
  if (MO.leadingZeroBytes() == 0u)
    return Builder.CreateAlignedLoad(Int256Ty, MO.addressVal(), Align(32));
  Value *HiAddrInt = Builder.getInt(MO.baseAddress());
  Value *HiAddr = Builder.CreateIntToPtr(HiAddrInt, MO.pointerType(*C));
  Value *Val = Builder.CreateAlignedLoad(Int256Ty, HiAddr, Align(32));
  Val = Builder.CreateShl(Val, 256 - MO.trailingZeroBits());
  Value *LoAddrInt = Builder.getInt(MO.baseAddress() + 32);
  Value *LoAddr = Builder.CreateIntToPtr(LoAddrInt, MO.pointerType(*C));
  Value *LoVal = Builder.CreateAlignedLoad(Int256Ty, LoAddr, Align(32));
  LoVal = Builder.CreateLShr(LoVal, MO.trailingZeroBits());
  return Builder.CreateOr(Val, LoVal);
}

Value *
SyncVMExpandUMA::generateUnalignedLoadUnknownAddr(const MemoryOperation &MO,
                                                  IRBuilder<> &Builder) {
  unsigned AS = MO.addrspace();
  assert(AS == SyncVMAS::AS_STACK && "Only stack address space is expected");
  Function *F = UnalignedLoadFunctions[AS];
  Value *AddrInt = Builder.CreatePtrToInt(MO.addressVal(), Int256Ty);
  return Builder.CreateCall(F->getFunctionType(), F, {AddrInt});
}

void SyncVMExpandUMA::expandUnalignedLoad(LoadInst *LI) {
  MemoryOperation MO(LI);
  IRBuilder<> Builder(LI);
  Value *NewLoad = nullptr;
  if (MO.isStaticallyExpandable())
    NewLoad = generateUnalignedLoadKnownAddr(MO, Builder);
  else
    NewLoad = generateUnalignedLoadUnknownAddr(MO, Builder);
  LI->replaceAllUsesWith(NewLoad);
}

void SyncVMExpandUMA::generateSmallStoreKnownAddr(const MemoryOperation &MO,
                                                  IRBuilder<> &Builder) {
  Value *HiAddrInt = Builder.getInt(MO.baseAddress());
  Value *HiAddr = Builder.CreateIntToPtr(HiAddrInt, MO.pointerType(*C));
  Value *StoreVal = MO.storeVal();
  StoreVal = Builder.CreateZExt(StoreVal, Int256Ty);
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

  unsigned BitsFirstCell = 256 - MO.leadingZeroBits();
  unsigned BitsSecondCell = MO.size() - BitsFirstCell;

  OrigVal = Builder.CreateAnd(OrigVal, Builder.getInt(MaskOneCell));
  auto StoreValHi = Builder.CreateLShr(StoreVal, BitsSecondCell);
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
  Function *F = SmallStoreFunctions[AS];
  Value *AddrInt = Builder.CreatePtrToInt(MO.addressVal(), Int256Ty);
  Value *ExtVal = Builder.CreateZExt(MO.storeVal(), Int256Ty);
  Builder.CreateCall(
      F->getFunctionType(), F,
      {AddrInt, ExtVal, Builder.getInt(APInt(256, MO.size(), false))});
}

void SyncVMExpandUMA::expandSmallStore(StoreInst *SI) {
  MemoryOperation MO(SI);
  IRBuilder<> Builder(SI);
  if (MO.isStaticallyExpandable())
    generateSmallStoreKnownAddr(MO, Builder);
  else
    generateSmallStoreUnknownAddr(MO, Builder);
}

void SyncVMExpandUMA::generateUnalignedStoreKnownAddr(const MemoryOperation &MO,
                                                      IRBuilder<> &Builder) {
  if (MO.leadingZeroBytes() == 0u) {
    Builder.CreateAlignedStore(MO.storeVal(), MO.addressVal(), Align(32));
    return;
  }

  Value *HiAddrInt = Builder.getInt(MO.baseAddress());
  Value *HiAddr = Builder.CreateIntToPtr(HiAddrInt, MO.pointerType(*C));
  Value *StoreVal = MO.storeVal();
  Value *OrigVal = Builder.CreateAlignedLoad(Int256Ty, HiAddr, Align(32));
  auto MaskOneCell = APInt(256, -1, true).shl(256 - MO.leadingZeroBits());

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

void SyncVMExpandUMA::generateUnalignedStoreUnknownAddr(
    const MemoryOperation &MO, IRBuilder<> &Builder) {
  unsigned AS = MO.addrspace();
  Function *F = UnalignedStoreFunctions[AS];
  Value *AddrInt = Builder.CreatePtrToInt(MO.addressVal(), Int256Ty);
  Builder.CreateCall(F->getFunctionType(), F, {AddrInt, MO.storeVal()});
}

void SyncVMExpandUMA::expandUnalignedStore(StoreInst *SI) {
  MemoryOperation MO(SI);
  IRBuilder<> Builder(SI);
  if (MO.isStaticallyExpandable())
    generateUnalignedStoreKnownAddr(MO, Builder);
  else
    generateUnalignedStoreUnknownAddr(MO, Builder);
}

bool SyncVMExpandUMA::runOnModule(Module &M) {
  Mod = &M;
  C = &M.getContext();
  Int256Ty = Type::getInt256Ty(*C);
  SmallLoadFunctions[0] = M.getFunction(SmallLoadFuncName);
  for (unsigned i = 0; i < 4; ++i)
    SmallStoreFunctions[i] = M.getFunction(SmallStoreFuncNames[i]);
  UnalignedLoadFunctions[0] = M.getFunction(UnalignedLoadFuncName);
  UnalignedStoreFunctions[0] = M.getFunction(UnalignedStoreFuncName);
  assert(SmallLoadFunctions[0] && SmallStoreFunctions[0] &&
         UnalignedLoadFunctions[0] && UnalignedStoreFunctions[0] &&
         "Runtime is broken");
  std::vector<Instruction *> Erase;
  for (Function &F : M)
    for (BasicBlock &BB : F)
      for (Instruction &I : BB) {
        if (auto *LI = dyn_cast<LoadInst>(&I)) {
          Type *ElTy =
              cast<PointerType>(LI->getPointerOperandType())->getElementType();
          if (ElTy->isIntegerTy() && ElTy->getIntegerBitWidth() != 256) {
            expandSmallLoad(LI);
            Erase.push_back(LI);
          } else if (LI->getPointerAddressSpace() == SyncVMAS::AS_STACK &&
                     LI->getAlignment() % 32) {
            expandUnalignedLoad(LI);
            Erase.push_back(LI);
          }
        } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
          Type *ElTy =
              cast<PointerType>(SI->getPointerOperandType())->getElementType();
          if (ElTy->isIntegerTy() && ElTy->getIntegerBitWidth() != 256) {
            expandSmallStore(SI);
            Erase.push_back(SI);
          } else if (SI->getPointerAddressSpace() == SyncVMAS::AS_STACK &&
                     SI->getAlignment() % 32) {
            expandUnalignedStore(SI);
            Erase.push_back(SI);
          }
        }
      }
  for (Instruction *I : Erase)
    I->eraseFromParent();
  return !Erase.empty();
}

ModulePass *llvm::createSyncVMExpandUMAPass() { return new SyncVMExpandUMA(); }
