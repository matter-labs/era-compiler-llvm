//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EVMSelectionDAGInfo.h"

#define GET_SDNODE_DESC
#include "EVMGenSDNodeInfo.inc"

using namespace llvm;

EVMSelectionDAGInfo::EVMSelectionDAGInfo()
    : SelectionDAGGenTargetInfo(EVMGenSDNodeInfo) {}

EVMSelectionDAGInfo::~EVMSelectionDAGInfo() = default;

const char *EVMSelectionDAGInfo::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<EVMISD::NodeType>(Opcode)) {
  case EVMISD::FCALL:
    return "EVMISD::FCALL";
  }

  return SelectionDAGGenTargetInfo::getTargetNodeName(Opcode);
}
