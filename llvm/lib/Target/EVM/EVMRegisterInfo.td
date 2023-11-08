//===---EVMRegisterInfo.td - Describe the EVM Registers -*- tablegen -*----===//
//
// This file describes the EVM register classes and some nominal
// physical registers.
//
//===----------------------------------------------------------------------===//

class EVMReg<string n> : Register<n> {
  let Namespace = "EVM";
}

//===----------------------------------------------------------------------===//
// Registers
//===----------------------------------------------------------------------===//

// Special registers used as the frame and stack pointer.
def SP : EVMReg<"%SP">;

// The register allocation framework requires register classes have at least
// one register, so we define one for the integer register class since
// we otherwise don't need a physical register in this class.
def GPR256 : EVMReg<"%reg">;

// The incoming arguments "register". This is an opaque entity which serves to
// order the ARGUMENT instructions that are emulating live-in registers and
// must not be scheduled below other instructions.
def ARGUMENTS : EVMReg<"ARGUMENTS">;

//===----------------------------------------------------------------------===//
//  Register classes
//===----------------------------------------------------------------------===//

def GPR : RegisterClass<"EVM", [i256], 256, (add GPR256, SP)>;
