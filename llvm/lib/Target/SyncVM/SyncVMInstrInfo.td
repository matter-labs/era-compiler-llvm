//===-- SyncVMInstrInfo.td - SyncVM Instruction defs -------*- tablegen -*-===//
//
// This file describes the SyncVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "SyncVMInstrFormats.td"

//===----------------------------------------------------------------------===//
// Arithmetic Instructions

multiclass Arithmetic<dag inputs, dag outputs> {
  def add : Add<
    OperationCode<0x00>,
    (inputs GR256:$src1, GR256:$src2),
    (outputs GR256:$dst)
  >;
}
