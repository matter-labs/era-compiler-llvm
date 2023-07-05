// RUN: llvm-tblgen -gen-disassembler -I %p/../../include %s | FileCheck %s

// Test for OPC_ExtractField/OPC_CheckField with start bit > 255.
// These large start values may arise for architectures with long instruction
// words.

include "llvm/Target/Target.td"

def archInstrInfo : InstrInfo { }

def arch : Target {
  let InstructionSet = archInstrInfo;
}

class TestInstruction : Instruction {
  let Size = 64;
  let OutOperandList = (outs);
  let InOperandList = (ins);
  field bits<512> Inst;
  field bits<512> SoftFail = 0;
}

def InstA : TestInstruction {
  let Inst{509-502} = {0,0,0,0,?,?,?,?};
  let AsmString = "InstA";
}

def InstB : TestInstruction {
  let Inst{509-502} = {0,0,0,0,0,0,?,?};
  let AsmString = "InstB";
  let DecoderMethod = "DecodeInstB";
  let hasCompleteDecoder = 0;
}


// CHECK:      /* 0 */       MCD::OPC_ExtractField, 250, 3, 4, 0,  // Inst{509-506} ...
// CHECK-NEXT: /* 5 */       MCD::OPC_FilterValue, 0, 20, 0, 0, // Skip to: 30
// CHECK-NEXT: /* 10 */      MCD::OPC_CheckField, 248, 3, 2, 0, 0, 7, 0, 0, // Skip to: 26
// CHECK-NEXT: /* 19 */      MCD::OPC_TryDecode, {{[0-9]+}}, {{[0-9]+}}, 0, 0, 0, 0, // Opcode: InstB, skip to: 26
// CHECK-NEXT: /* 26 */      MCD::OPC_Decode, {{[0-9]+}}, {{[0-9]+}}, 1, // Opcode: InstA
// CHECK-NEXT: /* 30 */      MCD::OPC_Fail,

// CHECK: if (!Check(S, DecodeInstB(MI, insn, Address, Decoder))) { DecodeComplete = false; return MCDisassembler::Fail; }

