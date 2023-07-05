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

// CHECK:      /* 0 */       MCD::OPC_ExtractField, 2, 1, 0, // Inst{2} ...
// CHECK-NEXT: /* 4 */       MCD::OPC_FilterValue, 0, 39, 0, 0, // Skip to: 48
// CHECK-NEXT: /* 9 */       MCD::OPC_ExtractField, 5, 3, 0, // Inst{7-5} ...
// CHECK-NEXT: /* 13 */      MCD::OPC_FilterValue, 0, 30, 0, 0, // Skip to: 48
// CHECK-NEXT: /* 18 */      MCD::OPC_CheckField, 0, 2, 0, 3, 7, 0, 0, // Skip to: 33
// CHECK-NEXT: /* 26 */      MCD::OPC_TryDecode, {{[0-9]+}}, {{[0-9]+}}, 0, 0, 0, 0, // Opcode: InstB, skip to: 33
// CHECK-NEXT: /* 33 */      MCD::OPC_CheckField, 3, 2, 0, 0, 7, 0, 0, // Skip to: 48
// CHECK-NEXT: /* 41 */      MCD::OPC_TryDecode, {{[0-9]+}}, {{[0-9]+}}, 1, 0, 0, 0, // Opcode: InstA, skip to: 48
// CHECK-NEXT: /* 48 */      MCD::OPC_Fail,

// CHECK: if (!Check(S, DecodeInstB(MI, insn, Address, Decoder))) { DecodeComplete = false; return MCDisassembler::Fail; }
// CHECK: if (!Check(S, DecodeInstA(MI, insn, Address, Decoder))) { DecodeComplete = false; return MCDisassembler::Fail; }
