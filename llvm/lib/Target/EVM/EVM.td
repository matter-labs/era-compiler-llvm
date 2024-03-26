//===---- EVM.td - Describe the EVM Target Machine -----*- tablegen -*-----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the top level entry point for the EVM target.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// scheduling information
//===----------------------------------------------------------------------===//
include "EVMSchedule.td"

//===----------------------------------------------------------------------===//
// Register File Description
//===----------------------------------------------------------------------===//

include "EVMRegisterInfo.td"


//===----------------------------------------------------------------------===//
// Instruction Descriptions
//===----------------------------------------------------------------------===//

include "EVMInstrInfo.td"

def EVMInstrInfo : InstrInfo;


//===----------------------------------------------------------------------===//
// Assembly Printers
//===----------------------------------------------------------------------===//

def EVMAsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
}

def : ProcessorModel<"evm", EVMModel, []>;

//===----------------------------------------------------------------------===//
// Target Declaration
//===----------------------------------------------------------------------===//

def EVM : Target {
  let InstructionSet = EVMInstrInfo;
  let AssemblyWriters = [EVMAsmWriter];
}
