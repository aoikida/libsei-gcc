/* -*- asm -*- */
/* -----------------------------------------------------------------------------
** Copyright (c) 2013 Diogo Behrens
** Distributed under the MIT license. See accompanying file LICENSE.
** -------------------------------------------------------------------------- */

/*
** There are two things we do in assembly:
** 1. we save the caller stack boundaries to be able to filter out calls to
**    local variables inside tmasco.
** 2. we save the callee-save registers, so that we can restore them correctly
**    when switching to the second execution of the traversal.
**
** To understand what this file does, check the excellent introduction to x64
**  assembly: http://cs.nyu.edu/courses/fall11/CSCI-GA.2130-001/x64-intro.pdf
**
**/

/* --- _ITM_beginTransaction ------------------------------------------------ */
	.text
	.align	4

	// globl makes the label visible to the linker while .type
	// defines the type of the labal, ie, a function
	.globl	_ITM_beginTransaction
	.type   _ITM_beginTransaction, @function

_ITM_beginTransaction:
	/* A function prologue typically looks like this:

	*	pushq	%rbp
	*	movq  	%rsp, %rbp
	*	subq	FRAME_SIZE, %rsp

	The callee saves the caller's rbp and acquires space from the
	stack.

	We cannot however the push the rbp before we save the caller's
	rsp.  This is necessary, because when tmasco_commit will be
	called, we will jump back to here for the second run of the
	handler without knowing how large the stack will be a that
	point, so we can simply pop everything up to the caller's rsp.

	We don't need to overwrite rbp since we simply perform another
	call. The callee will have to take care of rbp if it wants to
	modify it.

	Note that we are free to use all caller-save registers: rax,
	rcx, rdx, rdi, rsi, r8 up to r11. By convention the return
	value is given in rax and the first 6 arguments are passed in
	rdi, rsi, rdx, rcx, r8, r9 (in this order).

	Once called _ITM_beginTransaction, the return address was
	pushed into the stack. Thus the previous stack pointer is the
	current (with return address) plus 8 bytes. That has to be
	considered in the following load.
	*/
	leaq	8(%rsp), %rax

	// We could push each element to the stack but then each push
	// would increment the rsp. Instead, we add to rsp at once and
	// then move the values into the stack.
	subq	$56, %rsp

	// now we save the rbp as usual
	movq	%rbp,   (%rsp)

	// Save context: push all callee-save registers into stack
	movq	%rbx,  8(%rsp)
	movq	%r12, 16(%rsp)
	movq	%r13, 24(%rsp)
	movq	%r14, 32(%rsp)
	movq	%r15, 40(%rsp)

	// Save rsp of caller as explained above
	movq	%rax, 48(%rsp)

	// We are now ready to call tmasco_begin: The current %rsp
	// marks the begin of the context to be saved.  We have no use
	// for the first argument passed to beginTransaction, so we
	// overwrite it.
	movq	%rsp, %rdi
	call	tmasco_begin

	/* function epilogue typically looks like this:

	*	movq	return_value, %rax
	*	movq	%rbp, %rsp
	*	popq	%rbp
	*	ret

	The return value is already set by the tmasco_begin, so we
	don't overwrite rax. We also don't restore rbp since we
	haven't modified it. We just pop all values added to the
	stack.
	*/
	addq	$56, %rsp

	// We are ready to return
	ret

_ITM_beginTransaction.end:
	// Every function terminated with a .size line which tells the
	// assembler to calculate the distance from here up to the
	// _ITM_beginTransaction label.
	.size	_ITM_beginTransaction, .- _ITM_beginTransaction


/* --- tmasco_switch -------------------------------------------------------- */
	.text
	.align	4
	.globl	tmasco_switch
	.type	tmasco_switch, @function

tmasco_switch:

	/* Our first argument rdi points to a copy of the above
	context. Our task is simply to restore all registers return to
	the same address to which _ITM_beginTransaction returned. */
	movq	  (%rdi), %rbp	// restore stack base
	movq	 8(%rdi), %rbx	// restore callee-save register
	movq	16(%rdi), %r12	// restore callee-save register
	movq	24(%rdi), %r13	// restore callee-save register
	movq	32(%rdi), %r14	// restore callee-save register
	movq	40(%rdi), %r15	// restore callee-save register
	movq	48(%rdi), %rsp	// restore stack pointer
	movq	56(%rdi), %rdx	// copy return address

	// We also need to return the same value as _ITM_beginTransaction
	// returned. So our second argument, ie %rsi, represents the return
	// value. Note that _ITM_beginTransaction returns a 32 bit value.
	movl	%esi, %eax

	// now return
	jmpq *%rdx

tmasco_switch.end:
	.size	tmasco_switch, .- tmasco_switch
