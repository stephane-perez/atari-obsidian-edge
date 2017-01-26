/******************************************************************************
 * XRSRC.C
 *
 *	Extended Resource-Manager: RSC-Files can now be up to 4GB in length.
 *
 *	This source supports multiple resource files; just use a separate 15 WORD
 *	array to handle each one.  The functions that need the array are:
 *		xrsrc_load(), xrsrc_free(), xrsrc_gaddr(), xrsrc_saddr()
 *
 *	You *must* call init_xrsrc() before calling any of the other functions;
 *	when doing so, you may pass 0 instead of the current workstation VDI
 *	handle iff there are no colour icons in the resource(s) you will load.
 *
 *	You *may* call term_xrsrc() at program termination (although at the
 *	moment it does nothing).
 *
 *	This Source is copyrighted material by
 *		Oliver Groeger
 *		Graf-Konrad-Str.25
 *		8000 Munich 40
 *		Germany
 *
 *
 * Version	:  1.00
 * Date 	:  Aug 15th 1991
 * Author	:  Oliver Groeger
 *
 * Version	:  1.10
 * Datum	:  27 Sept. 1992
 * Autor	:  Olaf Meisiek
 * énderung :  - xrsrc_load lÑdt jetzt auch das alte Atari-RCS-Format
 *			   - TOS-unabhÑngige Farbicon-UnterstÅtzung hinzugefÅgt
 *
 * Version:	1.20
 * Date:	2 December 2002
 * Author:  Roger Burrows
 * Changes:   -	use aes.h/vdi.h instead of xaes.h/xvdi.h: add defines for
 *				TXT_NORMAL etc, change/add casts for ob_spec variable and
 *				others
 *			  -	remove CONST (not defined in portab.h); change LOCAL to
 *				static (different definition in portab.h)
 *			  -	remove usage of compatibility macro in prototypes; change
 *			 	names of MFDB structure variables to standard ones ...
 *			  -	move init_xrsrc(), term_xrsrc() so that they are still
 *				generated even if COLOR_ICONS is FALSE
 *			  -	add __saveds for xdraw_cicon (called from AES)
 *			  -	xfill_farbtbl(), test_rez(): rewrite to increase readability;
 *				many comments added
 *			  -	xfix_icon(): old_len & rest_len were not initialised for
 *				graphics with >8 bit planes.  This could cause various
 *				problems including crashes due to stack overlays.
 *			  -	xdraw_cicon(): fill attributes were not preserved; in
 *				particular, the writing mode was always left as transparent.
 *				This could cause display problems.
 *			  -	rs_obfix(): adjust height of USERDEF to be the same as the
 *				height of the monochrome icon in the CICONBLK
 *			  -	and the usual non-significant reformatting
 *
 * Issues:	form_error() is sometimes issued for initialisation errors, and
 *			sometimes not:
 *				rs_read(): sometimes
 *
 * Note:	must be compiled with stack-checking disabled, because
 *			xdraw_cicon() is called from the AES
 *
 * Anm. 	: Um diese Routinen benutzen zu dÅrfen, muû man im
 *			  Besitz eines Interface Originals sein!
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <portab.h>
#include <string.h>
#include <tos.h>
#include <vdi.h>
#include "xrsrc.h"

#ifdef MALLOC_DEBUG
	#include "trmalloc/trmalloc.h"	/* these two are just for trmalloc() stuff ... */
	#define MODULE_NUMBER	22
#endif

/****** Compilerswitches ******************************************************/

#define COLOR_ICONS TRUE		/* special colour icon support?		*/
#define SAVE_MEMORY FALSE		/* save memory for colour icons?	*/

#ifdef __TURBOC__
#pragma warn -sig				/* Warning "Conversion may lose significant digits" ausschalten */
#endif

#ifndef TXT_NORMAL
	#define TXT_NORMAL		 0x0000 /* text effects */
	#define TXT_THICKENED	 0x0001
	#define TXT_LIGHT		 0x0002
	#define TXT_SKEWED		 0x0004
	#define TXT_UNDERLINED	 0x0008
	#define TXT_OUTLINED	 0x0010
	#define TXT_SHADOWED	 0x0020

	#define ALI_LEFT		 0		/* horizontal text alignment */
	#define ALI_CENTER		 1
	#define ALI_RIGHT		 2

	#define ALI_BASE		 0		/* vertical text alignment */
	#define ALI_HALF		 1
	#define ALI_ASCENT		 2
	#define ALI_BOTTOM		 3
	#define ALI_DESCENT 	 4
	#define ALI_TOP 		 5
#endif


/****** TYPES ****************************************************************/

#ifndef __MYDIAL__
typedef struct {
	USERBLK ublk;
	UWORD old_type;
} OBBLK;
#endif

/****** VARIABLES ************************************************************/

static WORD xgl_wbox, xgl_hbox;
static GRECT xdesk;

static WORD *rs_global;
static RSXHDR *rs_hdr;
static RSXHDR *hdr_buf;

#if COLOR_ICONS == TRUE
static WORD farbtbl[256][32];
static ULONG farbtbl2[256];
static WORD rgb_palette[256][4];
static WORD is_palette;
static WORD xpixelbytes;
static WORD xscrn_planes;
static WORD xvdi_handle;
#endif

/****** FUNCTIONS ************************************************************/

static VOID rs_obfix(OBJECT *rs_otree,WORD rs_oobject);
static VOID rs_sglobal(WORD *base);
static WORD rs_free(WORD *base);
static WORD rs_gaddr(WORD *base,WORD re_gtype,WORD re_gindex,OBJECT **re_gaddr);
static WORD rs_sadd(WORD *base,WORD rs_stype,WORD rs_sindex,OBJECT *re_saddr);
static WORD rs_load(WORD *global,BYTE *fname);
static VOID *get_address(WORD type,WORD index);
static VOID *get_sub(WORD index,LONG offset,WORD size);
static WORD rs_read(WORD *global,BYTE *fname);
static VOID rs_fixindex(WORD *global);
static VOID do_rsfix(ULONG rs_size);
static VOID fix_treeindex(VOID);
static VOID fix_object(VOID);
static VOID fix_tedinfo(VOID);
static VOID fix_nptr(LONG index,WORD ob_type);
static WORD fix_ptr(WORD type,LONG index);
static WORD fix_long(LONG *lptr);
static VOID fix_chp(WORD *pcoord,WORD flag);

static VOID do_ciconfix(ULONG header,RSXHDR *rsxhdr,LONG rs_len);
static WORD xadd_cicon(CICONBLK *cicnblk,OBJECT *obj,WORD nub);
static VOID draw_bitblk(WORD *p,WORD x,WORD y,WORD w,WORD h,WORD num_planes,WORD mode,WORD *index);
static VOID xfix_cicon(UWORD *col_data,LONG len,WORD old_planes,WORD new_planes,MFDB *s);
static VOID std_to_byte(UWORD *col_data,LONG len,WORD old_planes,ULONG *farbtbl2,MFDB *s);
static VOID xrect2array(GRECT *rect,WORD *array);
static WORD test_rez(VOID);
static VOID xfill_farbtbl(VOID);
static WORD fill_cicon_liste(LONG *cicon_liste,ULONG header,RSXHDR *rsxhdr);
static int __saveds xdraw_cicon(PARMBLK *pb);


