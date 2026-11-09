/*
 *	edgeasm.S: GNU as version of edgeasm.s, for the GCC build
 *
 *	implements:
 *		void cold_boot(void)
 *		void fast_copy(void *to,void *from,long size)
 *
 *	Translated from the Lattice C assembler source edgeasm.s
 *	(Copyright (C) 2002-2017 Roger Burrows), distributed under the GPL,
 *	version 2 or at your option any later version.  See DOC/LICENSE.TXT.
 *
 *	Differences from edgeasm.s:
 *	  . GNU as syntax
 *	  . fast_copy() saves and restores d2, which GCC expects to be
 *	    preserved across calls (edgeasm.s clobbered it)
 */

memvalid	= 0x420
resvalid	= 0x426
_sysbase	= 0x4f2

	.text

	.globl	_cold_boot
	.globl	_fast_copy

/*
 * cold_boot
 */
_cold_boot:
	clr.l	-(%sp)
	move.w	#0x20,-(%sp)		/* Super(0L) */
	trap	#1
	addq.l	#6,%sp
	move.l	_sysbase,%a0
	clr.l	memvalid
	clr.l	resvalid
	jmp	(%a0)

/*
 * fast_copy
 */
_fast_copy:
	move.l	%d2,-(%sp)
	move.l	8(%sp),%a1
	move.l	12(%sp),%a0
	move.l	16(%sp),%d0
	beq.s	exit

/*
 * setting up for the different types of copy is in itself costly,
 * so it makes sense to just do a simple copy for small counts.
 * by experiment, we choose a cut-off point of 32.
 */
	cmpi.l	#32,%d0
	blo	finish

/*
 * processing depends on cpu type ...
 */
	cmpi.w	#30,_cpu_type
	bcc	handle_68030

/*
 * *** handle 68000 (& 68010, 68020 ...) ***
 *
 * copy 1 byte if a0 is not word aligned
 */
	move.l	%a0,%d2
	andi.b	#1,%d2
	beq.s	choose_copy_size
	move.b	(%a0)+,(%a1)+
	subq.l	#1,%d0

/*
 * we have to perform a copy: choose the move routine(s)
 * based on the alignment of a1
 */
choose_copy_size:
	move.l	%a1,%d2
	andi.b	#1,%d2
	beq.s	copyword
	bra.s	copybyte

/*
 * *** handle 68030 (& 68040, 68060 ...) ***
 *
 * for 68030 and above, there is no requirement for long-word accesses
 * to be aligned.  for these systems we do as many longword copies as
 * possible, followed by word copies, followed by byte copies
 */
handle_68030:
copylong:
	bsr	copy_by_longword
copyword:
	bsr	copy_by_word
copybyte:
	bsr	copy_by_byte

/*
 * the following rolled, single-byte copy loop finishes off the copy
 */
finish:
	bra.s	decrement
copy_1_byte:
	move.b	(%a0)+,(%a1)+
decrement:
	dbra	%d0,copy_1_byte
exit:
	move.l	(%sp)+,%d2
	rts

copy_by_byte:
	move.l	%d0,%d1
	lsr.l	#4,%d1
	beq.s	copybyte_exit
	move.l	%d1,%d2
	lsl.l	#4,%d2
	sub.l	%d2,%d0
copy_16_bytes:
	.rept	16
	move.b	(%a0)+,(%a1)+
	.endr
	subq.l	#1,%d1
	bne.s	copy_16_bytes
copybyte_exit:
	rts

copy_by_word:
	move.l	%d0,%d1
	lsr.l	#5,%d1
	beq.s	copyword_exit
	move.l	%d1,%d2
	lsl.l	#5,%d2
	sub.l	%d2,%d0
copy_16_words:
	.rept	16
	move.w	(%a0)+,(%a1)+
	.endr
	subq.l	#1,%d1
	bne.s	copy_16_words
copyword_exit:
	rts

copy_by_longword:
	move.l	%d0,%d1
	lsr.l	#6,%d1
	beq.s	copylong_exit
	move.l	%d1,%d2
	lsl.l	#6,%d2
	sub.l	%d2,%d0
copy_16_longwords:
	.rept	16
	move.l	(%a0)+,(%a1)+
	.endr
	subq.l	#1,%d1
	bne.s	copy_16_longwords
copylong_exit:
	rts
