//===-- SyncVM.td - Describe the SyncVM Target Machine -----*- tablegen -*-===//
//
// This is the top level entry point for the SyncVM target.
//
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// Target-independent interfaces
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"



//===----------------------------------------------------------------------===//
// Register File Description
//===----------------------------------------------------------------------===//

include "SyncVMRegisterInfo.td"



//===----------------------------------------------------------------------===//
// Calling Convention Description
//===----------------------------------------------------------------------===//

include "SyncVMCallingConv.td"



//===----------------------------------------------------------------------===//
// Instruction Descriptions
//===----------------------------------------------------------------------===//

include "SyncVMInstrInfo.td"

def SyncVMInstrInfo : InstrInfo;



//===---------------------------------------------------------------------===//
// Assembly Printers
//===---------------------------------------------------------------------===//

def SyncVMAsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
}



//===---------------------------------------------------------------------===//
// Assembly Parsers
//===---------------------------------------------------------------------===//

def SyncVMAsmParser : AsmParser {
  let AllowDuplicateRegisterNames = 1;
  let ShouldEmitMatchRegisterAltName = 1;
}



//===----------------------------------------------------------------------===//
// Target Declaration
//===----------------------------------------------------------------------===//

def SyncVM : Target {
  let InstructionSet = SyncVMInstrInfo;
  let AssemblyParsers = [SyncVMAsmParser];
}
