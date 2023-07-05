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
  let Inst = {0,0,0,0,?,?,?,?};
  let AsmString = "InstA";
}

def InstBOp : Operand<i32> {
  let DecoderMethod = "DecodeInstBOp";
  let hasCompleteDecoder = 0;
}

def InstB : TestInstruction {
  bits<2> op;
  let Inst{7...2} = {0,0,0,0,0,0};
  let Inst{1...0} = op;
  let OutOperandList = (outs InstBOp:$op);
  let AsmString = "InstB";
}

// CHECK:      /* 0 */       MCD::OPC_ExtractField, 4, 4, 0, // Inst{7-4} ...
// CHECK-NEXT: /* 4 */       MCD::OPC_FilterValue, 0, 19, 0, 0, // Skip to: 28
// CHECK-NEXT: /* 9 */       MCD::OPC_CheckField, 2, 2, 0, 0, 7, 0, 0, // Skip to: 24
// CHECK-NEXT: /* 17 */      MCD::OPC_TryDecode, {{[0-9]+}}, {{[0-9]+}}, 0, 0, 0, 0, // Opcode: InstB, skip to: 24
// CHECK-NEXT: /* 24 */      MCD::OPC_Decode, {{[0-9]+}}, {{[0-9]+}}, 1, // Opcode: InstA
// CHECK-NEXT: /* 28 */      MCD::OPC_Fail,

// CHECK: if (!Check(S, DecodeInstBOp(MI, tmp, Address, Decoder))) { DecodeComplete = false; return MCDisassembler::Fail; }
