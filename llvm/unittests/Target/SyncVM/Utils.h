//===---------------- llvm/unittests/Target/SyncVM/Utils.h ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SyncVMTargetMachine.h"
#include "SyncVMSubtarget.h"

#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "gtest/gtest.h"

using namespace llvm;
static std::once_flag flag;

static void InitializeSyncVMTarget() {
  std::call_once(flag, []() {
    LLVMInitializeSyncVMTargetInfo();
    LLVMInitializeSyncVMTarget();
    LLVMInitializeSyncVMTargetMC();
  });
}

[[maybe_unused]]
std::unique_ptr<const SyncVMTargetMachine>
createSyncVMTargetMachine() {
  InitializeSyncVMTarget();

  std::string Error;
  const Target *T = TargetRegistry::lookupTarget("syncvm", Error);
  if (!T)
    return nullptr;

  TargetOptions Options;
  return std::unique_ptr<SyncVMTargetMachine>(static_cast<SyncVMTargetMachine *>(
      T->createTargetMachine("syncvm", "", "", Options, None, None)));
}
