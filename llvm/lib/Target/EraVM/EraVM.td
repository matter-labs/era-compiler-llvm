//===-- EraVM.td - Describe the EraVM Target Machine -------*- tablegen -*-===//
//
// This is the top level entry point for the EraVM target.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

class Proc<string Name, list<SubtargetFeature> Features>
    : Processor<Name, NoItineraries, Features>;
def : Proc<"generic", []>;

//===----------------------------------------------------------------------===//
// Register File Description
//===----------------------------------------------------------------------===//

include "EraVMRegisterInfo.td"

//===----------------------------------------------------------------------===//
// Calling Convention Description
//===----------------------------------------------------------------------===//

include "EraVMCallingConv.td"

//===----------------------------------------------------------------------===//
// Instruction Descriptions
//===----------------------------------------------------------------------===//

include "EraVMInstrInfo.td"

def EraVMInstrInfo : InstrInfo;

//===---------------------------------------------------------------------===//
// Assembly Printers
//===---------------------------------------------------------------------===//

def EraVMAsmWriter : AsmWriter { string AsmWriterClassName = "InstPrinter"; }

//===---------------------------------------------------------------------===//
// Assembly Parsers
//===---------------------------------------------------------------------===//

def EraVMAsmParser : AsmParser {
  let AllowDuplicateRegisterNames = 1;
  let ShouldEmitMatchRegisterAltName = 1;
}

//===----------------------------------------------------------------------===//
// Target Declaration
//===----------------------------------------------------------------------===//

def EraVM : Target {
  let InstructionSet = EraVMInstrInfo;
  let AssemblyParsers = [EraVMAsmParser];
}
