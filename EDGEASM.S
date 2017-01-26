;
; edgeasm.s
;
; implements:
;	void cold_boot(void)
;	void fast_copy(void *to,void *from,long size)
;
; define SHORTINT iff you are compiling with short ints
;
;
; revised by rfb nov/2002:
;	. improve fast_copy() speed by handling 68000/68030 differently
;	  (among other things)
;	. change order of fast_copy() args to be the same as as memcpy
;

memvalid	equ		$420
resvalid	equ		$426
_sysbase	equ		$4f2

	section	text

	xdef	_cold_boot,_fast_copy
	xref	_cpu_type

;
; cold_boot
;
_cold_boot:
	clr.l	-(sp)
	move.w	#$20,-(sp)
	trap	#1
	addq	#6,sp
	move.l	_sysbase,a0
	clr.l	memvalid
	clr.l	resvalid
	jmp  	(a0)

;
; fast_copy
;
_fast_copy:
	move.l	4(a7),a1
	move.l	8(a7),a0
	move.l	12(a7),d0
	beq.b	exit

;
; setting up for the different types of copy is in itself costly,
; so it makes sense to just do a simple copy for small counts.
; by experiment, we choose a cut-off point of 32.
;
	cmpi.l	#32,d0
	blo		finish

;
; processing depends on cpu type ...
;
	cmpi.w	#30,_cpu_type
	bcc		handle_68030

;
; *** handle 68000 (& 68010, 68020 ...) ***
;

;
; copy 1 byte if a0 is not word aligned
;
	move.l	a0,d2
	andi.b	#1,d2
	beq.b	choose_copy_size
	move.b	(a0)+,(a1)+
	subq.l	#1,d0

;
; we have to perform a copy: choose the move routine(s)
; based on the alignment of a1
;
choose_copy_size:
	move.l	a1,d2
	andi.b	#1,d2
	beq.b	copyword
	bra.b	copybyte

;
; *** handle 68030 (& 68040, 68060 ...) ***
;
handle_68030:
;
; for 68030 and above, there is no requirement for long-word accesses
; to be aligned.  for these systems we do as many longword copies as
; possible, followed by word copies, followed by byte copies
;
copylong:
	bsr		copy_by_longword
copyword:
	bsr		copy_by_word
copybyte:
	bsr		copy_by_byte

;
; the following rolled, single-byte copy loop finishes off the copy
;
finish:
	bra.b	decrement
copy_1_byte:
	move.b	(a0)+,(a1)+
decrement:
	dbra	d0,copy_1_byte
exit:
	rts


;
;Note: In an earlier version of this module, the faster DBRA instruction
; 	  was used. However, it imposed a maximum of (2^15-1)*16 on src_len
; 	  so I changed it to the sub/bne combination. I suppose I could have
; 	  nested DBRAs but this is simpler and not much slower.
;
copy_by_byte:
	move.l	d0,d1
	lsr.l	#4,d1
	beq.b	copybyte_exit
	move.l	d1,d2
	lsl.l	#4,d2
	sub.l	d2,d0
copy_16_bytes:							  
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	move.b	(a0)+,(a1)+
	subq.l	#1,d1
	bne.b	copy_16_bytes
copybyte_exit:
	rts

copy_by_word:
	move.l	d0,d1
	lsr.l	#5,d1
	beq.b	copyword_exit
	move.l	d1,d2
	lsl.l	#5,d2
	sub.l	d2,d0
copy_16_words:
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	move.w	(a0)+,(a1)+
	subq.l	#1,d1
	bne.b	copy_16_words
copyword_exit:
	rts

copy_by_longword:
	move.l	d0,d1
	lsr.l	#6,d1
	beq.b	copylong_exit
	move.l	d1,d2
	lsl.l	#6,d2
	sub.l	d2,d0
copy_16_longwords:
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	move.l	(a0)+,(a1)+
	subq.l	#1,d1
	bne.b	copy_16_longwords
copylong_exit:
	rts

	end