/*****************************************************************************/

GLOBAL BOOLEAN init_xrsrc(WORD vdi_handle,GRECT *desk,WORD gl_wbox,WORD gl_hbox)
{
	xvdi_handle = vdi_handle;
	xdesk = *desk;
	xgl_wbox = gl_wbox;
	xgl_hbox = gl_hbox;

	return TRUE;
}

/*****************************************************************************/

GLOBAL VOID term_xrsrc(VOID)
{
}

/*****************************************************************************/

GLOBAL WORD xrsrc_load(BYTE *re_lpfname,WORD *pglobal)
{
	return rs_load(pglobal,re_lpfname);
}

/*****************************************************************************/

GLOBAL WORD xrsrc_free(WORD *pglobal)
{
	return rs_free(pglobal);
}

/*****************************************************************************/

GLOBAL WORD xrsrc_gaddr(WORD re_gtype,WORD re_gindex,VOID *re_gaddr,WORD *pglobal)
{
	return rs_gaddr(pglobal,re_gtype,re_gindex,re_gaddr);
}

/*****************************************************************************/

GLOBAL WORD xrsrc_saddr(WORD re_stype,WORD re_sindex,VOID *re_saddr,WORD *pglobal)
{
	return rs_sadd(pglobal,re_stype,re_sindex,re_saddr);
}

/*****************************************************************************/

GLOBAL WORD xrsrc_obfix(OBJECT *re_otree,WORD re_oobject)
{
	rs_obfix(re_otree,re_oobject);

	return TRUE;
}

/*****************************************************************************/

static VOID rs_obfix(OBJECT *rs_otree,WORD rs_oobject)
{
WORD *coord;
WORD tmp = FALSE;
WORD count = 0;
OBJECT *obj = &rs_otree[rs_oobject];

	coord = &obj->ob_x;

	while (count++ < 4) {
		fix_chp(coord++,tmp);
		tmp = tmp ? FALSE : TRUE;
	}

#ifdef COLOR_ICONS
	/*
	 *	rfb: fix up the object height for colour icons being displayed as userdefs.
	 *	this is a good time to do this because the object height is now in pixels.
	 *
	 *	we added this because the height of the object seems to be set to twice
	 *	the actual height (four times in medium-res) for some reason.  code similar
	 *	to this was in fix_objs() which we no longer use ...
	 */
	if ((obj->ob_type & 0xff) == G_USERDEF) {
		OBBLK *ub, *ub_start, *ub_end;

		ub = (OBBLK *)obj->ob_spec;
		ub_start = (OBBLK *) *(LONG *)rs_global;
		ub_end = ub_start + rs_global[4];
		if ((ub >= ub_start) && (ub < ub_end))
			if (ub->old_type == G_CICON)
				obj->ob_height = ((CICONBLK *)(ub->ublk.ub_parm))->monoblk.ib_hicon * xgl_hbox / 16;
	}
#endif

	return;
}

/*****************************************************************************/

static VOID rs_sglobal(WORD *base)
{
	rs_global = base;
	hdr_buf = (RSXHDR *) *(LONG *)&rs_global[7];
	rs_hdr = (RSXHDR *)((LONG)hdr_buf+sizeof(RSXHDR));

	return;
}

/*****************************************************************************/

static WORD rs_free(WORD *base)
{
#if COLOR_ICONS == TRUE
WORD i;
CICON *color_icn;
#endif
RSXHDR *rsxhdr;

	rs_global = base;
	rsxhdr = (RSXHDR *) *(LONG *)&rs_global[7];

#if COLOR_ICONS == TRUE

	if ((color_icn = (CICON *) *(LONG *)&rs_global[2]) != NULL) {
#if SAVE_MEMORY == TRUE
		VOID *rs_start, *rs_end;

		rs_start = (VOID *) ((LONG)rsxhdr + sizeof(RSXHDR));
		rs_end = (VOID *) (*(LONG *) (rsxhdr->rsh_rssize + (rsxhdr->rsh_rssize & 1L) + (LONG)rs_start) + (LONG)rs_start - 1L);

		for (i = 0; i < rs_global[4]; i++) {
			if (color_icn[i].num_planes > 0) {		/* avoid unused entries */
//was			if (color_icn[i].num_planes > 1) {
				if (color_icn[i].col_data != NULL && (color_icn[i].col_data < rs_start || color_icn[i].col_data > rs_end))
					free(color_icn[i].col_data);
				if (color_icn[i].sel_data != NULL && (color_icn[i].sel_data < rs_start || color_icn[i].sel_data > rs_end))
					free(color_icn[i].sel_data);
				if (color_icn[i].sel_data == NULL && color_icn[i].sel_mask != NULL && (color_icn[i].sel_mask < rs_start || color_icn[i].sel_mask > rs_end))
					free(color_icn[i].sel_mask);
			}
		}
#else
		for (i = 0; i < rs_global[4]; i++) {
			if (color_icn[i].num_planes > 0) {		/* avoid unused entries */
//was			if (color_icn[i].num_planes > 1) {
				if (color_icn[i].col_data != NULL)
					free(color_icn[i].col_data);
				if (color_icn[i].sel_data != NULL)
					free(color_icn[i].sel_data);
				else if (color_icn[i].sel_mask != NULL)
					free(color_icn[i].sel_mask);
			}
		}
#endif
		free(color_icn);
	}
	if (*(LONG *)rs_global)
		free((VOID *) *(LONG *)rs_global);
#endif

	free(rsxhdr);

	return TRUE;
}

/*****************************************************************************/

static WORD rs_gaddr(WORD *base,WORD re_gtype,WORD re_gindex,OBJECT **re_gaddr)
{
	rs_sglobal(base);

	*re_gaddr = get_address(re_gtype,re_gindex);

	if (*re_gaddr == (OBJECT *)NULL)
		return FALSE;

	return TRUE;
}

/*****************************************************************************/

static WORD rs_sadd(WORD *base,WORD rs_stype,WORD rs_sindex,OBJECT *re_saddr)
{
OBJECT *old_addr;

	rs_sglobal(base);

	old_addr = get_address(rs_stype,rs_sindex);

	if (old_addr == (OBJECT *)NULL)
		return FALSE;

	*old_addr = *re_saddr;

	return TRUE;
}

/*****************************************************************************/

static WORD rs_load(WORD *global,BYTE *fname)
{
	if (!rs_read(global,fname))
		return FALSE;

	rs_fixindex(global);

	return TRUE;
}

/*****************************************************************************/

