.data
.text
.globl _start
_start:
	movq %rsp, %rbp
	movb $0, %ah
	addb $3, %ah
	movq $60, %rax
	movq $0, %rdi
	syscall
