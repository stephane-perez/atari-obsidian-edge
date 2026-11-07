/*
 *	compmem.S: GNU as version of lib/compmem.s, for the GCC build
 *
 *	long compare_memory(const void *s,const void *t,long n);
 *		compares memory areas pointed to by s & t, for a length of n
 *		bytes; for best performance, both areas should be word-aligned.
 *		Timings and rationale: see lib/compmem.s.
 *
 *	Translated from lib/compmem.s (Copyright (C) 2002-2017 Roger Burrows),
 *	distributed under the GPL, version 2 or at your option any later
 *	version.  See DOC/LICENSE.TXT.  Only the syntax differs.
 *
 *	register use: clobbers a0,a1,d0,d1
 */

	.text
	.globl	_compare_memory

_compare_memory:
	move.l	4(%sp),%a0		/* a0 -> s */
	move.l	8(%sp),%a1		/* a1 -> t */
	move.l	%a0,%d0
	move.l	%a1,%d1
	eor.b	%d1,%d0			/* combine alignments */
	andi.b	#0x01,%d0		/*  & check */
	bne.s	unaligned		/* different, branch */
/*
 * memory regions have the same alignment
 */
	move.l	12(%sp),%d0		/* get byte count */
	beq.s	equal			/* zero: say they're equal */
	andi.b	#0x01,%d1		/* check t alignment */
	beq.s	aligned			/* aligned, so s must be too */
/*
 * both memory regions are byte-aligned
 */
	cmpm.b	(%a0)+,(%a1)+		/* check first byte, and word-align a0, a1 */
	bne.s	unequal
	subq.l	#1,%d0			/* adjust length */
/*
 * both pointers are word-aligned
 */
aligned:
	move.l	%d0,%d1
	andi.w	#3,%d1			/* d1.w = count of final bytes to compare (0-3) */
	lsr.l	#2,%d0			/* d0 = # of 4-byte compares */
	cmp.l	%d0,%d0			/* force initial equal condition */
	bra.s	2f
					/* outer loop: invoked for _really_ big compares! */
0:	swap	%d0			/* restore low-order word */
					/* inner loop: compares up to 65536*4 (=256K) bytes */
1:	cmpm.l	(%a0)+,(%a1)+		/* check longs */
2:	dbne	%d0,1b
	bne.s	unequal
	swap	%d0			/* d0.w = high-order word of compare count */
	dbra	%d0,0b
					/* completed main loop: handle 0-3 remaining bytes */
	moveq	#0,%d0			/* force initial equal condition (& set advance rc) */
	bra.s	6f
5:	cmpm.b	(%a0)+,(%a1)+		/* check bytes */
6:	dbne	%d1,5b
	bne.s	unequal
	rts				/* return with rc = 0 */
/*
 * memory regions have different alignments
 */
unaligned:
	move.l	12(%sp),%d0		/* d0 = count */
	beq.s	equal			/* zero: say they're equal */
	cmp.l	%d0,%d0			/* force initial equal condition */
	bra.s	2f
					/* outer loop: invoked for big compares! */
0:	swap	%d0			/* restore low-order word */
					/* inner loop: compares up to 65536 (=64K) bytes */
1:	cmpm.b	(%a0)+,(%a1)+		/* check bytes */
2:	dbne	%d0,1b
	bne.s	unequal
	swap	%d0			/* d0.w = high-order word of compare count */
	dbra	%d0,0b
equal:	moveq	#0,%d0			/* all done, set rc = 0 */
	rts
/*
 * unequal: set appropriate rc
 */
unequal:
	bcs.s	0f			/* if carry is set, *s > *t */
	moveq	#-1,%d0			/* *s < *t, set -ve rc */
	rts
0:	moveq	#1,%d0			/* *s > *t, set +ve rc */
	rts
