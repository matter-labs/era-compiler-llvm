//===-- SyncVMRegisterInfo.td - SyncVM Register defs -------*- tablegen -*-===//
//
//  Declarations that describe the SyncVM register file
//
//===----------------------------------------------------------------------===//

class SyncVMRegister<bits<4> num, string n> : Register<n> {
  field bits<4> Num = num;

  let Namespace = "SyncVM";
  let HWEncoding{3-0} = num;
}



//===----------------------------------------------------------------------===//
// General purpose registers
//===----------------------------------------------------------------------===//

def R1  : SyncVMRegister<0,  "r1">;
def R2  : SyncVMRegister<1,  "r2">;
def R3  : SyncVMRegister<2,  "r3">;
def R4  : SyncVMRegister<3,  "r4">;
def R5  : SyncVMRegister<4,  "r5">;
def R6  : SyncVMRegister<5,  "r6">;

def GR256 : RegisterClass<"SyncVM", [i256], 256, (add R1, R2, R3, R4, R5, R6)>;



//===----------------------------------------------------------------------===//
// Special registers
//===----------------------------------------------------------------------===//

def PC : SyncVMRegister<6, "pc">;
def SP : SyncVMRegister<7, "sp">;

def GR16 : RegisterClass<"SyncVM", [i16], 16, (add PC, SP)>;
