.data
.text
.globl _start
_start:
	movq %rsp, %rbp
	xorb %ah, %ah
	addb $3, %ah
	movq $60, %rax
	xorq %rdi, %rdi
	syscall
