//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_EVM_EVMSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

#define GET_SDNODE_ENUM
#include "EVMGenSDNodeInfo.inc"

namespace llvm {

namespace EVMISD {

enum NodeType : unsigned {
  FCALL = GENERATED_OPCODE_END,
};

} // namespace EVMISD

class EVMSelectionDAGInfo : public SelectionDAGGenTargetInfo {
public:
  EVMSelectionDAGInfo();

  ~EVMSelectionDAGInfo() override;

  const char *getTargetNodeName(unsigned Opcode) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSELECTIONDAGINFO_H
