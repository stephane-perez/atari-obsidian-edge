/*
 *	callstack.S: call a function on another stack, for the GCC build
 *
 *	long oe_call_on_stack(long (*fn)(void *),void *arg,void *stack_top);
 *		calls fn(arg) with the stack pointer set to stack_top, and
 *		returns its result.
 *
 *	Used for the routines that the AES calls (USERDEF objects): the AES
 *	runs them in supervisor mode on its own, small, stack.  GEMlib's VDI
 *	bindings put their parameter arrays on the stack (v_gtext() alone
 *	takes more than 2 KB), which overflowed that stack and overwrote AES
 *	variables: the AES crashed as soon as a menu was dropped down.
 *	(Lattice C's bindings used static arrays.)
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */

	.text
	.globl	_oe_call_on_stack

_oe_call_on_stack:
	move.l	4(%sp),%a0		/* fn */
	move.l	8(%sp),%d0		/* arg */
	move.l	12(%sp),%a1		/* top of the new stack */
	move.l	%sp,-(%a1)		/* save the caller's stack pointer on it */
	move.l	%a1,%sp
	move.l	%d0,-(%sp)
	jsr	(%a0)			/* d0 = fn(arg) */
	addq.l	#4,%sp
	move.l	(%sp),%sp		/* back to the caller's stack */
	rts
