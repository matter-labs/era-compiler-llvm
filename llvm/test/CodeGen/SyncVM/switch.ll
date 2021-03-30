; RUN: llc < %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: switch
define i256 @switch(i256 %p1) nounwind {
  switch i256 %p1, label %l3 [ i256 0, label %l1
                               i256 1, label %l2 ]
l1:
  ret i256 42
l2:
  ret i256 72
l3:
  ret i256 45
}
