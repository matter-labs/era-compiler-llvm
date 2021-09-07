; RUN: llc < %s | FileCheck %s
source_filename = "main"
target datalayout = "e-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-NOT: call __cxa_throw
declare i32 @__personality()

define void @__selector() personality i32 ()* @__personality {
entry:
  %a3 = load i8, i8 addrspace(2)* inttoptr (i256 256 to i8 addrspace(2)*), align 32
  invoke void @__entry_f2(i8 %a3)
          to label %join4 unwind label %catch

catch:                                            ; preds = %entry
  %landing = landingpad { i8*, i32 }
          catch i8* null
  call void @__cxa_throw(i8* null, i8* null, i8* null)
  unreachable

join4:                                            ; preds = %entry
  ret void
}

define dso_local void @__entry_f2(i8 %0) personality i32 ()* @__personality {
entry:
  %a = alloca i8, align 32
  store i8 %0, i8* %a, align 32
  %a1 = load i8, i8* %a, align 32
  %1 = zext i8 %a1 to i256
  %2 = mul i256 %1, 2
  %3 = icmp ugt i256 %2, 255
  br i1 %3, label %throw, label %join

throw:                                            ; preds = %entry
  call void @__cxa_throw(i8* null, i8* null, i8* null)
  unreachable

join:                                             ; preds = %entry
  ret void
}

; Function Attrs: noreturn
declare void @llvm.syncvm.throw() #0
declare dso_local void @__cxa_throw(i8*, i8*, i8*) #0

attributes #0 = { noreturn }
