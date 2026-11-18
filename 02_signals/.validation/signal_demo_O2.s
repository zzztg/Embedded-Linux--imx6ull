	.arch armv7-a
	.fpu vfpv3-d16
	.eabi_attribute 28, 1
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.file	"signal_demo.c"
	.text
	.align	1
	.p2align 2,,3
	.syntax unified
	.thumb
	.thumb_func
	.type	handle_stop, %function
handle_stop:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
	ldr	r3, .L3
.LPIC0:
	add	r3, pc
	str	r0, [r3]
	bx	lr
.L4:
	.align	2
.L3:
	.word	.LANCHOR0-(.LPIC0+4)
	.size	handle_stop, .-handle_stop
	.section	.rodata.str1.4,"aMS",%progbits,1
	.align	2
.LC0:
	.ascii	"install signal handler\000"
	.align	2
.LC1:
	.ascii	"PID=%ld; waiting for SIGINT or SIGTERM...\012\000"
	.align	2
.LC2:
	.ascii	"Received signal %d; exiting normally.\012\000"
	.section	.text.startup,"ax",%progbits
	.align	1
	.p2align 2,,3
	.global	main
	.syntax unified
	.thumb
	.thumb_func
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 144
	@ frame_needed = 0, uses_anonymous_args = 0
	ldr	ip, .L22
	movs	r2, #136
	push	{r4, r5, lr}
	movs	r1, #0
	ldr	r5, .L22+4
.LPIC10:
	add	ip, pc
	sub	sp, sp, #148
	add	r0, sp, #4
	ldr	r4, .L22+8
	ldr	r5, [ip, r5]
.LPIC4:
	add	r4, pc
	ldr	r5, [r5]
	str	r5, [sp, #140]
	mov	r5, #0
	bl	memset(PLT)
	ldr	r2, .L22+12
.LPIC1:
	add	r2, pc
	str	r2, [sp]
	bl	sigemptyset(PLT)
	adds	r0, r0, #1
	beq	.L8
	movs	r2, #0
	mov	r1, sp
	movs	r0, #2
	bl	sigaction(PLT)
	adds	r0, r0, #1
	beq	.L8
	movs	r2, #0
	mov	r1, sp
	movs	r0, #15
	bl	sigaction(PLT)
	adds	r0, r0, #1
	beq	.L8
	bl	getpid(PLT)
	ldr	r1, .L22+16
	mov	r2, r0
	movs	r0, #2
.LPIC3:
	add	r1, pc
	bl	__printf_chk(PLT)
	ldr	r3, .L22+20
	ldr	r3, [r4, r3]
	ldr	r4, .L22+24
.LPIC5:
	add	r4, pc
	ldr	r0, [r3]
	bl	fflush(PLT)
	ldr	r3, [r4]
	cbnz	r3, .L9
.L10:
	movs	r0, #1
	bl	sleep(PLT)
	ldr	r3, [r4]
	cmp	r3, #0
	beq	.L10
.L9:
	ldr	r3, .L22+28
	movs	r0, #2
	ldr	r1, .L22+32
.LPIC7:
	add	r3, pc
.LPIC8:
	add	r1, pc
	ldr	r2, [r3]
	bl	__printf_chk(PLT)
	movs	r0, #0
.L5:
	ldr	r2, .L22+36
	ldr	r3, .L22+4
.LPIC9:
	add	r2, pc
	ldr	r3, [r2, r3]
	ldr	r2, [r3]
	ldr	r3, [sp, #140]
	eors	r2, r3, r2
	mov	r3, #0
	bne	.L21
	add	sp, sp, #148
	@ sp needed
	pop	{r4, r5, pc}
.L21:
	bl	__stack_chk_fail(PLT)
.L8:
	ldr	r0, .L22+40
.LPIC2:
	add	r0, pc
	bl	perror(PLT)
	movs	r0, #1
	b	.L5
.L23:
	.align	2
.L22:
	.word	_GLOBAL_OFFSET_TABLE_-(.LPIC10+4)
	.word	__stack_chk_guard(GOT)
	.word	_GLOBAL_OFFSET_TABLE_-(.LPIC4+4)
	.word	handle_stop-(.LPIC1+4)
	.word	.LC1-(.LPIC3+4)
	.word	stdout(GOT)
	.word	.LANCHOR0-(.LPIC5+4)
	.word	.LANCHOR0-(.LPIC7+4)
	.word	.LC2-(.LPIC8+4)
	.word	_GLOBAL_OFFSET_TABLE_-(.LPIC9+4)
	.word	.LC0-(.LPIC2+4)
	.size	main, .-main
	.bss
	.align	2
	.set	.LANCHOR0,. + 0
	.type	stop_signal, %object
	.size	stop_signal, 4
stop_signal:
	.space	4
	.ident	"GCC: (Ubuntu 15.2.0-16ubuntu1) 15.2.0"
	.section	.note.GNU-stack,"",%progbits