static VOID *get_address(WORD type,WORD index)
{
VOID *the_addr = (VOID *)NULL;
union {
	VOID *dummy;
	BYTE *string;
	OBJECT **dpobject;
	OBJECT *object;
	TEDINFO *tedinfo;
	ICONBLK *iconblk;
	BITBLK *bitblk;
} all_ptr;

	switch(type) {
	case R_TREE:
		all_ptr.dpobject = (OBJECT **) (*(long **)&rs_global[5]);
		the_addr = all_ptr.dpobject[index];
		break;

	case R_OBJECT:
		the_addr = get_sub(index,hdr_buf->rsh_object,sizeof(OBJECT));
		break;

	case R_TEDINFO:
	case R_TEPTEXT:
		the_addr = get_sub(index,hdr_buf->rsh_tedinfo,sizeof(TEDINFO));
		break;

	case R_ICONBLK:
	case R_IBPMASK:
		the_addr = get_sub(index,hdr_buf->rsh_iconblk,sizeof(ICONBLK));
		break;

	case R_BITBLK:
	case R_BIPDATA:
		the_addr = get_sub(index,hdr_buf->rsh_bitblk,sizeof(BITBLK));
		break;

	case R_OBSPEC:
		all_ptr.object = get_address(R_OBJECT,index);
		the_addr = &all_ptr.object->ob_spec;
		break;

	case R_TEPVALID:
	case R_TEPTMPLT:
		all_ptr.tedinfo = get_address(R_TEDINFO,index);
		if (type == R_TEPVALID)
			the_addr = &all_ptr.tedinfo->te_pvalid;
		else
			the_addr = &all_ptr.tedinfo->te_ptmplt;
		break;

	case R_IBPDATA:
	case R_IBPTEXT:
		all_ptr.iconblk = get_address(R_ICONBLK,index);
		if (type == R_IBPDATA)
			the_addr = &all_ptr.iconblk->ib_pdata;
		else
			the_addr = &all_ptr.iconblk->ib_ptext;
		break;

	case R_STRING:
		the_addr = get_sub(index,hdr_buf->rsh_frstr,sizeof(BYTE *));
		break;

	case R_IMAGEDATA:
		the_addr = get_sub(index,hdr_buf->rsh_imdata,sizeof(BYTE *));
		the_addr = (VOID *) *(BYTE *)the_addr;
		break;

	case R_FRIMG:
		the_addr = get_sub(index,hdr_buf->rsh_frimg,sizeof(BYTE *));
		break;

	case R_FRSTR:
		the_addr = get_sub(index,hdr_buf->rsh_frstr,sizeof(BYTE *));
		break;
	}

	return the_addr;
}

/*****************************************************************************/

static VOID *get_sub(WORD index,LONG offset,WORD size)
{
UBYTE *ptr = (UBYTE *)rs_hdr;

	ptr += offset;
	ptr += ((LONG)index * (LONG)size);

	return (VOID *)ptr;
}

/*****************************************************************************/

static WORD rs_read(WORD *global,BYTE *fname)
{
WORD i, fh;
BYTE tmpnam[128];
DTA dta, *old_dta;
LONG size;
WORD ret = TRUE;

	strcpy(tmpnam,fname);

	if (!shel_find(tmpnam)) {
		form_error(-33);
		return FALSE;
	}
	rs_global = global;

	old_dta = Fgetdta();
	Fsetdta(&dta);
	if (Fsfirst(tmpnam,FA_SUBDIR) == 0)
		size = dta.d_length;
	else size = 0;
	Fsetdta(old_dta);

	if (size > sizeof(RSHDR) && (fh=Fopen(tmpnam,0)) > 0) {
		if ((hdr_buf = (RSXHDR *) malloc(size + sizeof(RSXHDR))) != NULL) {
			rs_hdr = (RSXHDR *)((LONG)hdr_buf+sizeof(RSXHDR));

			if (Fread(fh,size,rs_hdr) == size) {
				if (((RSHDR *)rs_hdr)->rsh_vrsn == 3)
					memcpy(hdr_buf,rs_hdr,sizeof(RSXHDR));
				else
					for (i = 0; i < sizeof(RSXHDR) / sizeof(LONG); i++)
						((ULONG *)hdr_buf)[i] = ((UWORD *)rs_hdr)[i];

				do_rsfix(hdr_buf->rsh_rssize);

				if (size > hdr_buf->rsh_rssize + 72L)	/* colour icons in resource? */
					do_ciconfix((ULONG)rs_hdr,hdr_buf,size);
			} else
				ret = FALSE;
		} else {
			form_error(-39);
			ret = FALSE;
		}

		Fclose(fh);
	} else
		ret = FALSE;

	return ret;
}

/*****************************************************************************/

static VOID rs_fixindex(WORD *global)
{
	rs_sglobal(global);

	fix_object();
}

/*****************************************************************************/

static VOID do_rsfix(ULONG size)
{
	rs_global[7] = ((LONG)hdr_buf >> 16) & 0xFFFF;
	rs_global[8] = (LONG)hdr_buf & 0xFFFF;
	rs_global[9] = (UWORD)size;

	fix_treeindex();
	fix_tedinfo();

	fix_nptr(hdr_buf->rsh_nib-1,R_IBPMASK);
	fix_nptr(hdr_buf->rsh_nib-1,R_IBPDATA);
	fix_nptr(hdr_buf->rsh_nib-1,R_IBPTEXT);

	fix_nptr(hdr_buf->rsh_nbb-1,R_BIPDATA);
	fix_nptr(hdr_buf->rsh_nstring-1,R_FRSTR);
	fix_nptr(hdr_buf->rsh_nimages-1,R_FRIMG);
}

/*****************************************************************************/

static VOID fix_treeindex(VOID)
{
OBJECT **adr;
LONG count;

	count = hdr_buf->rsh_ntree - 1L;

	adr = get_sub(0,hdr_buf->rsh_trindex,sizeof(OBJECT *));

	rs_global[5] = ((LONG)adr >> 16) & 0xFFFF;
	rs_global[6] = (LONG)adr & 0xFFFF;

	while (count >= 0) {
		fix_long((LONG *)(count * sizeof(OBJECT *) + (LONG)adr));
		count--;
	}
}

/*****************************************************************************/

static VOID fix_object(VOID)
{
WORD count;
OBJECT *obj;

	count = hdr_buf->rsh_nobs - 1;

	while (count >= 0) {
		obj = get_address(R_OBJECT,count);
		rs_obfix(obj,0);
		if ((obj->ob_type & 0xff) != G_BOX && (obj->ob_type & 0xff) != G_IBOX && (obj->ob_type & 0xff) != G_BOXCHAR)
			fix_long((LONG *)&obj->ob_spec);

		count--;
	}
}

/*****************************************************************************/

static VOID fix_tedinfo(VOID)
{
LONG count;
TEDINFO *tedinfo;

	count = hdr_buf->rsh_nted - 1;

	while (count >= 0) {
		tedinfo = get_address(R_TEDINFO,count);

		if (fix_ptr(R_TEPTEXT,count))
			tedinfo->te_txtlen = strlen(tedinfo->te_ptext) + 1;

		if (fix_ptr(R_TEPTMPLT,count))
			tedinfo->te_tmplen = strlen(tedinfo->te_ptmplt) + 1;

		fix_ptr(R_TEPVALID,count);

		count--;
	}

	return;
}

/*****************************************************************************/

static VOID fix_nptr(LONG index,WORD ob_type)
{
	while (index >= 0)
		fix_long(get_address(ob_type,index--));
}

/*****************************************************************************/

static WORD fix_ptr(WORD type,LONG index)
{
	return fix_long(get_address(type,index));
}

/*****************************************************************************/

