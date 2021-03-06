.section .text
.global setjmp
.type setjmp, @function
setjmp:
    # jmp_buf passed in %rdi

    movq %rbx, 0(%rdi)
    movq %rbp, 8(%rdi)
    movq %r12, 16(%rdi)
    movq %r13, 24(%rdi)
    movq %r14, 32(%rdi)
    movq %r15, 40(%rdi)

    leaq 8(%rsp), %rax
    movq %rax, 48(%rdi) # RSP
    
    movq (%rsp), %rax
    movq %rax, 56(%rdi) # RIP

    # Return 0
    xorl %eax, %eax
    ret
