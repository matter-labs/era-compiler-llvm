// RUN: llvm-tblgen -gen-disassembler -I %p/../../include %s | FileCheck %s

// Checks decoding of long instructions with the bit width > 256.

include "llvm/Target/Target.td"

def archInstrInfo : InstrInfo { }

def arch : Target {
  let InstructionSet = archInstrInfo;
}

class TestInstruction : Instruction {
  let OutOperandList = (outs);
  let InOperandList = (ins);
  field bits<264> Inst;
}

def InstA : TestInstruction {
  let Size = 33;
  let Inst{263-256} = 0x7E;
  let AsmString = "InstA";
}

// CHECK:      /* 0 */       MCD::OPC_CheckField, 128, 2, 8, 0, 126, 4, 0, 0, // Skip to: 13
// CHECK-NEXT: /* 9 */       MCD::OPC_Decode, {{[0-9]+}}, {{[0-9]+}}, 0, // Opcode: InstA
// CHECK-NEXT: /* 13 */      MCD::OPC_Fail,
