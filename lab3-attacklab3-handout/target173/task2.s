.section .text
.globl _start

_start:
    movq %rsp,%rax 
    movq %rax,%rdi 
    popq %rax 
    movl %eax,%edx 
    movl %edx,%ecx 
    movl %ecx,%esi 
    lea (%rdi, %rsi, 1), %rax
    movq %rax,%rdi 

