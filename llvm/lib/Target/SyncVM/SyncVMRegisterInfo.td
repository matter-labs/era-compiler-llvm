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
def R7  : SyncVMRegister<6,  "r7">;
def R8  : SyncVMRegister<7,  "r8">;
def R9  : SyncVMRegister<8,  "r9">;
def R10 : SyncVMRegister<9,  "r10">;
def R11 : SyncVMRegister<10, "r11">;
def R12 : SyncVMRegister<11, "r12">;

def GR256 : RegisterClass<"SyncVM", [i256], 256, (add R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12)>;



//===----------------------------------------------------------------------===//
// Special registers
//===----------------------------------------------------------------------===//

def PC : SyncVMRegister<12, "pc">;
def SP : SyncVMRegister<13, "sp">;

def GR16 : RegisterClass<"SyncVM", [i16], 16, (add PC, SP)>;