static WORD fix_long(LONG *lptr)
{
LONG base;

	base = *lptr;
	if (base == 0L)
		return FALSE;

	base += (LONG)rs_hdr;

	*lptr = base;

	return TRUE;
}

/*****************************************************************************/

static VOID fix_chp(WORD *pcoord,WORD flag)
{
WORD ncoord;

	ncoord = *pcoord & 0xff;

	if (!flag && ncoord == 0x50)
		ncoord = xdesk.g_w; 				/* xdesk.w = width of screen in pixels */
	else ncoord *= flag?xgl_hbox:xgl_wbox;	/* xgl_wbox, xgl_hbox = character width, height in pixels */

	if (((*pcoord >> 8) & 0xff) > 0x80)
		ncoord += (((*pcoord >> 8) & 0xff) | 0xff00);
	else ncoord += ((*pcoord >> 8) & 0xff);

	*pcoord = ncoord;
}

/*****************************************************************************/
/* Initialise colour icons for current resolution							 */
/*****************************************************************************/
static VOID do_ciconfix(ULONG header,RSXHDR *rsxhdr,LONG rs_len)
{
LONG *cicon_liste;
WORD i;
OBJECT *obj;

	cicon_liste = (LONG *) (*(LONG *)(rsxhdr->rsh_rssize + (rsxhdr->rsh_rssize & 1L) + header + sizeof(LONG)) + header);
	if ((LONG)cicon_liste - header > rsxhdr->rsh_rssize && (LONG)cicon_liste - header < rs_len) {
		if (fill_cicon_liste(cicon_liste,header,rsxhdr) != NIL) {
#if COLOR_ICONS == TRUE
			WORD nub = 0, work_out[57], *palette;

			if (*(LONG *)rs_global && *(LONG *)&rs_global[2]) {
				vq_extnd(xvdi_handle,1,work_out); 		/* determine number of planes */
				xscrn_planes = work_out[4];

				xpixelbytes = test_rez();				/* bytes/pixel in device-dependent format, used by xfill_farbtbl() */

				palette = (WORD *) *(LONG *)(rsxhdr->rsh_rssize + (rsxhdr->rsh_rssize & 1L) + header + 2 * sizeof(LONG));
				if (palette != NULL) {
					palette = (WORD *)((LONG)palette+header);
					memcpy(rgb_palette,palette,sizeof(rgb_palette));
					is_palette = TRUE;
				} else is_palette = FALSE;

				xfill_farbtbl();						/* populates farbtbl[], farbtbl2[] */

				for (i = 0; i < rsxhdr->rsh_nobs; i++) {
					obj = &((OBJECT *)(rsxhdr->rsh_object + header))[i];
					if ((obj->ob_type & 0xff) == G_CICON) {
						if (xadd_cicon((CICONBLK *)obj->ob_spec,obj,nub++) == FALSE) {
							memset(&((CICON *) *(LONG *)&rs_global[2])[nub-1],0,sizeof(CICON));	/* mark cicon_table entry as unused */
							obj->ob_type = (obj->ob_type & 0xff00) | G_ICON;
						}
						obj->ob_spec = (void *)((LONG)obj->ob_spec-header);
					}
				}
			} else
#endif
			{
				for (i = 0; i < rsxhdr->rsh_nobs; i++) {
					obj = &((OBJECT *)(rsxhdr->rsh_object + header))[i];
					if ((obj->ob_type & 0xff) == G_CICON) {
						obj->ob_type = (obj->ob_type & 0xff00) | G_ICON;
						obj->ob_spec = (void *)((LONG)obj->ob_spec-header);
					}
				}
			}
		}
	}
}

/*****************************************************************************/
/* initialise colour icon pointers in resource			 					 */
/*****************************************************************************/
static WORD fill_cicon_liste(LONG *cicon_liste,ULONG header,RSXHDR *rsxhdr)
{
WORD i, i2, num = 0;
BYTE *p;
LONG iclen, num_cicon, ob, p2;
CICONBLK *cblk;
CICON *cicon, *cold;
OBJECT *pobject;

	while (!cicon_liste[num])
		num++;

	if (cicon_liste[num] != -1L)
		return NIL;

	cblk = (CICONBLK *)&cicon_liste[num+1];

	for (i = 0; i < num; i++) {
		cicon_liste[i] = (LONG)cblk;
		p = (BYTE *)&cblk[1];
		cblk->monoblk.ib_pdata = (WORD *)p;
		iclen = cblk->monoblk.ib_wicon / 8 * cblk->monoblk.ib_hicon;
		p += iclen;
		cblk->monoblk.ib_pmask = (WORD *)p;
		p += iclen;
		p2 = (LONG)cblk->monoblk.ib_ptext;
		if (!p2 || header + p2 == (LONG)p || p2 < rsxhdr->rsh_string || p2 > rsxhdr->rsh_rssize)
			cblk->monoblk.ib_ptext = (BYTE *)p;
		else
			cblk->monoblk.ib_ptext = (BYTE *)(header+(LONG)cblk->monoblk.ib_ptext);

		cicon = (CICON *)&p[12];
		p += 12L;
		cold = cicon;
		if ((num_cicon = (LONG)cblk->mainlist) > 0) {
			cblk->mainlist = cicon;

			for (i2 = 0; i2 < num_cicon; i2++) {
				p = (BYTE *)&cicon[1];
				cicon->col_data = (WORD *)p;
				p += iclen * cicon->num_planes;
				cicon->col_mask = (WORD *)p;
				p += iclen;
				if (cicon->sel_data != NULL) {
					cicon->sel_data = (WORD *)p;
					p += iclen * cicon->num_planes;
					cicon->sel_mask = (WORD *)p;
					p += iclen;
				}
				cicon->next_res = (CICON *)p;
				cold = cicon;
				cicon = (CICON *)p;
			}
			cold->next_res = NULL;
		}
		cblk = (CICONBLK *)p;
	}

	if (num != NIL) {
		pobject = (OBJECT *)(header + rsxhdr->rsh_object);

		for (ob = 0; ob < rsxhdr->rsh_nobs; ob++)
			if ((pobject[ob].ob_type & 0xff) == G_CICON)
				pobject[ob].ob_spec = (void *)cicon_liste[(LONG)pobject[ob].ob_spec];

		if ((*(LONG *)rs_global = (LONG)malloc(num * sizeof(OBBLK))) != 0L)
			memset((VOID *) *(LONG *)rs_global,0,num * sizeof(OBBLK));
		if ((*(LONG *)&rs_global[2] = (LONG)malloc(num * sizeof(CICON))) != 0L)
			memset((VOID *) *(LONG *)&rs_global[2],0,num * sizeof(CICON));
		rs_global[4] = num;
	}
	return num;
}

