; RUN: llc < %s
; UNSUPPORTED: target=evm{{.*}}

@.str_1 = internal constant [16 x i8] c"%d %d %d %d %d\0A\00"           ; <ptr> [#uses=1]
@XA = external global i32               ; <ptr> [#uses=1]
@XB = external global i32               ; <ptr> [#uses=1]

declare i32 @printf(ptr, ...)

define void @test(i32 %A, i32 %B, i32 %C, i32 %D) {
entry:
        %t1 = icmp slt i32 %A, 0                ; <i1> [#uses=1]
        br i1 %t1, label %less, label %not_less

less:           ; preds = %entry
        br label %not_less

not_less:               ; preds = %less, %entry
        %t2 = phi i32 [ sub (i32 ptrtoint (ptr @XA to i32), i32 ptrtoint (ptr @XB to i32)), %less ], [ sub (i32 ptrtoint (ptr @XA to i32), i32 ptrtoint (ptr @XB to i32)), %entry ]               ; <i32> [#uses=1]
        %tmp.39 = call i32 (ptr, ...) @printf( ptr @.str_1, i32 %t2 )      ; <i32> [#uses=0]
        ret void
}

