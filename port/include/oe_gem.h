/*
 *	oe_gem.h: common part of <aes.h> and <vdi.h>
 *
 *	Part of the GCC port layer of Obsidian Edge: replaces a header of the
 *	Lattice C 5 library that the Diamond Edge sources were written for.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_GEM_H
#define OE_GEM_H

#include "portab.h"

/*
 *	The sources define their own grect_to_array() (returning void) and
 *	their own RSXHDR: hide GEMlib's versions.
 */
#define grect_to_array	gemlib_grect_to_array
#define __RSXHDR
#define __GEMLIB_OLDNAMES
#include <gem.h>
#undef grect_to_array

/* GEMlib error code with the same name as an object of EDGE.RSC (edge.h) */
#undef MISC_ERROR

/* colour names of the Lattice C <vdi.h> */
#ifndef WHITE
#define WHITE		G_WHITE
#define BLACK		G_BLACK
#define RED			G_RED
#define GREEN		G_GREEN
#define BLUE		G_BLUE
#define CYAN		G_CYAN
#define YELLOW		G_YELLOW
#define MAGENTA		G_MAGENTA
#define LWHITE		G_LWHITE
#define LBLACK		G_LBLACK
#define LRED		G_LRED
#define LGREEN		G_LGREEN
#define LBLUE		G_LBLUE
#define LCYAN		G_LCYAN
#define LYELLOW		G_LYELLOW
#define LMAGENTA	G_LMAGENTA
#endif

#ifndef FL3DMASK
#define FL3DMASK	OF_FL3DMASK
#endif

/*
 *	Lattice C AES bindings that differ from GEMlib's
 *
 *	ADDR() splits an address into the two 16-bit words the AES expects,
 *	as in wind_set(h,WF_NAME,ADDR(title)).  The extra level of macro
 *	expansion in wind_set() makes ADDR() expand into two arguments before
 *	they are counted.
 */
#define ADDR(a)		(short)((unsigned long)(a)>>16),(short)((unsigned long)(a)&0xffff)

#undef wind_set
#define wind_set(...)				OE_WIND_SET(__VA_ARGS__,0,0,0,0)
#define OE_WIND_SET(...)			OE_WIND_SET_(__VA_ARGS__)
#define OE_WIND_SET_(h,f,a,b,c,d,...) mt_wind_set(h,f,a,b,c,d,aes_global)

/* Lattice passes the timer interval as two 16-bit words (low, high) */
#undef evnt_multi
#define evnt_multi(ty,cl,bu,st,f1,x1,y1,w1,h1,f2,x2,y2,w2,h2,msg,lo,hi,mx,my,mb,ks,kr,br) \
	mt_evnt_multi(ty,cl,bu,st,f1,x1,y1,w1,h1,f2,x2,y2,w2,h2,msg, \
		((unsigned long)(unsigned short)(hi)<<16)|(unsigned short)(lo), \
		mx,my,mb,ks,kr,br,aes_global)

#undef evnt_timer
#define evnt_timer(lo,hi)	mt_evnt_timer(((unsigned long)(unsigned short)(hi)<<16)|(unsigned short)(lo),aes_global)

/* Lattice C names of wind_get()/wind_set() fields */
#ifndef WF_CXYWH
#define WF_CXYWH	WF_CURRXYWH
#endif

/* text alignments, as defined by xrsrc.c when TXT_NORMAL is undefined */
#ifndef ALI_LEFT
#define ALI_LEFT	0
#define ALI_CENTER	1
#define ALI_RIGHT	2
#define ALI_BASE	0
#define ALI_HALF	1
#define ALI_ASCENT	2
#define ALI_BOTTOM	3
#define ALI_DESCENT	4
#define ALI_TOP		5
#endif

/* Lattice C AES utility functions (port/lattice.c) */
void objc_xywh(OBJECT *tree,short obj,GRECT *rect);	/* screen rectangle of obj */
int rc_inside(short x,short y,const GRECT *rect);	/* TRUE if (x,y) is in rect */

/* Lattice C name of the AES global array */
#define _AESglobal	aes_global

#endif