#if COLOR_ICONS == TRUE
/****************************************************************************/
/* change icon to device-dependent format and convert to different			*/
/* resolution if required 													*/
/*	returns	TRUE if OK														*/
/*			FALSE if error (malloc failed)									*/
/****************************************************************************/
static WORD xadd_cicon(CICONBLK *cicnblk,OBJECT *obj,WORD nub)
{
WORD x, y, line, xmax, best_planes, find_planes;
CICON *cicn, *color_icn, *best_icn = NULL;
LONG len, *next;
MFDB d;
OBBLK *ub;

#if SAVE_MEMORY == TRUE
CICON *max_icn = NULL;

#endif

	len = cicnblk->monoblk.ib_wicon / 8 * cicnblk->monoblk.ib_hicon;

	color_icn = &((CICON *) *(LONG *)&rs_global[2])[nub];

	best_planes = 1;
	if (xscrn_planes > 8)
		find_planes = 4;
	else if ((xgl_hbox == 8) && (xscrn_planes > 2))		/* not in original distribution */
		find_planes = 2;
	else if ((xgl_hbox == 16) && (xscrn_planes == 2))	/* not in original distribution */
		find_planes = 1;								/* a mono icon looks better than a smooshed 4-colour one */
	else find_planes = xscrn_planes;

	cicn = cicnblk->mainlist;
	next = (LONG *) & cicnblk->mainlist;

	while (cicn != NULL) {
		*next = (LONG)cicn;
		next = (LONG *)&cicn->next_res;

#if SAVE_MEMORY == TRUE
		if (cicn->num_planes > xscrn_planes)
			max_icn = cicn;
#endif
		if (cicn->num_planes >= best_planes && cicn->num_planes <= find_planes) {
			best_planes = cicn->num_planes;
			best_icn = cicn;
		}
		cicn = cicn->next_res;
	}

	if (best_icn == NULL)		/* no suitable colour icon found */
		return FALSE;

	*color_icn = *best_icn;		/* copy best-match icon */

	if (best_planes > 1)
		color_icn->num_planes = xscrn_planes;
	else color_icn->num_planes = 1;

	/* allocate space for device-dependent format */
	if ((color_icn->col_data = malloc(len * color_icn->num_planes)) == NULL) {
		form_error(-39);
		return FALSE;
	}
	if (color_icn->sel_data) {
		if ((color_icn->sel_data = malloc(len * color_icn->num_planes)) == NULL) {
			free(color_icn->col_data);
			form_error(-39);
			return FALSE;
		}
	}
	if (best_planes > 1) {
		if (best_icn->sel_data == NULL) {
			/* prepare Selected mask */
			if ((color_icn->sel_mask = malloc(len)) == NULL) {
				free(color_icn->col_data);
				if (color_icn->sel_data)
					free(color_icn->sel_data);
				form_error(-39);
				return FALSE;
			}
			xmax = cicnblk->monoblk.ib_wicon / 16;

			for (y = 0; y < cicnblk->monoblk.ib_hicon; y++) {
				line = y * xmax;
				for (x = 0; x < xmax; x++)
					color_icn->sel_mask[line + x] = best_icn->col_mask[line + x] & ((y&1)?0xaaaa:0x5555);
			}
		}
		d.fd_addr = color_icn->col_data;
		d.fd_w = cicnblk->monoblk.ib_wicon;
		d.fd_h = cicnblk->monoblk.ib_hicon;
		d.fd_wdwidth = d.fd_w >> 4;
		d.fd_stand = TRUE;
		d.fd_nplanes = xscrn_planes;

		xfix_cicon((UWORD *)best_icn->col_data,len,best_planes,xscrn_planes,&d);
		if (best_icn->sel_data) {
			d.fd_addr = color_icn->sel_data;
			xfix_cicon((UWORD *)best_icn->sel_data,len,best_planes,xscrn_planes,&d);
		}
	} else {
		memcpy(color_icn->col_data,best_icn->col_data,len);
		memcpy(color_icn->sel_data,best_icn->sel_data,len);
	}

#if SAVE_MEMORY == TRUE
	if (best_icn->num_planes < color_icn->num_planes && max_icn != NULL) {
		if (best_icn->sel_data == NULL || max_icn->sel_data != NULL) {
			best_icn->col_data = max_icn->col_data;
			best_icn->sel_data = max_icn->sel_data;
			best_icn->num_planes = xscrn_planes;
		}
	}
	if (best_icn->num_planes == color_icn->num_planes) {
		memcpy(best_icn->col_data,color_icn->col_data,len * color_icn->num_planes);
		free(color_icn->col_data);
		color_icn->col_data = NULL;
		if (best_icn->sel_data != NULL) {
			memcpy(best_icn->sel_data,color_icn->sel_data,len * color_icn->num_planes);
			free(color_icn->sel_data);
			color_icn->sel_data = NULL;
		} else {
			memcpy(cicnblk->monoblk.ib_pmask,color_icn->sel_mask,len);
			best_icn->sel_mask = cicnblk->monoblk.ib_pmask;
			free(color_icn->sel_mask);
			color_icn->sel_mask = NULL;
		}
	} else {
		if (best_icn->sel_data == NULL) {
			memcpy(cicnblk->monoblk.ib_pmask,color_icn->sel_mask,len);
			free(color_icn->sel_mask);
			color_icn->sel_mask = NULL;
			best_icn->col_data = color_icn->col_data;
			best_icn->sel_mask = cicnblk->monoblk.ib_pmask;
			best_icn->num_planes = color_icn->num_planes;
		} else *best_icn = *color_icn;
	}
	cicnblk->mainlist = best_icn;
#else
	color_icn->next_res = cicnblk->mainlist;
	cicnblk->mainlist = color_icn;
#endif
	ub = (OBBLK *) *(LONG *)rs_global;
	ub[nub].old_type = G_CICON;
	ub[nub].ublk.ub_parm = (long)obj->ob_spec;
	ub[nub].ublk.ub_code = (int (*)(PARMBLK *))&xdraw_cicon;		/* need cast because of __saveds */
	obj->ob_spec = (void *) &ub[nub].ublk;
	obj->ob_type = (obj->ob_type & 0xff00) | G_USERDEF;

	return TRUE;
}

