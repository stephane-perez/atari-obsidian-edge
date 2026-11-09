/*
 *	mediach.S: Lattice C _mediach() for the GCC build
 *
 *	int _mediach(int drive);
 *		forces GEMDOS to notice a media change on 'drive' (0 = A:, ...),
 *		so that it re-reads the boot sector, FATs and directories after
 *		Diamond Edge has modified them with Rwabs().  Returns 0.
 *
 *	Uses the standard method (see the Atari Compendium, "forcing a media
 *	change"): hdv_mediach is hooked to report "definitely changed" for
 *	the drive, hdv_rw to report E_CHNG, and hdv_bpb to unhook everything
 *	when GEMDOS asks for the new BPB; a file open on the drive then makes
 *	GEMDOS go through this sequence.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */

hdv_bpb		= 0x472
hdv_rw		= 0x476
hdv_mediach	= 0x47e

E_CHNG		= -14

	.text
	.globl	__mediach

/*
 * The whole sequence (hooking, Fopen(), unhooking) runs in supervisor
 * mode, exactly as in the Lattice C library that the original binaries
 * were linked with.
 */
__mediach:
	move.l	4(%sp),%d0
	move.w	%d0,drive
	add.b	#'A',%d0
	move.b	%d0,fspec
	movem.l	%d2/%a2,-(%sp)		/* the traps may clobber d0-d2/a0-a2 */

	move.l	#1,-(%sp)		/* Super(1): already in supervisor mode? */
	move.w	#0x20,-(%sp)
	trap	#1
	addq.l	#6,%sp
	clr.l	oldssp
	tst.l	%d0
	bne.s	1f			/* yes (-1): stay as we are */
	clr.l	-(%sp)			/* no: Super(0L) */
	move.w	#0x20,-(%sp)
	trap	#1
	addq.l	#6,%sp
	move.l	%d0,oldssp
1:
	bsr	install

	clr.w	-(%sp)			/* Fopen(fspec,0): GEMDOS checks the media */
	pea	fspec
	move.w	#0x3d,-(%sp)
	trap	#1
	addq.l	#8,%sp
	tst.l	%d0
	bmi.s	2f
	move.w	%d0,-(%sp)		/* opened (unlikely): Fclose() it */
	move.w	#0x3e,-(%sp)
	trap	#1
	addq.l	#4,%sp
2:
	bsr	remove			/* make sure the vectors are restored */

	move.l	oldssp,%d0		/* back to user mode if we switched */
	beq.s	3f
	move.l	%d0,-(%sp)		/* Super(oldssp) */
	move.w	#0x20,-(%sp)
	trap	#1
	addq.l	#6,%sp
3:
	movem.l	(%sp)+,%d2/%a2
	moveq	#0,%d0
	rts

/*
 * supervisor mode: install / remove the hooks
 */
install:
	move.l	hdv_bpb,old_bpb
	move.l	hdv_rw,old_rw
	move.l	hdv_mediach,old_mediach
	move.l	#new_bpb,hdv_bpb
	move.l	#new_rw,hdv_rw
	move.l	#new_mediach,hdv_mediach
	st	hooked
	rts

remove:
	tst.b	hooked
	beq.s	1f
	move.l	old_bpb,hdv_bpb
	move.l	old_rw,hdv_rw
	move.l	old_mediach,hdv_mediach
	sf	hooked
1:	rts

/*
 * long Getbpb(short dev): unhook, then call the original routine
 */
new_bpb:
	move.w	4(%sp),%d0
	cmp.w	drive,%d0
	bne.s	1f
	bsr.s	remove
1:	move.l	old_bpb,-(%sp)
	rts

/*
 * long Rwabs(short rwflag,void *buf,short count,short recno,short dev)
 */
new_rw:
	move.w	14(%sp),%d0
	cmp.w	drive,%d0
	bne.s	1f
	moveq	#E_CHNG,%d0
	rts
1:	move.l	old_rw,-(%sp)
	rts

/*
 * long Mediach(short dev): 2 = media has definitely changed
 */
new_mediach:
	move.w	4(%sp),%d0
	cmp.w	drive,%d0
	bne.s	1f
	moveq	#2,%d0
	rts
1:	move.l	old_mediach,-(%sp)
	rts

	.data
fspec:	.ascii	"X:\\X"
	.byte	0
	.even

	.bss
	.even
old_bpb:	.ds.l	1
old_rw:		.ds.l	1
old_mediach:	.ds.l	1
oldssp:		.ds.l	1
drive:		.ds.w	1
hooked:		.ds.b	1
