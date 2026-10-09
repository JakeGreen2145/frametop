// SPDX-License-Identifier: MIT
#if defined(__aarch64__)
.text
.p2align 2
.global ft_native_portal_call
.hidden ft_native_portal_call
.type ft_native_portal_call,%function
ft_native_portal_call:
    stp x29,x30,[sp,#-32]!
    mov x29,sp
    stp x19,x20,[sp,#16]
    mov x16,x0
    mov x19,x1
    mov x8,x2
    ldp x0,x1,[x19]
    ldp x2,x3,[x19,#16]
    ldp x4,x5,[x19,#32]
    ldp x6,x7,[x19,#48]
    blr x16
    stp x0,x1,[x19]
    stp x2,x3,[x19,#16]
    stp x4,x5,[x19,#32]
    stp x6,x7,[x19,#48]
    mov w0,#1
    ldp x19,x20,[sp,#16]
    ldp x29,x30,[sp],#32
    ret
.size ft_native_portal_call,.-ft_native_portal_call
#endif
.section .note.GNU-stack,"",%progbits