/************************************************************************/
/*	Return number of bytes per pixel used in device-dependent format	*/
/*	returns 0 if palette-based (I think)								*/
/************************************************************************/
static WORD test_rez(VOID)
{
WORD i, np, pxy[8], rgb[3], bpp = 0;
UWORD backup[32], test[32];
WORD black[3] = {0, 0, 0};
WORD white[3] = {1000, 1000, 1000};
MFDB screen;
MFDB pixel = {NULL, 16, 1, 1, 0, 1, 0, 0, 0};	/* device-dependent format */
MFDB stdfm = {NULL, 16, 1, 1, 1, 1, 0, 0, 0};	/* device-independent format */

	if (xscrn_planes < 8)
		return 0;

	stdfm.fd_nplanes = pixel.fd_nplanes = xscrn_planes;

	/*
	 *	handle xscrn_planes = 8
	 */
	if (xscrn_planes == 8) {
		for (np = 0; np < xscrn_planes; np++)
			test[np] = 0x8000;
		pixel.fd_addr = stdfm.fd_addr = test;
		vr_trnfm(xvdi_handle,&stdfm,&pixel);	/* convert device-independent to device-dependent in test[] */

		if (test[0] & 0x00ff)					/* some bits strayed from the first byte ... */
			return 0;
		for (i = 1; i < xscrn_planes; i++)
			if (test[i])						/* likewise */
				return 0;

		return 1;
	}

	/*
	 *	handle xscrn_planes > 8
	 */
	vs_clip(xvdi_handle,FALSE,NULL);
	screen.fd_addr = NULL;

	memset(backup,0,sizeof(backup));

	vswr_mode(xvdi_handle,MD_REPLACE);
	vsl_ends(xvdi_handle,0,0);
	vsl_type(xvdi_handle,1);
	vsl_width(xvdi_handle,1);
	memset(pxy,0,sizeof(pxy));

	graf_mouse(M_OFF,NULL);

	/*
	 *	first, save the current value of the test pixel (in 'backup')
	 *	and the current RGB value of pen 15 (in 'rgb[]')
	 */
	pixel.fd_addr = backup; 	/* save area is 32 words, enough for 512 planes ... */
	vro_cpyfm(xvdi_handle,S_ONLY,pxy,&screen,&pixel);
	vq_color(xvdi_handle,15,1,rgb);

	/*
	 *	next, test device-dependent format for 'packed' format,
	 *	i.e. no unused bits:
	 *		. draw one pixel at (0,0) in white
	 *		. copy it via vro_cpyfm() to test[]
	 *		. test that high-order entries of test[] are all zeroes
	 *		. if so, repeat the above with a black pixel
	 *		. if all OK, set bytes/pixel = (planes+7)/8
	 */
	pixel.fd_addr = test;
	vsl_color(xvdi_handle,15);			/* draw lines in pen 15 */
	vs_color(xvdi_handle,15,white);		/* set pen 15 to white */
	v_pline(xvdi_handle,2,pxy);			/* draw 1 pixel at (0,0,0,0) */

	memset(test,0,xscrn_planes*sizeof(WORD));
	vro_cpyfm(xvdi_handle,S_ONLY,pxy,&screen,&pixel);	/* copy pixel to test[] array */

	for (i = (xscrn_planes + 15) / 16 * 2; i < xscrn_planes; i++)	//rfb: why times 2? i think this is a bug
		if (test[i])
			break;

	if (i >= xscrn_planes) {
		vs_color(xvdi_handle,15,black);	/* set pen 15 to black */
		v_pline(xvdi_handle,2,pxy);		/* draw 1 pixel */

		memset(test,0,xscrn_planes*sizeof(WORD));
		vro_cpyfm(xvdi_handle,S_ONLY,pxy,&screen,&pixel);	/* copy to test[] */

		for (i = (xscrn_planes+15)/16 * 2; i < xscrn_planes; i++)	//rfb: see comment above
			if (test[i])
				break;

		if (i >= xscrn_planes)
			bpp = (xscrn_planes + 7) / 8;
	}

	/*
	 *	last, restore the RGB value of pen 15 and the original pixel colour,
	 *	and return bytes/pixel
	 */
	vs_color(xvdi_handle,15,rgb);
	pixel.fd_addr = backup;
	vro_cpyfm(xvdi_handle,S_ONLY,pxy,&pixel,&screen);

	graf_mouse(M_ON,NULL);

	return bpp;
}

/************************************************************************/
/* determine TrueColor pixel values corresponding to RGB palette		*/
/************************************************************************/
static VOID xfill_farbtbl(VOID)
{
WORD np, color, pxy[8], backup[32], rgb[3];
MFDB screen;
MFDB pixel = {NULL, 16, 1, 1, 0, 1, 0, 0, 0};
MFDB stdfm = {NULL, 16, 1, 1, 1, 1, 0, 0, 0};
WORD pixtbl[16] = {0, 2, 3, 6, 4, 7, 5, 8, 9, 10, 11, 14, 12, 15, 13, 16};

	if (xscrn_planes < 8)					/* < 256 colours */
		return;

	if (xscrn_planes == 8) {				/* 256 colours */
		if (xpixelbytes)					/* i.e. TrueColor */
			for (color = 0; color < 256; color++)
				*(UBYTE *) &farbtbl2[color] = color;
		return;
	}

	/*
	 *	handle > 256 colours
	 */
	if (is_palette == FALSE) {		/* if no palette in the resource, generate one */
		/*
		 *	obtain the RGB values corresponding to palette entries 0-255
		 *	and store them in the my_rgb_palette[][] table.
		 *	my_rgb_palette[][0,1,2] contain the RGB values; my_rgb_palette[3]
		 *	contains the corresponding VDI pen number.
		 */
		for (color = 0; color < 16; color++) {
			vq_color(xvdi_handle,pixtbl[color],1,rgb_palette[color]);
			rgb_palette[color][3] = pixtbl[color];
		}
		for ( ; color < 255; color++) {
			vq_color(xvdi_handle,color+1,1,rgb_palette[color]);
			rgb_palette[color][3] = color + 1;
		}
		vq_color(xvdi_handle,1,1,rgb_palette[255]);
		rgb_palette[255][3] = 1;
		is_palette = TRUE;
	}

	vs_clip(xvdi_handle,FALSE,NULL);
	graf_mouse(M_OFF,NULL);

	memset(backup,0,sizeof(backup));
	memset(farbtbl,0,32 * 256 * sizeof(WORD));
	screen.fd_addr = NULL;
	stdfm.fd_nplanes = pixel.fd_nplanes = xscrn_planes;

	vswr_mode(xvdi_handle,MD_REPLACE);
	vsl_ends(xvdi_handle,0,0);
	vsl_type(xvdi_handle,1);
	vsl_width(xvdi_handle,1);
	memset(pxy,0,sizeof(pxy));	/* set src and dest rectangle = (0,0,0,0) */

	/*
	 *	first, save the current value of the test pixel in 'backup'	(in
	 *	device-dependent format), and the current RGB value of pen 15 in 'rgb[]'
	 */
	pixel.fd_addr = backup; 	/* save area is 32 words, enough for 512 planes ... */
	vro_cpyfm(xvdi_handle,S_ONLY,pxy,&screen,&pixel);
	vq_color(xvdi_handle,15,1,rgb);

	/*
	 *	next, for each entry in rgb_palette[]:
	 *		. set pen 15 to the corresponding RGB value
	 *		. set the polyline color index to pen 15
	 *		. draw a line from (0,0,0,0) to (0,0,0,0), i.e. a point
	 *		. get the (device-dependent) data from that point into the farbtbl[] entry for that colour
	 *		. for TrueColor graphics, copy that data to farbtbl2[]
	 *		. convert the farbtbl[] entry to device-independent format (in place)
	 */
	for (color = 0; color < 256; color++) {
		vs_color(xvdi_handle,15,rgb_palette[color]);
		vsl_color(xvdi_handle,15);
		v_pline(xvdi_handle,2,pxy);

		stdfm.fd_addr = pixel.fd_addr = farbtbl[color];

		vro_cpyfm(xvdi_handle,S_ONLY,pxy,&screen,&pixel);	/* vro_cpyfm, since v_get_pixel doesn't work with TrueColor (>=24 planes) */

		if (xpixelbytes) {								/* i.e. not palette-based */
			farbtbl2[color] = 0L;
			memcpy(&farbtbl2[color],pixel.fd_addr,xpixelbytes);
		}
		vr_trnfm(xvdi_handle,&pixel,&stdfm);
		for (np = 0; np < xscrn_planes; np++)
			if (farbtbl[color][np])
				farbtbl[color][np] = 0xffff;
	}

	/*
	 *	last, restore the RGB value of pen 15 and the original pixel colour
	 */
	vs_color(xvdi_handle,15,rgb);

	pixel.fd_addr = backup;
	vro_cpyfm(xvdi_handle,S_ONLY,pxy,&pixel,&screen);

	graf_mouse(M_ON,NULL);
}

