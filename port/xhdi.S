/*
 *	xhdi.S: two XHDI calls, for the GCC build
 *
 *	long oe_xhdi_version(long entry);
 *		XHGetVersion()
 *	long oe_xhdi_inqdev2(long entry,long dev,unsigned long *start,char *partid);
 *		XHInqDev2(dev,NULL,NULL,start,NULL,NULL,partid)
 *
 *	entry is the value of the 'XHDI' cookie.  XHDI takes 16-bit
 *	parameters on the stack (the opcode first), which GCC's 32-bit ints
 *	cannot pass directly.  Must be called in supervisor mode (Supexec).
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */

	.text
	.globl	_oe_xhdi_version
	.globl	_oe_xhdi_inqdev2

_oe_xhdi_version:
	move.l	4(%sp),%a0
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	clr.w	-(%sp)			/* XHGetVersion */
	jsr	(%a0)
	addq.l	#2,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

_oe_xhdi_inqdev2:
	move.l	4(%sp),%a0		/* entry */
	move.l	8(%sp),%d0		/* dev */
	move.l	12(%sp),%d1		/* start */
	move.l	16(%sp),%a1		/* partid */
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	move.l	%a1,-(%sp)		/* partid */
	clr.l	-(%sp)			/* blocks */
	clr.l	-(%sp)			/* bpb */
	move.l	%d1,-(%sp)		/* start_sector */
	clr.l	-(%sp)			/* minor */
	clr.l	-(%sp)			/* major */
	move.w	%d0,-(%sp)		/* bios_device */
	move.w	#12,-(%sp)		/* XHInqDev2 */
	jsr	(%a0)
	lea	28(%sp),%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts
