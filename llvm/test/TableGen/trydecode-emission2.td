// RUN: llvm-tblgen -gen-disassembler -I %p/../../include %s | FileCheck %s

include "llvm/Target/Target.td"

def archInstrInfo : InstrInfo { }

def arch : Target {
  let InstructionSet = archInstrInfo;
}

class TestInstruction : Instruction {
  let Size = 1;
  let OutOperandList = (outs);
  let InOperandList = (ins);
  field bits<8> Inst;
  field bits<8> SoftFail = 0;
}

def InstA : TestInstruction {
  let Inst = {0,0,0,0,0,0,?,?};
  let AsmString = "InstA";
  let DecoderMethod = "DecodeInstA";
  let hasCompleteDecoder = 0;
}

def InstB : TestInstruction {
  let Inst = {0,0,0,?,?,0,1,1};
  let AsmString = "InstB";
  let DecoderMethod = "DecodeInstB";
  let hasCompleteDecoder = 0;
}

// CHECK:      /* 0 */       MCD::OPC_ExtractField, 2, 0, 1, 0, // Inst{2} ...
// CHECK-NEXT: /* 5 */       MCD::OPC_FilterValue, 0, 42, 0, 0, // Skip to: 52
// CHECK-NEXT: /* 10 */      MCD::OPC_ExtractField, 5, 0, 3, 0, // Inst{7-5} ...
// CHECK-NEXT: /* 15 */      MCD::OPC_FilterValue, 0, 32, 0, 0, // Skip to: 52
// CHECK-NEXT: /* 20 */      MCD::OPC_CheckField, 0, 0, 2, 0, 3, 7, 0, 0, // Skip to: 36
// CHECK-NEXT: /* 29 */      MCD::OPC_TryDecode, {{[0-9]+}}, 1, 0, 0, 0, 0, // Opcode: InstB, skip to: 36
// CHECK-NEXT: /* 36 */      MCD::OPC_CheckField, 3, 0, 2, 0, 0, 7, 0, 0, // Skip to: 52
// CHECK-NEXT: /* 45 */      MCD::OPC_TryDecode, {{[0-9]+}}, 1, 1, 0, 0, 0, // Opcode: InstA, skip to: 52
// CHECK-NEXT: /* 52 */      MCD::OPC_Fail,

// CHECK: if (DecodeInstB(MI, insn, Address, Decoder) == MCDisassembler::Fail) { DecodeComplete = false; return MCDisassembler::Fail; }
// CHECK: if (DecodeInstA(MI, insn, Address, Decoder) == MCDisassembler::Fail) { DecodeComplete = false; return MCDisassembler::Fail; }