/****************************************************/
/*	Convert icon to current graphics resolution		*/
/*	(e.g. 4 plane icon to 24 plane TrueColor)		*/
/****************************************************/
static VOID xfix_cicon(UWORD *col_data,LONG len,WORD old_planes,WORD new_planes,MFDB *s)
{
LONG x, i, old_len, rest_len, mul[32], pos;
UWORD np, *new_data, mask, pixel, bit, color, back[32], old_col[32], maxcol;
WORD got_mem = FALSE;
MFDB d;

	len >>= 1;		/* convert length in bytes to length in WORDs */

	/*
	 *	icon resolution & current resolution match ...
	 */
	if (old_planes == new_planes) {
		if (s != NULL) {
			if (new_planes == xscrn_planes) {
				d = *s;
				d.fd_stand = FALSE;
				s->fd_addr = col_data;
				if (d.fd_addr == s->fd_addr) {
					if ((d.fd_addr = malloc(len * sizeof(WORD) * new_planes)) == NULL)
						d.fd_addr = s->fd_addr;
					else got_mem = TRUE;
				}
				vr_trnfm(xvdi_handle,s,&d);
				if (d.fd_addr != s->fd_addr && got_mem == TRUE) {
					memcpy(s->fd_addr,d.fd_addr,len * sizeof(WORD) * new_planes);
					free(d.fd_addr);
				}
			} else memcpy(s->fd_addr,col_data,len * sizeof(WORD) * new_planes);
		}
		return;
	}

	old_len = old_planes * len;
	rest_len = new_planes * len - old_len;

	/*
	 *	handle mismatched resolutions with palette-based display
	 */
	if (new_planes <= 8) {
		if (s != NULL) {
			new_data = &((UWORD *)s->fd_addr)[old_len];
			memset(new_data,0,rest_len * sizeof(WORD));
			memcpy(s->fd_addr,col_data,old_len * sizeof(WORD));
			col_data = s->fd_addr;
		} else new_data = (UWORD *)&col_data[old_len];

		for (x = 0; x < len; x++) {
			mask = 0xffff;

			for (i = 0; i < old_len; i += len)
				mask &= (UWORD)col_data[x+i];

			if (mask)
				for (i = 0; i < rest_len; i += len)
					new_data[x+i] |= mask;
		}

		if (s != NULL) {		/* convert to device-dependent format */
			d = *s;
			d.fd_stand = 0;
			if ((d.fd_addr = malloc(len * sizeof(WORD) * new_planes)) == NULL)
				d.fd_addr = s->fd_addr;

			vr_trnfm(xvdi_handle,s,&d);
			if (d.fd_addr != s->fd_addr) {
				memcpy(s->fd_addr,d.fd_addr,len * sizeof(WORD) * new_planes);
				free(d.fd_addr);
			}
		}
		return;
	}

	/*
	 *	handle mismatched resolutions with TrueColor display, i.e. RGB-oriented pixel values
	 */
	if (!xpixelbytes || s == NULL) {
		for (i = 0; i < new_planes; i++)
			mul[i] = i * len;

		if (old_planes < 8) {
			maxcol = (1 << old_planes) - 1;
			memcpy(old_col,farbtbl[maxcol],new_planes * sizeof(WORD));
			memset(farbtbl[maxcol],0,new_planes * sizeof(WORD));
		}
		if (s != NULL) {
			new_data = &((UWORD *)s->fd_addr)[old_len];
			memset(new_data,0,rest_len * sizeof(WORD));
			memcpy(s->fd_addr,col_data,old_len * sizeof(WORD));
			col_data = s->fd_addr;
		}
		for (x = 0; x < len; x++) {
			bit = 1;
			for (np = 0; np < old_planes; np++)
				back[np] = col_data[mul[np]+x];

			for (pixel = 0; pixel < 16; pixel++) {
				color = 0;
				for (np = 0; np < old_planes; np++) {
					color += ((back[np] & 1) << np);
					back[np] >>= 1;
				}

				for (np = 0; np < new_planes; np++) {
					pos = mul[np] + x;
					col_data[pos] = (col_data[pos] & ~bit) | (farbtbl[color][np] & bit);
				}

				bit <<= 1;
			}
		}
		if (old_planes < 8)
			memcpy(farbtbl[maxcol],old_col,new_planes * sizeof(WORD));

		if (s != NULL) {		/* convert to device-dependent format */
			d = *s;
			d.fd_stand = 0;
			if ((d.fd_addr = malloc(len * sizeof(WORD) * new_planes)) == NULL)
				d.fd_addr = s->fd_addr;

			vr_trnfm(xvdi_handle,s,&d);
			if (d.fd_addr != s->fd_addr) {
				memcpy(s->fd_addr,d.fd_addr,len * sizeof(WORD) * new_planes);
				free(d.fd_addr);
			}
		}
	} else
		std_to_byte(col_data,len,old_planes,farbtbl2,s);
}

/****************************************************************************/
/*	std_to_byte converts a graphic in device-independent format directly 	*/
/*	into device-dependent format (for resolutions with > 8 planes)			*/
/****************************************************************************/
static VOID std_to_byte(UWORD *col_data,LONG len,WORD old_planes,ULONG *farbtbl2,MFDB *s)
{
LONG x, i, mul[32], pos;
UWORD np, *new_data, pixel, color, back[32];
WORD memflag = FALSE;
UBYTE *p1, *p2;
ULONG colback;

	if (s->fd_addr == col_data) {
		if ((col_data = malloc(len * sizeof(WORD) * s->fd_nplanes)) == NULL) {
			form_error(-39);
			return;
		}
		memcpy(col_data,s->fd_addr,len * sizeof(WORD) * s->fd_nplanes);
		memflag = TRUE;
	}
	new_data = (UWORD *)s->fd_addr;
	p1 = (UBYTE *)new_data;

	if (old_planes < 8) {
		colback = farbtbl2[(1<<old_planes)-1];
		farbtbl2[(1<<old_planes)-1] = farbtbl2[255];
	}
	for (i = 0; i < old_planes; i++)
		mul[i] = i * len;

	pos = 0;

	for (x = 0; x < len; x++) {
		for (np = 0; np < old_planes; np++)
			back[np] = col_data[mul[np] + x];

		for (pixel = 0; pixel < 16; pixel++) {
			color = 0;
			for (np = 0; np < old_planes; np++) {
				color |= ((back[np] & 0x8000) >> (15 - np));
				back[np] <<= 1;
			}

			switch(xpixelbytes) {
			case 2:
				new_data[pos++] = *(UWORD *) &farbtbl2[color];
				break;
			case 3:
				p2 = (UBYTE *) &farbtbl2[color];
				*(p1++) = *(p2++);
				*(p1++) = *(p2++);
				*(p1++) = *(p2++);
				break;
			case 4:
				((ULONG *)new_data)[pos++] = farbtbl2[color];
				break;
			}
		}
	}

	if (old_planes < 8)
		farbtbl2[(1<<old_planes)-1] = colback;

	if (memflag)
		free(col_data);
}

