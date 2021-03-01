//===-- SyncVMInstrFormats.td - SyncVM Instruction Formats -*- tablegen -*-===//
//
// 
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Describe SyncVM instructions format here
//

class OperationCode<bits<8> value> {
  bits<8> Value = value;
}


// Generic instruction
class SyncVMInstruction<
  dag inputs,
  dag outputs
> : Instruction
{
  field bits<256> Bits;

  dag InputOperandList = inputs;
  dag OutputOperandList = outputs;
}


// ADD arithmetic instruction
class Add<
  OperationCode opcode,
  dag inputs,
  dag outputs
> : SyncVMInstruction<
  inputs,
  outputs
>
{
  let Bits{0-7} = opcode.Value;
}
