//===---- EVM.td - Describe the EVM Target Machine -----*- tablegen -*-----===//
//
// This is the top level entry point for the EVM target.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"


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


def : ProcessorModel<"evm", NoSchedModel, []>;

//===----------------------------------------------------------------------===//
// Target Declaration
//===----------------------------------------------------------------------===//

def EVM : Target {
  let InstructionSet = EVMInstrInfo;
  let AssemblyWriters = [EVMAsmWriter];
}