/****************************/
/*	Display colour icon		*/
/*	NOTE: called from AES	*/
/****************************/
static int __saveds xdraw_cicon(PARMBLK *pb)
{
WORD ob_x, ob_y, x, y, dummy, pxy[4], m_mode, i_mode, mskcol, icncol;
LONG ob_spec;
ICONBLK *iconblk;
CICON *cicn;
WORD *mask, *data, *dark = NULL;
BYTE letter[2];
WORD selected, mindex[2], iindex[2], buf, xy[4], attr[5];
BOOLEAN invert = FALSE;

	vqf_attributes(xvdi_handle,attr);				/* remember old attributes */
	selected = pb->pb_currstate & SELECTED;

	xrect2array((GRECT *)&pb->pb_xc,xy);
	vs_clip(xvdi_handle,TRUE,xy);					/* set clipping rectangle (duh) */

	ob_spec = pb->pb_parm;
	ob_x = pb->pb_x;
	ob_y = pb->pb_y;

	iconblk = (ICONBLK *)ob_spec;
	cicn = ((CICONBLK *)ob_spec)->mainlist;
	m_mode = MD_TRANS;

	if (selected) { 			/* it was an objc_change */
		if (cicn->sel_data != NULL) {
			mask = cicn->sel_mask;
			data = cicn->sel_data;
			if (cicn->num_planes > 1) {
				if (cicn->num_planes > 8)		/* TrueColor, i.e. RGB-oriented graphics card? */
					i_mode = S_AND_D;
				else i_mode = S_OR_D;
			} else i_mode = MD_TRANS;
		} else {
			mask = cicn->col_mask;
			data = cicn->col_data;
			if (cicn->num_planes > 1) {
				if (cicn->num_planes > 8)
					i_mode = S_AND_D;
				else i_mode = S_OR_D;
				dark = cicn->sel_mask;
			} else invert = TRUE;
		}
	} else {
		mask = cicn->col_mask;
		data = cicn->col_data;
		if (cicn->num_planes > 1) {
			if (cicn->num_planes > 8)
				i_mode = S_AND_D;
			else i_mode = S_OR_D;
		} else i_mode = MD_TRANS;
	}

	mindex[0] = ((iconblk->ib_char & 0x0f00) != 0x0100) ? (iconblk->ib_char & 0x0f00) >> 8 : WHITE;
	mindex[1] = WHITE;

	icncol = iindex[0] = (WORD) (((UWORD) iconblk->ib_char & 0xf000U) >> 12U);
	iindex[1] = WHITE;

	mskcol = (iconblk->ib_char & 0x0f00) >> 8;

	x = ob_x + iconblk->ib_xicon;
	y = ob_y + iconblk->ib_yicon;

	if (invert) {
		buf = iindex[0];
		iindex[0] = mindex[0];
		mindex[0] = buf;
		i_mode = MD_TRANS;
	}
	if (selected) {
		buf = icncol;
		icncol = mskcol;
		mskcol = buf;
	}
	draw_bitblk(mask,x,y,iconblk->ib_wicon,iconblk->ib_hicon,1,m_mode,mindex);
	draw_bitblk(data,x,y,iconblk->ib_wicon,iconblk->ib_hicon,cicn->num_planes,i_mode,iindex);

	if (dark) {
		mindex[0] = BLACK;
		mindex[1] = WHITE;
		draw_bitblk(dark,x,y,iconblk->ib_wicon,iconblk->ib_hicon,1,MD_TRANS,mindex);
	}
	if (iconblk->ib_ptext[0]) {
		x = ob_x + iconblk->ib_xtext;
		y = ob_y + iconblk->ib_ytext;

		pxy[0] = x;
		pxy[1] = y;
		pxy[2] = x + iconblk->ib_wtext - 1;
		pxy[3] = y + iconblk->ib_htext - 1;

		vswr_mode(xvdi_handle,MD_REPLACE);		/* draw textbox */
		vsf_color(xvdi_handle,mskcol);
		vsf_interior(xvdi_handle,FIS_SOLID);
		vsf_perimeter(xvdi_handle,FALSE);
		v_bar(xvdi_handle,pxy);
	}
	vswr_mode(xvdi_handle,MD_TRANS);
	vst_font(xvdi_handle,1);					/* Systemfont */
	vst_height(xvdi_handle,4,&dummy,&dummy,&dummy,&dummy);
	vst_color(xvdi_handle,icncol);
	vst_effects(xvdi_handle,TXT_NORMAL);
	vst_alignment(xvdi_handle,ALI_LEFT,ALI_TOP,&dummy,&dummy);
	vst_rotation(xvdi_handle,0);

	if (iconblk->ib_ptext[0]) {
		x += (iconblk->ib_wtext - strlen(iconblk->ib_ptext) * 6) / 2;
		y += (iconblk->ib_htext - 6) / 2;

		v_gtext(xvdi_handle,x,y,iconblk->ib_ptext);
	}
	letter[0] = iconblk->ib_char & 0xff;
	if (letter[0]) {
		letter[1] = '\0';
		x = ob_x + iconblk->ib_xicon + iconblk->ib_xchar;
		y = ob_y + iconblk->ib_yicon + iconblk->ib_ychar;

		v_gtext(xvdi_handle,x,y,letter);
	}

	vs_clip(xvdi_handle,FALSE,NULL);

	vswr_mode(xvdi_handle,attr[3]);				/* restore old attributes */
	if (iconblk->ib_ptext[0]) {					/* (only if they were changed) */
		vsf_interior(xvdi_handle,attr[0]);
		vsf_color(xvdi_handle,attr[1]);
		vsf_style(xvdi_handle,attr[2]);
		vsf_perimeter(xvdi_handle,attr[4]);
	}

	return (pb->pb_currstate & ~SELECTED);
}

/*****************************************************************************/

static VOID draw_bitblk(WORD *p,WORD x,WORD y,WORD w,WORD h,WORD num_planes,WORD mode,WORD *index)
{
WORD pxy[8];
MFDB s, d;

	d.fd_addr = NULL;				/* screen */
	s.fd_addr = (VOID *) p;
	s.fd_w = w;
	s.fd_h = h;
	s.fd_wdwidth = w >> 4;
	s.fd_stand = FALSE;
	s.fd_nplanes = num_planes;

	pxy[0] = 0;
	pxy[1] = 0;
	pxy[2] = s.fd_w - 1;
	pxy[3] = s.fd_h - 1;

	pxy[4] = x;
	pxy[5] = y;
	pxy[6] = pxy[4] + pxy[2];
	pxy[7] = pxy[5] + pxy[3];

	if (num_planes > 1)
		vro_cpyfm(xvdi_handle,mode,pxy,&s,&d);
	else
		vrt_cpyfm(xvdi_handle,mode,pxy,&s,&d,index);		/* copy it */
}

/*****************************************************************************/

static VOID xrect2array(GRECT *rect,WORD *array)
{
	*array++ = rect->g_x;
	*array++ = rect->g_y;
	*array++ = rect->g_x + rect->g_w - 1;
	*array = rect->g_y + rect->g_h - 1;
}
#endif
