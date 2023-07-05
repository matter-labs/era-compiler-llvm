; RUN: llc --evm-keep-registers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @sdiv(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @sdiv
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: SDIV [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.sdiv(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @div(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @div
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: DIV [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.div(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @smod(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @smod
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: SMOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.smod(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @mod(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @mod
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: MOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.mod(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @shl(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @shl
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: SHL [[TMP:\$[0-9]+]], [[IN2]], [[IN1]]

  %res = call i256 @llvm.evm.shl(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @shr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @shr
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: SHR [[TMP:\$[0-9]+]], [[IN2]], [[IN1]]

  %res = call i256 @llvm.evm.shr(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @sar(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @sar
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: SAR [[TMP:\$[0-9]+]], [[IN2]], [[IN1]]

  %res = call i256 @llvm.evm.sar(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @addmod(i256 %rs1, i256 %rs2, i256 %rs3) nounwind {
; CHECK-LABEL: @addmod
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ADDMOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]]

  %res = call i256 @llvm.evm.addmod(i256 %rs1, i256 %rs2, i256 %rs3)
  ret i256 %res
}

define i256 @mulmod(i256 %rs1, i256 %rs2, i256 %rs3) nounwind {
; CHECK-LABEL: @mulmod
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: MULMOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]]

  %res = call i256 @llvm.evm.mulmod(i256 %rs1, i256 %rs2, i256 %rs3)
  ret i256 %res
}

define i256 @exp(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @exp
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: EXP [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.exp(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @sha3(ptr addrspace(1) %offset, i256 %size) nounwind {
; CHECK-LABEL: @sha3
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: SHA3 [[RES1:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.sha3(ptr addrspace(1) %offset, i256 %size)
  ret i256 %res
}

define i256 @signextend(i256 %bytesize, i256 %val) nounwind {
; CHECK-LABEL: @signextend
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: SIGNEXTEND [[RES1:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.signextend(i256 %bytesize, i256 %val)
  ret i256 %res
}

define i256 @byte(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @byte
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: BYTE [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.byte(i256 %rs1, i256 %rs2)
  ret i256 %res
}

define i256 @pc() nounwind {
; CHECK-LABEL: @pc
; CHECK: PC [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.pc()
  ret i256 %res
}

define i256 @msize() nounwind {
; CHECK-LABEL: @msize
; CHECK: MSIZE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.msize()
  ret i256 %res
}

define i256 @address() nounwind {
; CHECK-LABEL: @address
; CHECK: ADDRESS [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.address()
  ret i256 %res
}

define i256 @origin() nounwind {
; CHECK-LABEL: @origin
; CHECK: ORIGIN [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.origin()
  ret i256 %res
}

define i256 @caller() nounwind {
; CHECK-LABEL: @caller
; CHECK: CALLER [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.caller()
  ret i256 %res
}

define i256 @balance(i256 %rs1) nounwind {
; CHECK-LABEL: @balance
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: BALANCE [[RES1:\$[0-9]+]], [[IN1]]

  %res = call i256 @llvm.evm.balance(i256 %rs1)
  ret i256 %res
}

define i256 @calldatasize() nounwind {
; CHECK-LABEL: @calldatasize
; CHECK: CALLDATASIZE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.calldatasize()
  ret i256 %res
}

define i256 @calldataload(ptr addrspace(2) %rs1) nounwind {
; CHECK-LABEL: @calldataload
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CALLDATALOAD [[RES1:\$[0-9]+]], [[IN1]]

  %res = call i256 @llvm.evm.calldataload(ptr addrspace(2) %rs1)
  ret i256 %res
}

define i256 @callvalue() nounwind {
; CHECK-LABEL: @callvalue
; CHECK: CALLVALUE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.callvalue()
  ret i256 %res
}

define i256 @codesize() nounwind {
; CHECK-LABEL: @codesize
; CHECK: CODESIZE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.codesize()
  ret i256 %res
}

define i256 @gasprice() nounwind {
; CHECK-LABEL: @gasprice
; CHECK: GASPRICE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.gasprice()
  ret i256 %res
}

define i256 @extcodesize(i256 %rs1) nounwind {
; CHECK-LABEL: @extcodesize
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: EXTCODESIZE [[RES1:\$[0-9]+]], [[IN1]]

  %res = call i256 @llvm.evm.extcodesize(i256 %rs1)
  ret i256 %res
}

define void @extcodecopy(i256 %addr, ptr addrspace(1) %dst, ptr addrspace(4) %src, i256 %size) nounwind {
; CHECK-LABEL: @extcodecopy
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: EXTCODECOPY [[IN1]], [[IN2]], [[IN3]], [[IN4]]

  call void @llvm.evm.extcodecopy(i256 %addr, ptr addrspace(1) %dst, ptr addrspace(4) %src, i256 %size)
  ret void
}

define i256 @extcodehash(i256 %rs1) nounwind {
; CHECK-LABEL: @extcodehash
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: EXTCODEHASH [[RES1:\$[0-9]+]], [[IN1]]

  %res = call i256 @llvm.evm.extcodehash(i256 %rs1)
  ret i256 %res
}

define i256 @returndatasize() nounwind {
; CHECK-LABEL: @returndatasize
; CHECK: RETURNDATASIZE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.returndatasize()
  ret i256 %res
}

define i256 @blockhash(i256 %rs1) nounwind {
; CHECK-LABEL: @blockhash
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: BLOCKHASH [[RES1:\$[0-9]+]], [[IN1]]

  %res = call i256 @llvm.evm.blockhash(i256 %rs1)
  ret i256 %res
}

define i256 @blobhash(i256 %rs1) nounwind {
; CHECK-LABEL: @blobhash
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: BLOBHASH [[RES1:\$[0-9]+]], [[IN1]]

  %res = call i256 @llvm.evm.blobhash(i256 %rs1)
  ret i256 %res
}

define i256 @coinbase() nounwind {
; CHECK-LABEL: @coinbase
; CHECK: COINBASE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.coinbase()
  ret i256 %res
}

define i256 @timestamp() nounwind {
; CHECK-LABEL: @timestamp
; CHECK: TIMESTAMP [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.timestamp()
  ret i256 %res
}

define i256 @number() nounwind {
; CHECK-LABEL: @number
; CHECK: NUMBER [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.number()
  ret i256 %res
}

define i256 @difficulty() nounwind {
; CHECK-LABEL: @difficulty
; CHECK: DIFFICULTY [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.difficulty()
  ret i256 %res
}

define i256 @gaslimit() nounwind {
; CHECK-LABEL: @gaslimit
; CHECK: GASLIMIT [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.gaslimit()
  ret i256 %res
}

define i256 @chainid() nounwind {
; CHECK-LABEL: @chainid
; CHECK: CHAINID [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.chainid()
  ret i256 %res
}

define i256 @selfbalance() nounwind {
; CHECK-LABEL: @selfbalance
; CHECK: SELFBALANCE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.selfbalance()
  ret i256 %res
}

define i256 @basefee() nounwind {
; CHECK-LABEL: @basefee
; CHECK: BASEFEE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.basefee()
  ret i256 %res
}

define i256 @blobbasefee() nounwind {
; CHECK-LABEL: @blobbasefee
; CHECK: BLOBBASEFEE [[RES1:\$[0-9]+]]

  %res = call i256 @llvm.evm.blobbasefee()
  ret i256 %res
}

define void @log0(ptr addrspace(1) %off, i256 %size) nounwind {
; CHECK-LABEL: @log0
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: LOG0 [[IN1]], [[IN2]]

  call void @llvm.evm.log0(ptr addrspace(1) %off, i256 %size)
  ret void
}

define void @log1(ptr addrspace(1) %off, i256 %size, i256 %t1) nounwind {
; CHECK-LABEL: @log1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: LOG1 [[IN1]], [[IN2]], [[IN3]]

  call void @llvm.evm.log1(ptr addrspace(1) %off, i256 %size, i256 %t1)
  ret void
}

define void @log2(ptr addrspace(1) %off, i256 %size, i256 %t1, i256 %t2) nounwind {
; CHECK-LABEL: @log2
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: LOG2 [[IN1]], [[IN2]], [[IN3]], [[IN4]]

  call void @llvm.evm.log2(ptr addrspace(1) %off, i256 %size, i256 %t1, i256 %t2)
  ret void
}

define void @log3(ptr addrspace(1) %off, i256 %size, i256 %t1, i256 %t2, i256 %t3) nounwind {
; CHECK-LABEL: @log3
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: ARGUMENT [[IN5:\$[0-9]+]], 4
; CHECK: LOG3 [[IN1]], [[IN2]], [[IN3]], [[IN4]], [[IN5]]

  call void @llvm.evm.log3(ptr addrspace(1) %off, i256 %size, i256 %t1, i256 %t2, i256 %t3)
  ret void
}

define void @log4(ptr addrspace(1) %off, i256 %size, i256 %t1, i256 %t2, i256 %t3, i256 %t4) nounwind {
; CHECK-LABEL: @log4
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: ARGUMENT [[IN5:\$[0-9]+]], 4
; CHECK: ARGUMENT [[IN6:\$[0-9]+]], 5
; CHECK: LOG4 [[IN1]], [[IN2]], [[IN3]], [[IN4]], [[IN5]], [[IN6]]

  call void @llvm.evm.log4(ptr addrspace(1) %off, i256 %size, i256 %t1, i256 %t2, i256 %t3, i256 %t4)
  ret void
}

define i256 @create(i256 %val, ptr addrspace(1) %off, i256 %size) nounwind {
; CHECK-LABEL: @create
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: CREATE [[RES1:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]]

  %ret = call i256 @llvm.evm.create(i256 %val, ptr addrspace(1) %off, i256 %size)
  ret i256 %ret
}

define i256 @call(i256 %gas, i256 %addr, i256 %val, ptr addrspace(1) %arg_off, i256 %arg_size, ptr addrspace(1) %ret_off, i256 %ret_size) nounwind {
; CHECK-LABEL: @call
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: ARGUMENT [[IN5:\$[0-9]+]], 4
; CHECK: ARGUMENT [[IN6:\$[0-9]+]], 5
; CHECK: ARGUMENT [[IN7:\$[0-9]+]], 6
; CHECK: CALL [[RES:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]], [[IN4]], [[IN5]], [[IN6]], [[IN7]]

  %ret = call i256 @llvm.evm.call(i256 %gas, i256 %addr, i256 %val, ptr addrspace(1) %arg_off, i256 %arg_size, ptr addrspace(1) %ret_off, i256 %ret_size)
  ret i256 %ret
}

define i256 @delegatecall(i256 %gas, i256 %addr, ptr addrspace(1) %arg_off, i256 %arg_size, ptr addrspace(1) %ret_off, i256 %ret_size) nounwind {
; CHECK-LABEL: @delegatecall
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: ARGUMENT [[IN5:\$[0-9]+]], 4
; CHECK: ARGUMENT [[IN6:\$[0-9]+]], 5
; CHECK: DELEGATECALL [[RES:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]], [[IN4]], [[IN5]], [[IN6]]

  %ret = call i256 @llvm.evm.delegatecall(i256 %gas, i256 %addr, ptr addrspace(1) %arg_off, i256 %arg_size, ptr addrspace (1) %ret_off, i256 %ret_size)
  ret i256 %ret
}

define i256 @create2(i256 %val, ptr addrspace(1) %off, i256 %size, i256 %salt) nounwind {
; CHECK-LABEL: @create2
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: CREATE2 [[RES1:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]], [[IN4]]

  %ret = call i256 @llvm.evm.create2(i256 %val, ptr addrspace(1) %off, i256 %size, i256 %salt)
  ret i256 %ret
}

define i256 @staticcall(i256 %gas, i256 %addr, ptr addrspace(1) %arg_off, i256 %arg_size, ptr addrspace(1) %ret_off, i256 %ret_size) nounwind {
; CHECK-LABEL: @staticcall
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: ARGUMENT [[IN5:\$[0-9]+]], 4
; CHECK: ARGUMENT [[IN6:\$[0-9]+]], 5
; CHECK: STATICCALL [[RES:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]], [[IN4]], [[IN5]], [[IN6]]

  %ret = call i256 @llvm.evm.staticcall(i256 %gas, i256 %addr, ptr addrspace(1) %arg_off, i256 %arg_size, ptr addrspace(1) %ret_off, i256 %ret_size)
  ret i256 %ret
}

define void @selfdestruct(i256 %addr) nounwind {
; CHECK-LABEL: @selfdestruct
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SELFDESTRUCT [[IN1]]

  call void @llvm.evm.selfdestruct(i256 %addr)
  ret void
}

define void @return(ptr addrspace(1) %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @return
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: RETURN [[IN1]], [[IN2]]

  call void @llvm.evm.return(ptr addrspace(1) %rs1, i256 %rs2)
  ret void
}

define void @revert(ptr addrspace(1) %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @revert
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: REVERT [[IN1]], [[IN2]]

  call void @llvm.evm.revert(ptr addrspace(1) %rs1, i256 %rs2)
  ret void
}

define void @invalid() nounwind {
; CHECK-LABEL: @invalid
; CHECK: INVALID

  call void @llvm.evm.invalid()
  ret void
}

declare i256 @llvm.evm.sdiv(i256, i256)
declare i256 @llvm.evm.div(i256, i256)
declare i256 @llvm.evm.mod(i256, i256)
declare i256 @llvm.evm.smod(i256, i256)
declare i256 @llvm.evm.shl(i256, i256)
declare i256 @llvm.evm.shr(i256, i256)
declare i256 @llvm.evm.sar(i256, i256)
declare i256 @llvm.evm.addmod(i256, i256, i256)
declare i256 @llvm.evm.mulmod(i256, i256, i256)
declare i256 @llvm.evm.exp(i256, i256)
declare i256 @llvm.evm.sha3(ptr addrspace(1), i256)
declare i256 @llvm.evm.signextend(i256, i256)
declare i256 @llvm.evm.byte(i256, i256)
declare i256 @llvm.evm.pc()
declare i256 @llvm.evm.msize()
declare i256 @llvm.evm.address()
declare i256 @llvm.evm.origin()
declare i256 @llvm.evm.caller()
declare i256 @llvm.evm.balance(i256)
declare i256 @llvm.evm.calldatasize()
declare i256 @llvm.evm.calldataload(ptr addrspace(2))
declare i256 @llvm.evm.callvalue()
declare i256 @llvm.evm.codesize()
declare i256 @llvm.evm.gasprice()
declare i256 @llvm.evm.extcodesize(i256)
declare void @llvm.evm.extcodecopy(i256, ptr addrspace(1), ptr addrspace(4), i256)
declare i256 @llvm.evm.extcodehash(i256)
declare i256 @llvm.evm.blockhash(i256)
declare i256 @llvm.evm.blobhash(i256)
declare i256 @llvm.evm.returndatasize()
declare i256 @llvm.evm.coinbase()
declare i256 @llvm.evm.timestamp()
declare i256 @llvm.evm.number()
declare i256 @llvm.evm.difficulty()
declare i256 @llvm.evm.gaslimit()
declare i256 @llvm.evm.chainid()
declare i256 @llvm.evm.selfbalance()
declare i256 @llvm.evm.basefee()
declare i256 @llvm.evm.blobbasefee()
declare void @llvm.evm.log0(ptr addrspace(1), i256)
declare void @llvm.evm.log1(ptr addrspace(1), i256, i256)
declare void @llvm.evm.log2(ptr addrspace(1), i256, i256, i256)
declare void @llvm.evm.log3(ptr addrspace(1), i256, i256, i256, i256)
declare void @llvm.evm.log4(ptr addrspace(1), i256, i256, i256, i256, i256)
declare i256 @llvm.evm.create(i256, ptr addrspace(1), i256)
declare i256 @llvm.evm.call(i256, i256, i256, ptr addrspace(1), i256, ptr addrspace(1), i256)
declare i256 @llvm.evm.delegatecall(i256, i256, ptr addrspace(1), i256, ptr addrspace(1), i256)
declare i256 @llvm.evm.create2(i256, ptr addrspace(1), i256, i256)
declare i256 @llvm.evm.staticcall(i256, i256, ptr addrspace(1), i256, ptr addrspace(1), i256)
declare void @llvm.evm.selfdestruct(i256)
declare void @llvm.evm.return(ptr addrspace(1), i256)
declare void @llvm.evm.revert(ptr addrspace(1), i256)
declare void @llvm.evm.invalid()
declare void @llvm.evm.pop(i256)
