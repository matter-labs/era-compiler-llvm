//===-- SyncVMRegisterInfo.td - SyncVM Register defs -------*- tablegen -*-===//
//
//  Declarations that describe the SyncVM register file
//
//===----------------------------------------------------------------------===//

class SyncVMReg<bits<4> num, string n> : Register<n> {
  field bits<4> Num = num;
  let Namespace = "SyncVM";
}

//===----------------------------------------------------------------------===//
//  Registers
//===----------------------------------------------------------------------===//

def PC  : SyncVMReg<0,   "pc">;
def SP  : SyncVMReg<1,   "sp">;
def R1  : SyncVMReg<2,   "r1">;
def R2  : SyncVMReg<3,   "r2">;
def R3  : SyncVMReg<4,   "r3">;
def R4  : SyncVMReg<5,   "r4">;
def R5  : SyncVMReg<6,   "r5">;
def R6  : SyncVMReg<7,   "r6">;
def R7  : SyncVMReg<8,   "r7">;
def R8  : SyncVMReg<9,   "r8">;
def R9  : SyncVMReg<10,  "r9">;
def R10 : SyncVMReg<11,  "r10">;
def R11 : SyncVMReg<12,  "r11">;
def R12 : SyncVMReg<13,  "r12">;

def GR256 : RegisterClass<"SyncVM", [v256i1], 256, (add
    PC,
    SP,
    R1,  R2,  R3,  R4,
    R5,  R6,  R7,  R8,
    R9,  R10, R11, R12
)>;
