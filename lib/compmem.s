;
;	compmem.s: source for compare_memory()
;
;	Copyright (C) 2002-2017 Roger Burrows
;
;	This file is distributed under the GPL, version 2 or at your
;	option any later version.  See DOC\LICENSE.TXT for details.
;

;
;	modification history
;	--------------------
;	version:	1.0
;	author:		Roger Burrows
;	date:		11 January 2003
;	summary:	Original version, taken from backutil.s in cdbackup source.
;				Compare_memory() is faster than memcmp() when comparing large
;				areas; see comments below for some timings.
;
;	version:	2.57
;	author:		roger burrows
;	date:		january/2017
;	summary:	prepare for public release
;

;
;	note: assembly language is compatible with Lattice C v5 ASM
;
;	function prototype:
;		long compare_memory(const void *s,const void *t,long n);
;			compares memory areas pointed to by s & t, for a length of n
;			bytes; for best performance, both areas should be word-aligned
;
;

		csect	compmem,code,,0,4
*
***	compare_memory()
*
;	much faster than inline or library memcmp() for most compare lengths,
;	especially when the areas compared have the same alignment (word or byte).
;
;	ST:	inline memcmp(), same alignment:
;			Compare_memory() is faster for length >= 8; the speedup
;			factor increases monotonically from 2.0 at length 16 to
;			7.0 for length>=4096.
;		inline memcmp(), different alignment:
;			Compare_memory() is faster for length >= 8; the speedup
;			factor increases monotonically from 1.5 at length 16 to
;			2.4 for length>=4096.
;		subroutine memcmp(), same alignment:
;			Compare_memory() is faster for length >= 16; the speedup
;			factor increases monotonically from 1.5 at length 32 to
;			3.0 for length>=4096.
;		subroutine memcmp(), different alignment:
;			Compare_memory() is marginally _slower_ up to length = 32;
;			for greater lengths, it is essentially the same speed.
;
;	TT:	inline memcmp(), same alignment:
;			Compare_memory() is faster for length >= 16; the speedup
;			factor varies between 1.5 & 3.5 in an irregular fashion,
;			depending also on whether both areas are byte- or word-aligned.
;			A typical value is 2.5.
;		inline memcmp(), different alignment:
;			Compare_memory() is faster for length >= 32, reaching a
;			maximum value of about 1.8 times for a length of 32K.
;			For lengths of 64K or more, the speedup factor is only
;			about 1.05.
;		subroutine memcmp(), same alignment:
;			Compare_memory() is faster for length >= 64; the speedup
;			factor varies between 1.5 & 3.5 in an irregular fashion,
;			depending also on whether both areas are byte- or word-aligned.
;			A typical value is 2.5.
;		subroutine memcmp(), different alignment:
;			Compare_memory() is marginally _slower_ up to length = 256;
;			for greater lengths, it is essentially the same speed.
;
;
;	register use:
;		clobbers a0,a1,d0,d1

		xdef	compare_memory

compare_memory:
		move.l	4(sp),a0		;a0 -> s
		move.l	8(sp),a1		;a1 -> t
		move.l	a0,d0
		move.l	a1,d1
		eor.b	d1,d0			;combine alignments			
		andi.b	#$01,d0			; & check
		bne.b	unaligned		;different, branch
;
; memory regions have the same alignment
;
		move.l	12(sp),d0		;get byte count
		beq.b	equal			;zero: say they're equal
		andi.b	#$01,d1			;check t alignment
		beq.b	aligned			;aligned, so s must be too
;
; both memory regions are byte-aligned
;
		cmpm.b	(a0)+,(a1)+		;check first byte, and word-align a0, a1
		bne.b	unequal
		subq.l	#1,d0			;adjust length
;
; both pointers are word-aligned
;
aligned:
		move.l	d0,d1
		andi.w	#3,d1			;d1.w = count of final bytes to compare (0-3)
		lsr.l	#2,d0			;d0 = # of 4-byte compares
		cmp.l	d0,d0			;force initial equal condition
		bra.b	2$
						;outer loop: invoked for _really_ big compares!
0$:		swap d0					;restore low-order word
						;inner loop: compares up to 65536*4 (=256K) bytes
1$:		cmpm.l	(a0)+,(a1)+		;check longs
2$:		dbne	d0,1$
		bne.b	unequal
		swap	d0				;d0.w = high-order word of compare count
		dbra	d0,0$
						;completed main loop: handle 0-3 remaining bytes
		moveq	#0,d0			;force initial equal condition (& set advance rc)
		bra.b	6$
5$:		cmpm.b	(a0)+,(a1)+		;check bytes
6$:		dbne	d1,5$
		bne.b	unequal
		rts						;return with rc = 0
;
;	memory regions have different alignments
;
unaligned:
		move.l	12(sp),d0		;d0 = count
		beq.b	equal			;zero: say they're equal
		cmp.l	d0,d0			;force initial equal condition
		bra.b	2$
						;outer loop: invoked for big compares!
0$:		swap d0					;restore low-order word
						;inner loop: compares up to 65536 (=64K) bytes
1$:		cmpm.b	(a0)+,(a1)+		;check bytes
2$:		dbne	d0,1$
		bne.b	unequal
		swap	d0				;d0.w = high-order word of compare count
		dbra	d0,0$
equal:	moveq	#0,d0			;all done, set rc = 0
		rts
;
; unequal: set appropriate rc
;
unequal:
		bcs.b	0$				;if carry is set, *s > *t
		moveq	#-1,d0			;*s < *t, set -ve rc
		rts
0$:		moveq	#1,d0			;*s > *t, set +ve rc
		rts

		end
