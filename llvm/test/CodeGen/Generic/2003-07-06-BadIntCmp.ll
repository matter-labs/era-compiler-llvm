; RUN: llc < %s
; UNSUPPORTED: target=evm{{.*}}

;; Date: May 28, 2003.
;; From: test/Programs/MultiSource/Olden-perimeter/maketree.c
;; Function: int CheckOutside(int x, int y)
;; 
;; Note: The .ll code below for this regression test has identical
;;	 behavior to the above function up to the error, but then prints
;; 	 true/false on the two branches.
;; 
;; Error: llc generates a branch-on-xcc instead of branch-on-icc, which
;;        is wrong because the value being compared (int euclid = x*x + y*y)
;;	  overflows, so that the 64-bit and 32-bit compares are not equal.

@.str_1 = internal constant [6 x i8] c"true\0A\00"              ; <ptr> [#uses=1]
@.str_2 = internal constant [7 x i8] c"false\0A\00"             ; <ptr> [#uses=1]

declare i32 @printf(ptr, ...)

define internal void @__main() {
entry:
        ret void
}

define internal void @CheckOutside(i32 %x.1, i32 %y.1) {
entry:
        %tmp.2 = mul i32 %x.1, %x.1             ; <i32> [#uses=1]
        %tmp.5 = mul i32 %y.1, %y.1             ; <i32> [#uses=1]
        %tmp.6 = add i32 %tmp.2, %tmp.5         ; <i32> [#uses=1]
        %tmp.8 = icmp sle i32 %tmp.6, 4194304           ; <i1> [#uses=1]
        br i1 %tmp.8, label %then, label %else

then:           ; preds = %entry
        %tmp.11 = call i32 (ptr, ...) @printf( ptr @.str_1 )           ; <i32> [#uses=0]
        br label %UnifiedExitNode

else:           ; preds = %entry
        %tmp.13 = call i32 (ptr, ...) @printf( ptr @.str_2 )           ; <i32> [#uses=0]
        br label %UnifiedExitNode

UnifiedExitNode:                ; preds = %else, %then
        ret void
}

define i32 @main() {
entry:
        call void @__main( )
        call void @CheckOutside( i32 2097152, i32 2097152 )
        ret i32 0
}

