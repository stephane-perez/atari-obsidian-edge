/*
 *	rsc.c: resource handling
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

/*
 *	version 2.11 (november/2002)	roger burrows
 *		merged in the remnant of initrsc.c, after splitting out the
 *		original xrsrcfix.c into a separate file.  the added functions
 *		are:
 *			fix_rsrc(), term_rsrc(), geticon(), fix_objs(), noshadow(),
 *			trans_gimage(), vdi_fix(), vdi_trans()
 *		changes from the original v2/04 source include:
 *			fix_rsrc():
 *				we now dynamically set up the drive icons in the PCSEL
 *				tree, to reduce the size of the	generated C code from
 *				the RSC file
 *			load_rsrc():
 *				added to call init_xrsrc()/xrsrc_load()/xrsrc_gaddr()
 *			cleanup_rsrc():
 *				added so that we can call term_xrsrc() and free up
 *				memory allocated by init_xrsrc()
 *			geticon():
 *				we use an alternate method to obtain this info; also
 *				it's now a PRIVATE function
 *
 *	version 2.12 (december/2002)	roger burrows
 *		added xform_alert()
 *
 *	Temporary(?) changes:
 *		#ifdef out fix_objs() & friends (trans_gimage(), vdi_trans(), vdi_fix())
 *
 *	version 2.57 (january/2017)		roger burrows
 *		prepare for public release
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <assert.h>
#include <dos.h>
#include <portab.h>
#include <stdarg.h>
#include <string.h>
#include <vdi.h>

#include "edge.h"
#include "edgedefs.h"
#include "keycodes.h"
#include "map.h"
#include "miscdef.h"
#include "proto.h"
#include "version.h"
#include "xrsrc.h"
#include "lib/driver.h"

#define MODULE_NUMBER	13		/* used by error() macro */

#define NIL 			-1

typedef struct {				// this should perhaps be in xrsrc.h
	USERBLK ublk;
	UWORD old_type;
} OBBLK;


/*
 *	shared globals
 */
extern short mouse_x, mouse_y, num_clicks, key, keystate, xchar, ychar, dev;
extern short msg_buf[8];
extern short bgcolor, indcolor, windhidden, workx, worky, workw, workh;
extern long drivemap;
extern short rez, numflops, pxyarray[10], myhandle, Xw, Yh, nplanes;
extern STATUS status;
extern GRECT gmap;
									/* pointers to trees */
extern OBJECT *about, *menu, *text, *printstr, *dinfo, *bottom, *pcsel, *multi, *chart, *dmap, *prefs;
extern OBJECT *saveinfo, *restinfo, *restconf, *medic, *badpref, *mirror, *bar, *dup, *obar;
extern OBJECT *selscsi, *scsiedit, *newsize, *split, *rebone, *bone, *undel, *undname;


/*
 *	private variables
 */
MLOCAL short mmstate, mxx, myy, nc;
MLOCAL long screen_in_bytes, map_in_bytes;
MLOCAL MFDB screen = { NULL };		/* i.e. screen display memory */
MLOCAL MFDB savescreen = { NULL };	/* for saving entire screen */
MLOCAL MFDB savemap = { NULL };		/* for saving cluster map */
							/* Global variables used by 'mapped' functions */
MLOCAL GRECT br_rect;				/* Current break rectangle	*/
MLOCAL short br_mx, br_my, br_togl;	/* Break mouse posn & flag	*/
MLOCAL short fn_obj;				/* Found tabbable object 	*/
MLOCAL short fn_last;				/* Object tabbing from		*/
MLOCAL short fn_prev;				/* Last EDITABLE obj seen	*/
MLOCAL short fn_dir;				/* 1 = TAB, 0 = BACKTAB 	*/
MLOCAL WORD rscglobal[15];					/* for xrsrc stuff */

MLOCAL OBJECT *mdmap, *hdmap;		/* one of these is assigned to dmap ... */

MLOCAL struct {
	WORD index;
	OBJECT **objptr;
} tree[] = {
/* main.c */	ABOUT, &about, MAIN, &menu, TEXT, &text, PRINTSTR, &printstr, DINFO, &dinfo, BOTTOM, &bottom, PCSEL, &pcsel, MULTIPLE, &multi,
/* disk.c */	DCHART, &chart,
/* dmap.c */	MDMAP, &mdmap, HDMAP, &hdmap,
/* fileio.c */	PREFS, &prefs,
/* info.c */	SAVEINFO, &saveinfo, RESTINFO, &restinfo, RESTCONF, &restconf,
/* medic1.c */	MEDIC, &medic, MBADPREF, &badpref,
/* mirror.c */	MIRROR, &mirror,
/* misc.c */	BAR, &bar, DUPNAME, &dup,
/* opt2.c */	OPTUP, &obar,
/* part1.c */	SELSCSI, &selscsi, EDITSCSI, &scsiedit, NEWSIZE, &newsize, SPPART, &split, REBONE, &rebone, BOOTONLY, &bone,
/* undel.c */	UNDELFRM, &undel, UNDNAME, &undname, -1, NULL
};


/*
 *	function prototypes
 */
PRIVATE short boxcolour(OBJECT *tree,short j);
PRIVATE short break_x(short *pxy);
PRIVATE short break_y(short *pxy);
PRIVATE short break_obj(OBJECT *tree,short obj);
PRIVATE void do_radio(OBJECT *tree,short obj);
PRIVATE short find_def(OBJECT *tree,short obj);
PRIVATE short find_tab(OBJECT *tree,short obj);
//PRIVATE void fix_objs(OBJECT *tree);
PRIVATE CICONBLK *geticon(OBJECT *tree,short idex);
PRIVATE short get_parent(OBJECT *tree,short obj);
PRIVATE void icon_medy(OBJECT *tree,short idex,short amount);
PRIVATE void map_tree(OBJECT *tree,short this,short last,short (*routine)(struct object *, short));
PRIVATE short myform_button(OBJECT *tree,short obj,short clicks,short *next_obj,short *hot_obj,short my,short edit_obj,short *idx,short ks);
PRIVATE short myform_hot(OBJECT *tree,short hot_obj,short mx,short my,GRECT *rect,short *mode);
PRIVATE short myform_keybd(OBJECT *tree,short edit_obj,short next_obj,short kr,short *out_obj,short *okr);
PRIVATE short no3dmask(OBJECT *tree,short j);
PRIVATE short noshadow(OBJECT *tree,short j);
PRIVATE void objc_toggle(OBJECT *tree,short obj);
PRIVATE void objc_xtoggle(OBJECT *tree,short obj);
PRIVATE short scsi_form_button(OBJECT *tree,WORD obj,WORD clicks,WORD *next_obj,WORD *hot_obj,short my,short edit_obj,short *idx);
PRIVATE void set_interior_colour(OBJECT *tree,WORD obj,int colour);
//PRIVATE void trans_gimage(void *obspec,WORD typ);
//PRIVATE void vdi_fix(MFDB *pfd,void *theaddr,WORD wb,WORD h);
//PRIVATE void vdi_trans(WORD *saddr,WORD swb,WORD *daddr,WORD dwb,WORD h);


/********************************************************************/
/* Fixes up embedded resource coordinates, makes monochrome color	*/		//fix description!
/* adjustments, and stops shadowing on Falcons w/ 3D interface		*/
/********************************************************************/
PUBLIC void fix_rsrc()
{
int i;
OBJECT *t;

	if (IS_COLOUR_AES) {
		if ((rez == 2) || (nplanes > 2)) {
			for (i = 0; tree[i].index >= 0; i++) {
				t = *tree[i].objptr;
				switch(tree[i].index) {
				case MAIN:
					break;
				case DCHART:
					set_interior_colour(t,ROOT,bgcolor);	/* this assumes object is BOX/IBOX/BOXCHAR */
					break;
				default:
					map_tree(t,ROOT,NIL,&noshadow);
					break;
				}
			}
		}
	}

	/*
	 *	fix up box interior colours
	 */
	if (nplanes <= 2) {
		for (i = 0; tree[i].index >= 0; i++) {
			t = *tree[i].objptr;
			map_tree(t,ROOT,NIL,&boxcolour);
		}
	}

	/*
	 *	fix up 3D mask
	 */
	if (((rez == 1) && (nplanes == 2)) || !IS_COLOUR_AES) {
		for (i = 0; tree[i].index >= 0; i++) {
			t = *tree[i].objptr;
			map_tree(t,ROOT,NIL,&no3dmask);
		}
	}

#if 0											//rfb: is this necessary???
	for (i = 0; tree[i].index >= 0; i++)
		fix_objs(*tree[i].objptr);
#endif

}

PRIVATE short boxcolour(OBJECT *tree,short j)
{
long value;

	switch(tree[j].ob_type) {
	case G_BOX:
	case G_IBOX:
	case G_BOXCHAR:
		value = (long)tree[j].ob_spec;
		if ((value & 0x0fL) >= 4L) {
			value &= ~0x0fL;			/* reset to white */
			tree[j].ob_spec = (void *)value;
		}
		break;
	case G_BOXTEXT:
		value = ((TEDINFO *)tree[j].ob_spec)->te_color;
		if ((value & 0x0fL) >= 4L) {
			value &= ~0x0fL;			/* reset to white */
			((TEDINFO *)tree[j].ob_spec)->te_color = (short)value;
		}
		break;
	}

	return TRUE;
}

PRIVATE short no3dmask(OBJECT *tree,short j)
{
	tree[j].ob_flags &= ~FL3DMASK;
	return TRUE;
}

PRIVATE short noshadow(OBJECT *tree,short j)
{
	tree[j].ob_state &= ~SHADOWED;
	return TRUE;
}

/*
 *	load resource file and extract some useful values
 */
PUBLIC int load_rsrc()
{
int i;
GRECT desk;

	wind_get(0,WF_WORKXYWH,&desk.g_x,&desk.g_y,&desk.g_w,&desk.g_h);
	if (init_xrsrc(myhandle,&desk,xchar,ychar) == 0)
		return -1;

	if (!xrsrc_load(RSC_FILE,rscglobal))
		return -1;

	/*
	 *	set up external tree pointers (used by other modules)
	 */
	for (i = 0; tree[i].index >= 0; i++)
		if (!xrsrc_gaddr(R_TREE,tree[i].index,tree[i].objptr,rscglobal))
			return -1;

	dmap = (rez==1)?mdmap:hdmap;		/* use map appropriate to character height ... */

	return 0;
}

/*
 *	clean up (neatness counts)
 */
PUBLIC void cleanup_rsrc()
{
	xrsrc_free(rscglobal);			/* finished with resource ... */
	term_xrsrc();					/* not really necessary */
	if (savescreen.fd_addr)			/* this should never happen anyway ... */
		free(savescreen.fd_addr);
	if (savemap.fd_addr)
		free(savemap.fd_addr);
}

/*
 *	issue form_alert, with:
 *		alert string specified via index
 *		variable number of arguments to modify the alert string via vsprintf()
 *
 *	we also ensure that the mouse cursor is visible when the form_alert() is
 *	issued, while preserving the status on exiting the routine
 */
PUBLIC int xform_alert(int dft,int index, ...)
{
va_list args;
char temp[MAX_ALERT_SIZE], **alert;
int was_hidden, rc;

	if (xrsrc_gaddr(R_FRSTR,index,&alert,rscglobal) == 0)
		return dft;

	va_start(args,index);
	vsprintf(temp,*alert,args);
	va_end(args);

	was_hidden = mouse_on();
	rc = form_alert(dft,temp);
	if (was_hidden)
		mouse_off();

	return rc;
}

#if 0
PRIVATE void fix_objs(OBJECT *tree)		//rfb: is this necessary (I guess so)
{
WORD obj;
OBJECT *ob;
ICONBLK *ib;
BITBLK *bi;
UWORD type;
OBBLK *temp;

#if GEM & (GEM2 | GEM3 | XGEM)
	BYTE *s;
#endif

	if (tree != NULL) {
		obj = NIL;
		do {
			ob = &tree[++obj];
			type = ob->ob_type & 0xFF;

			switch (type) {
			case G_ICON:
				ib = (ICONBLK *) ob->ob_spec;
				/* ob->ob_height = ib->ib_ytext + ib->ib_htext;  Objekth�he = Iconh�he */
				ob->ob_height = ib->ib_hicon;
				trans_gimage(ib,type);			/* adapt icon to screen */
				break;
			case G_USERDEF:
				temp = (OBBLK *) ob->ob_spec;
//rfb: the following does not make sense: why are we comparing a pointer to 0x600 ????
				if (((long) ob->ob_spec > 0x600) && (temp->old_type == G_CICON)) {
					ib = (ICONBLK *) ((OBBLK *) ob->ob_spec)->ublk.ub_parm;
					/* ob->ob_height = ib->ib_ytext + ib->ib_htext;  Objekth�he = Iconh�he */
					/* ob->ob_height = ib->ib_hicon; org */
					if (rez == 2)
						ob->ob_height = ib->ib_hicon;
					else ob->ob_height = ib->ib_hicon / 2;
					trans_gimage(ib,G_CICON);	/* adapt colour icons to screen */
				}
				break;
			case G_IMAGE:
				bi = (BITBLK *) ob->ob_spec;
				ob->ob_height = bi->bi_hl;		/* object height = image height */
				trans_gimage(bi,type);			/* adapt bit image to screen */
				break;
			}
		} while (!(ob->ob_flags & LASTOB));
	}
}

PRIVATE void trans_gimage(void *obspec,WORD typ)
{
ICONBLK *piconblk;
BITBLK *pbitblk;
WORD *taddr;
WORD wb, hl;

	switch (typ) {
	case G_CICON:
	case G_ICON:
		piconblk = (ICONBLK *) obspec;
		taddr = piconblk->ib_pmask;
		wb = piconblk->ib_wicon;
		wb = wb >> 3;
		hl = piconblk->ib_hicon;
		vdi_trans(taddr,wb,taddr,wb,hl);
		taddr = piconblk->ib_pdata;
		break;
	case G_IMAGE:
		pbitblk = (BITBLK *) obspec;
		taddr = pbitblk->bi_pdata;
		wb = pbitblk->bi_wb;
		hl = pbitblk->bi_hl;
		break;
	}

	vdi_trans(taddr,wb,taddr,wb,hl);
}

PRIVATE void vdi_trans(WORD *saddr,WORD swb,WORD *daddr,WORD dwb,WORD h)
{
FDB src, dst;

	vdi_fix(&src,saddr,swb,h);
	src.fd_stand = TRUE;

	vdi_fix(&dst,daddr,dwb,h);
	dst.fd_stand = FALSE;

	vr_trnfm(myhandle,&src,&dst);
}

PRIVATE void vdi_fix(MFDB *pfd,void *theaddr,WORD wb,WORD h)
{
	pfd->fd_addr = theaddr;
	pfd->fd_w = wb << 3;
	pfd->fd_h = h;
	pfd->fd_wdwidth = wb >> 1;
	pfd->fd_nplanes = 1;
}
#endif

/*
 *	returns pointer to CICONBLK
 */
PRIVATE CICONBLK *geticon(OBJECT *tree,short idex)
{
OBBLK *obblk;
CICONBLK *icon;

	icon = tree[idex].ob_spec;
	if (tree[idex].ob_type == G_USERDEF) {
		obblk = (OBBLK *)icon;
		if (obblk->old_type == G_CICON)
			icon = (CICONBLK *)obblk->ublk.ub_parm;
	}

	return icon;
}


/****************************************************************
 *																*
 *	the following functions are the original contents of rsc.c	*
 *																*
 ****************************************************************/
PUBLIC void init_forms(void)
{
short i, j;
int device;
WORD *ibc_ptr;
void *bspec, *cboxspec, *icspec;
short btype, cboxtype, ictype;
CICONBLK *icon;

	/*
	 *	test if low-level I/O interface is available; if not,
	 *	disable any function that might use it
	 */
	if (init_driver() < 0) {
		menu[MPART].ob_state = DISABLED;
		menu[USSCSI].ob_state = DISABLED;
		menu[URSCSI].ob_state = DISABLED;
	}

	if (rez == 1) {
		icon_medy(bottom,BACTIVEF,11);					//MAGNUM
		icon_medy(bottom,BACTIVEH,6);					//MAGNUM

		obar->ob_state &= ~SHADOWED;
		undel[UNDELBCK].ob_x -= 3;

		/* dmap[MMAPAREA].ob_x -= 2; */
		dmap[MMAPAREA].ob_state |= SHADOWED;
		dmap[MFAREA].ob_state |= SHADOWED;
		dmap[MMAPEXT].ob_state |= SHADOWED;
		B0 = MB0;
		B1 = MB1;
		B2 = MB2;
		B3 = MB3;
		B4 = MB4;
		S0 = MS0;
		S1 = MS1;
		S2 = MS2;
		S3 = MS3;
		S4 = MS4;
		FOLDER = MFOLDER;
		FILES = MFILES;
		FRAG = MFRAG;
		FRAGPFF = MFRAGPFF;
		FRAG100 = MFRAG100;
		BAR1 = MBAR1;
		BAR2 = MBAR2;
		BAR3 = MBAR3;
		BAR4 = MBAR4;
		BAR5 = MBAR5;
		BAR6 = MBAR6;
		BAR7 = MBAR7;
		MAPAREA = MMAPAREA;
		FRAGI = MFRAGI;
		USED = MUSED;
		FAREA = MFAREA;
		FMAPBPC = MFMAPBPC;
		FSTAT = MFSTAT;

		/* medic[MDAREA].ob_x -= 2; 	*/
		medic[MDAREA].ob_state |= SHADOWED;
		medic[MSAREA].ob_state |= SHADOWED;
		medic[MRAREA].ob_state |= SHADOWED;
		medic[MBAREA].ob_state |= SHADOWED;
		medic[MEDBACK].ob_state |= SHADOWED;
		medic[MDAREA].ob_height -= 4;
		chart[GRANDTOT].ob_y += 2;
	} else {
		B0 = HB0;
		B1 = HB1;
		B2 = HB2;
		B3 = HB3;
		B4 = HB4;
		S0 = HS0;
		S1 = HS1;
		S2 = HS2;
		S3 = HS3;
		S4 = HS4;
		FOLDER = HFOLDER;
		FILES = HFILES;
		FRAG = HFRAG;
		FRAGPFF = HFRAGPFF;
		FRAG100 = HFRAG100;
		BAR1 = HBAR1;
		BAR2 = HBAR2;
		BAR3 = HBAR3;
		BAR4 = HBAR4;
		BAR5 = HBAR5;
		BAR6 = HBAR6;
		BAR7 = HBAR7;
		MAPAREA = HMAPAREA;
		FRAGI = HFRAGI;
		USED = HUSED;
		FAREA = HFAREA;
		FMAPBPC = HFMAPBPC;
		FSTAT = MFSTAT;
	}

	if (nplanes == 1) {
		dmap[FRAGI].ob_spec = GREYBOX;
		dmap[USED].ob_spec = BLACKBOX;
		medic[MEDPROB].ob_spec = GREYBOX;
		medic[MEDOK].ob_spec = BLACKBOX;
	}

	if (IS_COLOUR_AES) {
		/* dmap[MAPAREA].ob_x -=2; */
		assert(medic[MDAREA].ob_x==dmap[MAPAREA].ob_x);		//rfb: test theory below
		medic[MDAREA].ob_x = dmap[MAPAREA].ob_x;			//rfb: aren't they already the same?
		obar->ob_state &= ~SHADOWED;
		obar->ob_height += 1;

		if (undel[UUP].ob_flags & FL3DMASK) {
			undel[UUP].ob_y += 5;
			undel[UBARBACK].ob_y += 10;
			undel[UBARBACK].ob_height -= 15;
			undel[UTOP].ob_x += 6;
			undel[UTOP].ob_width -= 6;
		}
		if (rez == 1)
			undel[USTR1].ob_y += 1;
	} else if (nplanes == 1) {
		undel[UUP].ob_y += 1;
		undel[UBARBACK].ob_y += 1;
		undel[UBARBACK].ob_height -= 1;
	}

	savescreen.fd_w = Xw + 1;								/* fd_addr is already NULL */
	savescreen.fd_h = Yh + 1;
	savescreen.fd_stand = 0;
	savescreen.fd_nplanes = nplanes;
	savescreen.fd_wdwidth = (savescreen.fd_w + 15) / 16;
	screen_in_bytes = sizeof(WORD) * savescreen.fd_wdwidth * savescreen.fd_h * nplanes;

	objc_offset(dmap,MAPAREA,&gmap.g_x,&gmap.g_y);
															/* fd_addr is already NULL */
	savemap.fd_w = gmap.g_w = 370;								//rfb: shouldn't this be dmap[MAPAREA].ob_width ?
	savemap.fd_h = gmap.g_h = dmap[MAPAREA].ob_height;
	savemap.fd_stand = 0;
	savemap.fd_nplanes = nplanes;
	savemap.fd_wdwidth = (savemap.fd_w+15) / 16;
	map_in_bytes = sizeof(WORD) * savemap.fd_wdwidth * savemap.fd_h * nplanes;

	if (IS_COLOUR_AES) {						/* set colours for map legends */
		set_interior_colour(medic,MEDOK,GREEN);
		set_interior_colour(medic,MEDFREE,indcolor);
		set_interior_colour(hdmap,HUSED,GREEN);
		set_interior_colour(hdmap,HFREE,indcolor);
		set_interior_colour(mdmap,MUSED,GREEN);
	}

	if (rez == 1) {								/* adjust heights of map legends */
		medic[MEDFREE].ob_state &= ~OUTLINED;
		medic[MEDOK].ob_state &= ~OUTLINED;
		medic[MEDPROB].ob_state &= ~OUTLINED;
		medic[MEDFREE].ob_height /= 2;
		medic[MEDOK].ob_height /= 2;
		medic[MEDPROB].ob_height /= 2;
		medic[MEDFREE].ob_y += 2;
		medic[MEDOK].ob_y += 2;
		medic[MEDPROB].ob_y += 2;
	}

	/******setting up about icons & version*************/
	icspec = about[MEDICIC].ob_spec;
	ictype = about[MEDICIC].ob_type;
	about[AB2DB3].ob_spec = icspec;
	about[AB2DB3].ob_type = ictype;
	about[ABVERSION].ob_spec = PROGRAM_VERSION;


	/******setting up prefs*************/
	cboxspec = prefs[NOVICEU].ob_spec;
	cboxtype = prefs[NOVICEU].ob_type;

	prefs[EXPERTU].ob_spec = cboxspec;
	prefs[EXPERTU].ob_type = cboxtype;
	prefs[WARNDL].ob_spec = cboxspec;
	prefs[WARNDL].ob_type = cboxtype;
	prefs[WARNN].ob_spec = cboxspec;
	prefs[WARNN].ob_type = cboxtype;

	prefs[PREFIC].ob_spec = icspec;
	prefs[PREFIC].ob_type = ictype;


	/******setting up boot only************/
	bone[BOOTAT].ob_spec = cboxspec;
	bone[BOOTAT].ob_type = cboxtype;
	bone[BOOTICD].ob_spec = cboxspec;
	bone[BOOTICD].ob_type = cboxtype;


	/******setting up drive selectors*************/
	if (numflops == 1)
		drivemap &= ~0x02;

	if (dev < 2) {
		bottom[BACTIVEF].ob_flags &= ~HIDETREE;
		bottom[BACTIVEH].ob_flags |= HIDETREE;
		j = BACTIVEF;
	} else {
		bottom[BACTIVEF].ob_flags |= HIDETREE;
		bottom[BACTIVEH].ob_flags &= ~HIDETREE;
		j = BACTIVEH;
	}
	icon = geticon(bottom,j);
	ibc_ptr = &icon->monoblk.ib_char;
	*ibc_ptr = (*ibc_ptr & 0xff00) | (dev+'A');			/* set drive letter */

	undel[UNDEL].ob_spec = bottom[BUNDEL].ob_spec;
	undel[UNDEL].ob_type = bottom[BUNDEL].ob_type;
	undel[UDSIM].ob_spec = cboxspec;
	undel[UDSIM].ob_type = cboxtype;
	undel[UDMIR].ob_spec = cboxspec;
	undel[UDMIR].ob_type = cboxtype;

	for (device = 0; device < NUM_DEVICES; device++) {
		if (!((1L<<device) & drivemap)) {
			pcsel[PCA+device].ob_state |= DISABLED;
			pcsel[PCA+device].ob_flags = 0;

			saveinfo[SIA+device].ob_state |= DISABLED;
			saveinfo[SIA+device].ob_flags = 0;

			restinfo[RESTA+device].ob_state |= DISABLED;
			restinfo[RESTA+device].ob_flags = 0;

			multi[AMULTI+device].ob_state |= DISABLED;
			multi[AMULTI+device].ob_flags = 0;
			if (device >= FIRST_HD_DEVICE) {
				mirror[MTC+device-FIRST_HD_DEVICE].ob_state |= DISABLED;
				mirror[MTC+device-FIRST_HD_DEVICE].ob_flags = 0;

				mirror[MTC+device-FIRST_HD_DEVICE+NUM_HD_DEVICES].ob_state |= DISABLED;
				mirror[MTC+device-FIRST_HD_DEVICE+NUM_HD_DEVICES].ob_flags = 0;
			}
		}
		bspec = pcsel[PCA+device].ob_spec;
		btype = pcsel[PCA+device].ob_type;

		saveinfo[SIA+device].ob_spec = bspec;
		saveinfo[SIA+device].ob_type = btype;

		restinfo[RESTA+device].ob_spec = bspec;
		restinfo[RESTA+device].ob_type = btype;

		multi[AMULTI+device].ob_spec = bspec;
		multi[AMULTI+device].ob_type = btype;

		if (device >= FIRST_HD_DEVICE) {
			mirror[MTC+device-FIRST_HD_DEVICE].ob_spec = bspec;
			mirror[MTC+device-FIRST_HD_DEVICE].ob_type = btype;

			mirror[MMC+device-FIRST_HD_DEVICE].ob_spec = bspec;
			mirror[MMC+device-FIRST_HD_DEVICE].ob_type = btype;
		}

		if (rez == 1) {
			if (device < FIRST_HD_DEVICE)
				i = 3;
			else i = 1;
			icon_medy(pcsel,PCA+device,i);
			icon_medy(saveinfo,SIA+device,i);
			icon_medy(restinfo,RESTA+device,i);
			icon_medy(multi,AMULTI+device,i);
			if (device >= FIRST_HD_DEVICE) {
				icon_medy(mirror,MTC+device-FIRST_HD_DEVICE,1);
				icon_medy(mirror,MMC+device-FIRST_HD_DEVICE,1);
			}
		}
	}

	/***********finishing Mirror setup*************/
	mirror[MALWAYS].ob_spec = cboxspec;
	mirror[MALWAYS].ob_type = cboxtype;
	mirror[MONCEAD].ob_spec = cboxspec;
	mirror[MONCEAD].ob_type = cboxtype;
	mirror[MONCEAWK].ob_spec = cboxspec;
	mirror[MONCEAWK].ob_type = cboxtype;

	mirror[TALWAYS].ob_spec = cboxspec;
	mirror[TALWAYS].ob_type = cboxtype;
	mirror[TONCEAD].ob_spec = cboxspec;
	mirror[TONCEAD].ob_type = cboxtype;
	mirror[TONCEAWK].ob_spec = cboxspec;
	mirror[TONCEAWK].ob_type = cboxtype;

	mirror[MIRIC].ob_spec = icspec;
	mirror[MIRIC].ob_type = ictype;

	/* setting up preferences */
	init_prefs();
	init_mirror_prefs();
	init_scsi_editobject();
	init_ud_form();
}

PRIVATE void set_interior_colour(OBJECT *tree,WORD obj,int colour)
{
long value;

	value = (long)tree[obj].ob_spec;
	value &= ~0x0fL;			/* clear interior colour */
	value |= colour&0x0fL;		/* insert new colour */
	tree[obj].ob_spec = (void *)value;
}

/**************************************************************************/
PUBLIC void redraw_object(OBJECT *tree,short idex)
{
short xoff, yoff;

	/*
	 *	perhaps we ought to use redraw_xobject() for everything, but this
	 *	should fix some redraw problems without causing too much overhead
	 */
	if (tree->ob_flags&FL3DMASK) {
		redraw_xobject(tree,idex);
		return;
	}

	objc_offset(tree,idex,&xoff,&yoff);
	objc_draw(tree,0,MAX_DEPTH,xoff,yoff,tree[idex].ob_width,tree[idex].ob_height);
}

/**************************************************************************/
PUBLIC void redraw_xobject(OBJECT *tree,short idex)
{
short xoff, yoff;

	objc_offset(tree,idex,&xoff,&yoff);
	objc_draw(tree,0,MAX_DEPTH,xoff-3,yoff-3*rez,tree[idex].ob_width+6,tree[idex].ob_height+6*rez);
}

/**************************************************************************/
PUBLIC void obj_head(OBJECT *tree)
{
	objc_draw(tree,0,MAX_DEPTH,tree->ob_x-3,tree->ob_y-3*rez,
				tree->ob_width+6,tree->ob_height+6*rez);
}

/****************************************************************/
PUBLIC void refresh_now(OBJECT *tree,short obj)
{
short xx, yy;
GRECT r;

	objc_offset(tree,obj,&xx,&yy);
	r.g_x = xx - 3;
	r.g_y = yy - 3*rez;
	r.g_w = tree[obj].ob_width + 6;
	r.g_h = tree[obj].ob_height + 6*rez;
	form_dial(FMD_FINISH,0,0,0,0,r.g_x,r.g_y,r.g_w,r.g_h);
	if (!windhidden)
		wait_for_redraw(0);
}

/****************************************************************/
PUBLIC void wait_for_redraw(short win)
{
short event, dummy;
short still_waiting = 1;

	while(still_waiting) {
		event = evnt_multi(MU_KEYBD | MU_MESAG | MU_BUTTON | MU_TIMER,
							NUM_CLICKS, LEFT_BUTTON, BUTTON_DOWN,
							0, 0, 0, 0, 0,
							0, 0, 0, 0, 0,
							msg_buf, 1, 0,
							&mouse_x, &mouse_y, &dummy, &dummy, &key, &num_clicks);

		if (event & MU_MESAG) {
			if (msg_buf[0] == WM_REDRAW) {
				if (!win)
					still_waiting = 0;
				if (msg_buf[3] == win)
					still_waiting = 0;
			}
			handle_messages();
		}
	}
}

/****************************************************************/
PUBLIC int handle_dial(OBJECT *tree,short txt,short start,short dohot)
{
	tree->ob_x = workx + (workw - tree->ob_width) / 2;
	tree->ob_y = worky + (workh - tree->ob_height) / 2;

	return do_dial(tree,txt,start,dohot);
}

/****************************************************************/
PUBLIC int do_dial(OBJECT *tree,short txt,short start,short dohot)
{
short xdial, ydial, wdial, hdial, exitobj;
short txt1;

	txt1 = txt;
	xdial = tree->ob_x - 3;
	ydial = tree->ob_y - 3*rez;
	wdial = tree->ob_width + 6;
	hdial = tree->ob_height + 6*rez;
	objc_draw(tree,start,MAX_DEPTH,xdial,ydial,wdial,hdial);
	exitobj = myform_do(tree,&txt1,dohot) & 0x7fff;
	tree[exitobj].ob_state &= ~SELECTED;
	refresh_now(tree,0);

	return exitobj;
}

/************************************/
PUBLIC void draw_dial(OBJECT *tree,short start)
{
short xdial, ydial, wdial, hdial;

	myform_center(tree);
	xdial = tree->ob_x - 3;
	ydial = tree->ob_y - 3*rez;
	wdial = tree->ob_width + 6;
	hdial = tree->ob_height + 6*rez;
	objc_draw(tree,start,MAX_DEPTH,xdial,ydial,wdial,hdial);
}

/************* Utility routines for custom forms manager ***************/
/* Reverse the SELECTED state of an object, and redraw it immediately. */
/***********************************************************************/
PRIVATE void objc_toggle(OBJECT *tree,short obj)
{
	tree[obj].ob_state ^= SELECTED;
	redraw_object(tree,obj);
}

/* Reverse the SELECTED state of an object, and redraw it immediately, */
/* but do a border round the object too */
PRIVATE void objc_xtoggle(OBJECT *tree,short obj)
{
	tree[obj].ob_state ^= SELECTED;
	redraw_xobject(tree,obj);
}

/*************************************************************/
/* If the object is not already SELECTED, make it so.		 */
/*************************************************************/
PUBLIC void objc_sel(OBJECT *tree,short obj)
{
	if (!(tree[obj].ob_state & SELECTED))
		objc_toggle(tree,obj);
}

/*************************************************************/
/* If the object is SELECTED, then deselect it. 			 */
/*************************************************************/
PUBLIC void objc_dsel(OBJECT *tree,short obj)
{
	if (tree[obj].ob_state & SELECTED)
		objc_toggle(tree,obj);
}

/*************************************************************/
/* If the object is not already SELECTED, make it so.		 */
/*************************************************************/
PUBLIC void objc_xsel(OBJECT *tree,short obj)
{
	if (!(tree[obj].ob_state & SELECTED))
		objc_xtoggle(tree,obj);
}

/*************************************************************/
/* If the object is SELECTED, then deselect it. 			 */
/*************************************************************/
PUBLIC void objc_xdsel(OBJECT *tree,short obj)
{
	if (tree[obj].ob_state & SELECTED)
		objc_xtoggle(tree,obj);
}

/**************************************************************************************/
/* Non-cursive traverse of an object tree.	This routine is described in PRO GEM #5.  */
/**************************************************************************************/
PRIVATE void map_tree(OBJECT *tree,short this,short last,short (*routine)(OBJECT *,short))
{
short tmp1;

	tmp1 = this;				/* Initialize to impossible value: TAIL won't point to self!	*/
								/* Look until final node, or off the end of tree				*/
	while (this != last && this != NIL) {	/* Did we 'pop' into this node for the second time? */
		if (tree[this].ob_tail != tmp1) {
			tmp1 = this;					/* This is a new node		*/
			this = NIL;
			if ((*routine)(tree,tmp1))
				this = tree[tmp1].ob_head;	/* Apply operation, testing for rejection of sub-tree */
			if (this == NIL)
				this = tree[tmp1].ob_next;	/* Subtree path not taken, so traverse right */
		} else {						/* Revisiting parent: No operation, move right	*/
			tmp1 = this;
			this = tree[tmp1].ob_next;
		}
	}
}

/************************************************************/
/* Find the parent object of by traversing right until we	*/
/* find nodes whose NEXT and TAIL links point to each other	*/
/* returns NIL if there is no parent (i.e. this is ROOT)    */
/************************************************************/
PRIVATE short get_parent(OBJECT *tree,short obj)
{
short pobj;

	if (obj == NIL)
		return NIL;

	pobj = tree[obj].ob_next;
	if (pobj != NIL) {
		while (tree[pobj].ob_tail != obj) {
			obj = pobj;
			pobj = tree[obj].ob_next;
		}
	}
	return pobj;
}

/*****************************************************************/
/************* "Hot-spot" manager and subroutines  ***************/
/*****************************************************************/
PRIVATE short break_x(short *pxy)
{
	if (br_mx < pxy[0]) {		/* Breaking object is right of	mouse. Reduce width of bounding
								 * rectangle. */
		br_rect.g_w = pxy[0] - br_rect.g_x;
		return TRUE;
	}
	if (br_mx > pxy[2]) {		/* Object to left. Reduce width and move rect. to right  */
		br_rect.g_w += br_rect.g_x - pxy[2] - 1;
		br_rect.g_x = pxy[2] + 1;
		return TRUE;
	}
	/* Emergency escape test! Protection vs. turkeys who nest */
	/* non-selectable objects inside of selectables.		  */
	if (br_mx >= pxy[0] && br_mx <= pxy[2]) {	/* Will X break fail?	  If so, punt!			*/
		br_rect.g_x = br_mx;
		br_rect.g_y = br_my;
		br_rect.g_w = br_rect.g_h = 1;
		return TRUE;
	}

	return FALSE; 				/* Mouse within object segment.Break attempt fails. 		*/
}

/*****************************************************************/
PRIVATE short break_y(short *pxy)
{
	if (br_my < pxy[1]) {		/* Object below mouse.	Reduce height of bounding rect. */
		br_rect.g_h = pxy[1] - br_rect.g_y;
		return TRUE;
	}
	if (br_my > pxy[3]) {		/* Object above mouse.	Reduce height and shift downward. */
		br_rect.g_h += br_rect.g_y - pxy[3] - 1;
		br_rect.g_y = pxy[3] + 1;
		return TRUE;
	}
	/* Emergency escape test! Protection vs. turkeys who nest */
	/* non-selectable objects inside of selectables.		  */
	if (br_mx >= pxy[0] && br_mx <= pxy[2]) {	/* Will X break fail?	  If so, punt!			*/
		br_rect.g_x = br_mx;
		br_rect.g_y = br_my;
		br_rect.g_w = br_rect.g_h = 1;
		return TRUE;
	}

	return FALSE;
}

/********************************************************************************/
/* Called once per object to check if the bounding rect.needs to be modified.	*/
/********************************************************************************/
PRIVATE short break_obj(OBJECT *tree,short obj)
{
GRECT s;
short broken, pxy[4];

	objc_xywh(tree,obj,&s);
	if (!rc_intersect(&br_rect,&s))
		return FALSE; 			/* Trivial rejection case		*/
	grect_to_array(&s,pxy);

	if (tree[obj].ob_flags & HIDETREE)
		return FALSE;	 		/* Is this object a potential hot-spot?   */
	if (!(tree[obj].ob_flags & SELECTABLE))
		return TRUE;
	if (tree[obj].ob_state & DISABLED)
		return TRUE;

	for (broken = FALSE; !broken; ) {	/* This could take two passes if the first break fails.  */
		if (br_togl) {
			broken = break_x(pxy);
		} else {
			broken = break_y(pxy);
		}
		br_togl = !br_togl;
	}
	return TRUE;
}

/**********************************/
/* Manages mouse rectangle events */
/**********************************/
PRIVATE short myform_hot(OBJECT *tree,short hot_obj,short mx,short my,GRECT *rect,short *mode)
{
GRECT root;
short pobj;

	objc_xywh(tree,ROOT,&root);
	if (hot_obj != NIL)
		objc_toggle(tree,hot_obj);		/* If there is already a hot-spot turn it off */
	if (!(rc_inside(mx,my,&root))) {	/* Mouse has moved outside of the dialog.  Wait for return.	*/
		*rect = root;
		*mode = M1_ENTER;
		return NIL;
	}
	hot_obj = objc_find(tree,ROOT,MAX_DEPTH,mx,my); /* What object is mouse over?	  (Hit is
														 * guaranteed.) 		  */

	pobj = get_parent(tree,hot_obj);
	if (pobj > ROOT)
		hot_obj = pobj;

	/*
	 * state = tree[hot_obj].ob_state;	Is this object a hot-spot?
	 */
	if (tree[hot_obj].ob_flags & SELECTABLE) {
		if (!(tree[hot_obj].ob_state & DISABLED)) { 	/* Yes!  Set up wait state. 	  */
			*mode = M1_EXIT;
			objc_xywh(tree,hot_obj,rect);
			if (tree[hot_obj].ob_state & SELECTED) {	/* But only toggle if it's not already
														 * SELECTED! */
				return NIL;
			} else {
				objc_toggle(tree,hot_obj);
				return hot_obj;
			}
		}
	}
	br_rect = root;				/* No hot object, so compute	*/
	br_mx = mx; 				/* mouse bounding rectangle.	*/
	br_my = my;
	br_togl = 0;
	map_tree(tree,ROOT,NIL,break_obj);
	*rect = br_rect;			/* Then return to wait state.	*/
	*mode = M1_EXIT;

	return NIL;
}


/**************************************************************/
/************* Keyboard manager and subroutines ***************/
/**************************************************************/
/* Check if the object is DEFAULT		*/
PRIVATE short find_def(OBJECT *tree,short obj)
{
	if (HIDETREE & tree[obj].ob_flags)
		return FALSE;	 		/* Is sub-tree hidden?					*/

	if (DEFAULT & tree[obj].ob_flags) { /* Must be DEFAULT and not DISABLED 	*/
		if (!(DISABLED & tree[obj].ob_state))
			fn_obj = obj;		/* Record object number 				*/
	}

	return TRUE;
}

/****************************************/
/* Look for target of TAB operation.	*/
/****************************************/
PRIVATE short find_tab(OBJECT *tree,short obj)
{
	if (HIDETREE & tree[obj].ob_flags)
		return FALSE;	 		/* Check for hidden subtree.			*/

	if (!(EDITABLE & tree[obj].ob_flags))
		return TRUE;			/* If not EDITABLE, who cares?			*/

	if (fn_dir && fn_prev == fn_last)
		fn_obj = obj;			/* Check for forward tab match			*/

	if (!fn_dir && obj == fn_last)
		fn_obj = fn_prev;		/* Check for backward tab match 		*/

	fn_prev = obj;				/* Record object for next call. 		*/
	return TRUE;
}

/************************************************************************************************/
PRIVATE short myform_keybd(OBJECT *tree,short edit_obj,short next_obj,short kr,short *out_obj,short *okr)
{
	/* if (LLOBT(kr)) kr &= 0xff;	 If lower byte valid, mask out extended code byte.			 */

	fn_dir = 0; 				/* Default tab direction if backward.	*/
	switch (kr) {
	case ENTR:
	case CR:
		*okr = 0;				/* Zap character. */
		fn_obj = NIL;
		map_tree(tree,ROOT,NIL,find_def);	/* Look for a DEFAULT object. */
		if (fn_obj != NIL) {				/* If found, SELECT and force exit.  */
			objc_xsel(tree,fn_obj);
			*out_obj = fn_obj;
			return FALSE;
		}
	case TAB:					/* Falls through to tab if no default	 */
	case DN_ARROW:
		fn_dir = 1; 			/* Set fwd direction	*/
	case BTAB:
	case UP_ARROW:
		*okr = 0;				/* Zap character		*/
		fn_last = edit_obj;
		fn_prev = fn_obj = NIL; /* Look for TAB object	*/
		map_tree(tree,ROOT,NIL,find_tab);
		if (fn_obj == NIL)
			map_tree(tree,ROOT,NIL,find_tab);		/* try to wrap around	*/
		if (fn_obj != NIL) {
			if (tree[fn_obj].ob_state & DISABLED)
				break;
			*out_obj = fn_obj;
		}
		break;
	default:					/* Pass other chars 	*/
		return TRUE;
	}

	return TRUE;
}

/*************************************************************************/
PUBLIC int mywatchbox(OBJECT *tree,short idex,short instate,short outstate)
{
GRECT rect;
short x, y, event, dum;
short mx, my;
short watchmode;

	objc_offset(tree,idex,&x,&y);
	rect.g_x = x;
	rect.g_y = y;
	rect.g_w = tree[idex].ob_width;
	rect.g_h = tree[idex].ob_height;

	watchmode = M1_EXIT;
	graf_mkstate(&mx,&my,&event,&dum);
	if (!event)
		return 1;

	if (rc_inside(mx,my,&rect))
		watchmode = M1_ENTER;
	else watchmode = M1_EXIT;
	event = MU_M1;				/* fake for initial test in while() loop */

	while (TRUE) {
		if (event & MU_M1) {
			if (watchmode == M1_ENTER) {
				tree[idex].ob_state = instate;
				redraw_xobject(tree,idex);
				watchmode = M1_EXIT;
			} else {
				tree[idex].ob_state = outstate;
				redraw_xobject(tree,idex);
				watchmode = M1_ENTER;
			}
		}

		if (event & MU_BUTTON)
			return rc_inside(mx,my,&rect);

		event = evnt_multi(MU_KEYBD | MU_MESAG | MU_BUTTON | MU_M1,
							NUM_CLICKS, LEFT_BUTTON, BUTTON_UP,
							watchmode, rect.g_x, rect.g_y, rect.g_w, rect.g_h,
							0, 0, 0, 0, 0,
							msg_buf, 0, 0,
							&mx, &my, &dum, &dum, &dum, &dum);
	}
}

/******************************************************************/
/************* Mouse button manager and subroutines ***************/
/******************************************************************/
PRIVATE void do_radio(OBJECT *tree,short obj)
{
GRECT root;
short pobj, sobj;

	objc_xywh(tree,ROOT,&root);
	pobj = get_parent(tree,obj);		/* Get the object's parent */
	for (sobj = tree[pobj].ob_head; sobj != pobj; sobj = tree[sobj].ob_next) {
		if ((sobj != obj) && (tree[sobj].ob_flags & RBUTTON)) {
			objc_xdsel(tree,sobj);		/* Deselect all but...	   old */
		}
	}
	objc_xsel(tree,obj);				/* the one being SELECTED  */
}

/***************************/
/* Mouse button handler    */
/***************************/
PRIVATE short myform_button(OBJECT *tree,short obj,short clicks,short *next_obj,short *hot_obj,short my,short edit_obj,short *idx,short ks)
{
short flags, state, hibit, in_state, texit, sble, dsbld, edit, pobj;

	flags = tree[obj].ob_flags; /* Get flags and states   */
	state = tree[obj].ob_state;
	texit = flags & TOUCHEXIT;
	sble = flags & SELECTABLE;
	dsbld = state & DISABLED;
	edit = flags & EDITABLE;

	if (edit & dsbld) {
		*next_obj = 0;
		ring_bell();
		return TRUE;
	}

	if (!texit && (!sble || dsbld) && !edit) {	/* Normally uninteresting object but may have custom processing */
		pobj = get_parent(tree,obj);
		if (pobj <= ROOT) {
			*next_obj = 0;
			ring_bell();
			return TRUE;
		}
		obj = pobj;
		flags = tree[obj].ob_flags;	/* Get flags and states   */
		state = tree[obj].ob_state;
		texit = flags & TOUCHEXIT;
		sble = flags & SELECTABLE;
		dsbld = state & DISABLED;
		edit = flags & EDITABLE;
		if (!sble) {
			*next_obj = 0;
			ring_bell();
			return TRUE;
		}
	}

	if (texit && clicks == 2)	/* Preset special flag	*/
		hibit = 0x8000;
	else hibit = 0x0000;

	if (sble && !dsbld) {		/* Hot stuff!			*/
		if (flags & RBUTTON) {	/* Process radio buttons */
			do_radio(tree,obj);	/* immediately! 		*/
		} else if (!texit) {
			in_state = (obj == *hot_obj) ? state : state ^ SELECTED;	/* Already toggled ? */
			if (!mywatchbox(tree,obj,in_state,in_state^SELECTED)) {		/* He gave up...  */
				*next_obj = 0;
				*hot_obj = NIL;
				return TRUE;
			}
		} else {						/* if (texit) */
			if (obj != *hot_obj)
				objc_toggle(tree,obj);	/* Force SELECTED		*/
		}
	}

	if (obj == *hot_obj)
		*hot_obj = NIL; 				/* We're gonna do it! So don't turn it off later.			*/

	if (texit || (flags & EXIT)) {		/* Exit conditions. 			*/
		*next_obj = obj | hibit;
		return FALSE;	 				/* Time to leave!				*/
	}

	if (!edit)	 						/* Clear object unless tabbing	*/
		*next_obj = 0;

	return TRUE;
}


/************************************************************************/
/************* New forms manager: Entry point and main loop *************/
/************************************************************************/
PUBLIC int myform_do(OBJECT *tree,short *start_fld,short do_hot)
{
register short edit_obj;
short next_obj, hot_obj, hot_mode;
short which, cont;
short idx, buttonevent;
short mx, my, mb, ks, kr, br;
GRECT hot_rect;

	wind_update(BEG_UPDATE);
	wind_update(BEG_MCTRL);

	next_obj = *start_fld;		/* Init. editing		*/
	edit_obj = 0;
	hot_obj = NIL;				/* Initial hotspot cndx */
	hot_mode = M1_ENTER;
	cont = TRUE;
	objc_xywh(tree,ROOT,&hot_rect);
	buttonevent = 2 - do_hot;

	while (cont) {				/* Main event loop		*/
		if (edit_obj != next_obj) { 	/* position cursor on	*/
			if (next_obj != 0) {/* the selected 	  */
				edit_obj = next_obj;	/* editting field	  */
				next_obj = 0;
				objc_edit(tree,edit_obj,0,&idx,EDINIT);
			}
		}
		/* wait for button or key or rectangle 	*/
		if (do_hot) {
			which = evnt_multi(MU_KEYBD | MU_BUTTON | MU_M1,
								2, LEFT_BUTTON, buttonevent,
								hot_mode, hot_rect.g_x, hot_rect.g_y, hot_rect.g_w, hot_rect.g_h,
								0, 0, 0, 0, 0,
								NULL, 0, 0,
								&mx, &my, &mb, &ks, &kr, &br);

			if (which & MU_M1) {/* handle rect. event	*/
				hot_obj = myform_hot(tree,hot_obj,mx,my,&hot_rect,&hot_mode);
			}
		} else {
			which = evnt_multi(MU_KEYBD | MU_BUTTON | MU_M1,
								2, LEFT_BUTTON, BUTTON_DOWN,
								hot_mode, hot_rect.g_x, hot_rect.g_y, hot_rect.g_w, hot_rect.g_h,
								0, 0, 0, 0, 0,
								NULL, 0, 0,
								&mx, &my, &mb, &ks, &kr, &br);
		}

		if (which & MU_KEYBD) { /* handle keyboard event */
			cont = myform_keybd(tree,edit_obj,next_obj,kr,&next_obj,&kr);	/* Control char filter	*/
			if (kr && edit_obj) /* Add others to object */
				objc_edit(tree,edit_obj,kr,&idx,EDCHAR);
		}
		if (which & MU_BUTTON) {/* handle button event	*/
			next_obj = objc_find(tree,ROOT,MAX_DEPTH,mx,my);		/* Which object hit?	*/
			if (next_obj == NIL) {
				next_obj = 0;
				if (do_hot) {
					cont = 0;
				} else {
					ring_bell();
				}
			} else {			/* Process a click		*/
				cont = myform_button(tree,next_obj,br,&next_obj,&hot_obj,my,edit_obj,&idx,ks);
			}
		}
		/* handle end of field clean up		   */
		if (!cont || (next_obj != edit_obj && next_obj != 0)) {
			if (edit_obj != 0)
				objc_edit(tree,edit_obj,0,&idx,EDEND);
		}
	}

	if (hot_obj != (next_obj & 0x7fff)) {		/* If defaulted, may need to clear hotspot */
		if (hot_obj != NIL)
			objc_toggle(tree,hot_obj);
	}
	*start_fld = edit_obj;		/* return exit object - hi bit may be set  */
	wind_update(END_MCTRL);
	wind_update(END_UPDATE);

	return next_obj;			/* if exit obj. was double-clicked	 */
}

/*****************************************************************************/
/************* New scsi forms manager: Entry point and main loop *************/
/*****************************************************************************/
PUBLIC int scsi_form_do(OBJECT *tree,WORD *start_fld)
{
WORD edit_obj;
WORD next_obj, hot_obj, hot_mode;
WORD which, cont;
WORD idx;
WORD mx, my, mb, kr, br;
GRECT hot_rect;

	/* Init. editing		*/
	next_obj = *start_fld;
	edit_obj = 0;

	/* Initial hotspot cndx */
	hot_obj = NIL;
	hot_mode = M1_ENTER;
	objc_xywh(tree,ROOT,&hot_rect);

	/* Main event loop		*/
	cont = TRUE;
	while(cont) {
		/*
		 * position cursor on the selected editing field
		 */
		if (edit_obj != next_obj)
			if (next_obj != 0) {
				edit_obj = next_obj;
				next_obj = 0;
				objc_edit(tree,edit_obj,0,&idx,EDINIT);
			}
		/*
		 * wait for button or key or rectangle
		 */
		which = evnt_multi(MU_KEYBD | MU_BUTTON,
							2, LEFT_BUTTON, BUTTON_DOWN,
							0, 0, 0, 0, 0,
							0, 0, 0, 0, 0,
							NULL, 0, 0,
							&mx, &my, &mb, &keystate, &kr, &br);

		if (which & MU_KEYBD) { 	/* Control char filter	*/
			cont = myform_keybd(tree,edit_obj,next_obj,kr,&next_obj,&kr);
			if (kr && edit_obj) 	/* Add others to object */
				objc_edit(tree,edit_obj,kr,&idx,EDCHAR);
		}

		if (which & MU_BUTTON) {	/* Which object hit?	*/
			mxx = mx;
			myy = my;
			mmstate = mb;
			nc = br;
			next_obj = objc_find(tree,ROOT,MAX_DEPTH,mx,my);
			if (next_obj == NIL) {
				next_obj = 0;
				ring_bell();
			} else {			/* Process a click		*/
				cont = scsi_form_button(tree,next_obj,br,
										&next_obj,&hot_obj,my,edit_obj,&idx);
			}
		}
		/*
		 * handle end of field clean up
		 */
		if (!cont || (next_obj != edit_obj && next_obj != 0))
			if (edit_obj != 0)
				objc_edit(tree,edit_obj,0,&idx,EDEND);
	}

	/*
	 * If defaulted, may need to clear hotspot
	 */
	if (hot_obj != (next_obj & 0x7fff))
		if (hot_obj != NIL)
			objc_toggle(tree,hot_obj);

	/*
	 * return exit object
	 * hi bit may be set if exit obj. was double-clicked
	 */
	*start_fld = edit_obj;

	return next_obj;
}


/******************************************/
/* Mouse button handler for scsi dialog   */
/******************************************/
PRIVATE short scsi_form_button(OBJECT *tree,WORD obj,WORD clicks,WORD *next_obj,WORD *hot_obj,short my,short edit_obj,short *idx)
{
WORD flags, state, hibit, texit, sble, dsbld, edit;
WORD in_state;
short x, y, mstate, kstate;

	flags = tree[obj].ob_flags;		/* Get flags and states   */
	state = tree[obj].ob_state;
	texit = flags & TOUCHEXIT;
	sble = flags & SELECTABLE;
	dsbld = state & DISABLED;
	edit = flags & EDITABLE;

	if (!texit && (!sble || dsbld) && !edit) {	/* This is not an interesting object */
		switch(obj) {
		case SUP:
			objc_edit(tree,edit_obj,0,idx,EDEND);
			do_up_one();
			objc_edit(tree,edit_obj,0,idx,EDINIT);
			break;
		case SDOWN:
			objc_edit(tree,edit_obj,0,idx,EDEND);
			do_down_one();
			objc_edit(tree,edit_obj,0,idx,EDINIT);
			break;
		case SBBAR:
			objc_offset(tree,STBAR,&x,&y);
			objc_edit(tree,edit_obj,0,idx,EDEND);
			if (my < y) {
				do_up_page();
			} else {
				do_down_page();
			}
			objc_edit(tree,edit_obj,0,idx,EDINIT);
			break;
		case STBAR:
			graf_mkstate(&x,&y,&mstate,&kstate);
			if (mstate & 0x01) {
				kstate = graf_slidebox(tree,SBBAR,STBAR,1);
				objc_edit(tree,edit_obj,0,idx,EDEND);
				do_random_page(kstate);
				objc_edit(tree,edit_obj,0,idx,EDINIT);
			}
			break;
		case ESTOTAL:
			if (keystate) {
				do_edisksize();
			}
			break;
		default:
			ring_bell();
			break;
		}
		*next_obj = 0;
		return TRUE;
	}

	if (texit && clicks == 2)	/* Preset special flag	*/
		hibit = 0x8000;
	else
		hibit = 0x0000;

	if (sble && !dsbld) {		/* Hot stuff!			*/
		if (flags & RBUTTON)		/* Process radio buttons */
			do_radio(tree,obj); 	/* immediately! 		*/
		else if (!texit) {
			in_state = (obj == *hot_obj) ?		/* Already toggled ? */
				state : state ^ SELECTED;
			if (!graf_watchbox(tree,obj,in_state,in_state^SELECTED)) {	/* He gave up...  */
				*next_obj = 0;
				*hot_obj = NIL;
				return TRUE;
			}
		} else
		 /* if (texit) */ if (obj != *hot_obj)	/* Force SELECTED		*/
			objc_toggle(tree,obj);
	}
	if (obj == *hot_obj)		/* We're gonna do it! So don't	*/
		*hot_obj = NIL; 		/* turn it off later.			*/

	if (texit || (flags & EXIT)) {		/* Exit conditions. 			*/
		*next_obj = obj | hibit;
		return FALSE; 			/* Time to leave!				*/
	} else if (!edit)			/* Clear object unless tabbing	*/
		*next_obj = 0;

	return TRUE;
}


/*****************************************************************************/
/************* New scsi forms manager: Entry point and main loop *************/
/*****************************************************************************/
PUBLIC int edit_form_do(OBJECT *tree,short start_fld)
{
REG WORD edit_obj;
WORD next_obj, hot_obj, hot_mode;
WORD which, cont;
WORD idx;
WORD mx, my, mb, ks, kr, br;
GRECT hot_rect;
TEDINFO *temp;
short i, len;

	/* Init. editing		*/

	next_obj = start_fld;
	edit_obj = 0;
	/* Initial hotspot cndx */
	hot_obj = NIL;
	hot_mode = M1_ENTER;
	objc_xywh((OBJECT *) tree,ROOT,&hot_rect);

	/* Main event loop		*/
	cont = TRUE;
	while (cont) {
		/* position cursor on	*/
		/* the selected 	  */
		/* editting field	  */
		if (edit_obj != next_obj)
			if (next_obj != 0) {
				edit_obj = next_obj;
				next_obj = 0;
				objc_edit((OBJECT *) tree,edit_obj,0,&idx,EDINIT);
				objc_edit((OBJECT *) tree,edit_obj,0,&idx,EDEND);
				objc_edit((OBJECT *) tree,edit_obj,0,&idx,EDINIT);
			}
		/* wait for button or	*/
		/* key or rectangle 	*/
		which = evnt_multi(MU_KEYBD | MU_BUTTON,
							2, LEFT_BUTTON, BUTTON_DOWN,
							0, 0, 0, 0, 0,
							0, 0, 0, 0, 0,
							NULL, 0, 0,
							&mx, &my, &mb, &ks, &kr, &br);

		if (which & MU_KEYBD) { /* Control char filter	*/
			cont = myform_keybd((OBJECT *) tree,edit_obj,next_obj,kr,&next_obj,&kr);
			if (kr && edit_obj) /* Add others to object */
				if ((kr == DOT1) || (kr == DOT2)) {
					temp = (TEDINFO *) ((OBJECT *) tree + edit_obj)->ob_spec;
					len = strlen(temp->te_ptext);
					for (i = len; i < 8; i++) {
						objc_edit((OBJECT *) tree,edit_obj,SPACE,&idx,EDCHAR);
					}
				} else {
					objc_edit((OBJECT *) tree,edit_obj,kr,&idx,EDCHAR);
				}
		}
		/* handle button event	*/
		if (which & MU_BUTTON) {/* Which object hit?	*/
			mxx = mx;
			myy = my;
			mmstate = mb;
			nc = br;
			next_obj = objc_find((OBJECT *) tree,ROOT,MAX_DEPTH,mx,my);
			if (next_obj == NIL)
				next_obj = 0;
			else				/* Process a click		*/
				cont = myform_button(tree,next_obj,br,&next_obj,&hot_obj,my,edit_obj,&idx,ks);
		}
		/* handle end of field	*/
		/* clean up 		  */
		if (!cont || (next_obj != edit_obj && next_obj != 0))
			if (edit_obj != 0)
				objc_edit((OBJECT *) tree,edit_obj,0,&idx,EDEND);
	}
	/* If defaulted, may	*/
	/* need to clear hotspot */
	if (hot_obj != (next_obj & 0x7fff))
		if (hot_obj != NIL)
			objc_toggle((OBJECT *) tree,hot_obj);
	/* return exit object	*/
	/* hi bit may be set  */
	/* if exit obj. was   */
	/* double-clicked	  */
	start_fld = edit_obj;

	return next_obj;
}

/****************************************************/
/* convert x,y,w,h to upr lt x,y and lwr rt x,y 	*/
/****************************************************/
PUBLIC void grect_to_array(GRECT *area,WORD *array)
{
	*array++ = area->g_x;
	*array++ = area->g_y;
	*array++ = area->g_x + area->g_w - 1;
	*array = area->g_y + area->g_h - 1;
}

/*********************************************************/
PUBLIC void save_screen(void)
{
short xy[8];
int was_hidden;

	if (savescreen.fd_addr == NULL) {		/* shouldn't happen, but ... */
		if ((savescreen.fd_addr=malloc(screen_in_bytes)) == NULL) {
			error(screen_in_bytes,MEMALLOC,"");
			return;
		}
	}

	xy[0] = xy[4] = 0;
	xy[1] = xy[5] = 0;
	xy[2] = xy[6] = Xw;
	xy[3] = xy[7] = Yh;
	was_hidden = mouse_off();
	vro_cpyfm(myhandle,S_ONLY,xy,&screen,&savescreen);
	if (!was_hidden)
		mouse_on();
}

PUBLIC void restore_screen(void)
{
short xy[8];
int was_hidden;

	if (savescreen.fd_addr == NULL)
		return;

	xy[0] = xy[4] = 0;
	xy[1] = xy[5] = 0;
	xy[2] = xy[6] = Xw;
	xy[3] = xy[7] = Yh;
	was_hidden = mouse_off();
	vro_cpyfm(myhandle,S_ONLY,xy,&savescreen,&screen);
	if (!was_hidden)
		mouse_on();

	free(savescreen.fd_addr);
	savescreen.fd_addr = NULL;
}

/*********************************************************/
PUBLIC void save_map(void)
{
short xy[8];
int was_hidden;

	if (savemap.fd_addr == NULL) {
		if ((savemap.fd_addr=malloc(map_in_bytes)) == NULL) {
			error(map_in_bytes,MEMALLOC,"");
			return;
		}
	}

	xy[0] = gmap.g_x;
	xy[1] = gmap.g_y;
	xy[2] = gmap.g_x + gmap.g_w - 1;
	xy[3] = gmap.g_y + gmap.g_h - 1;
	xy[4] = 0;
	xy[5] = 0;
	xy[6] = gmap.g_w - 1;
	xy[7] = gmap.g_h - 1;
	was_hidden = mouse_off();
	vro_cpyfm(myhandle,S_ONLY,xy,&screen,&savemap);
	if (!was_hidden)
		mouse_on();
}

PUBLIC void restore_map(GRECT rect)
{
GRECT r2;
short xy[8];
int was_hidden;

	if (savemap.fd_addr == NULL)
		return;

	r2 = gmap;

	set_clip();
	if (rc_intersect(&rect,&r2)) {
		xy[0] = r2.g_x - gmap.g_x;
		xy[1] = r2.g_y - gmap.g_y;
		xy[2] = xy[0] + r2.g_w - 1;
		xy[3] = xy[1] + r2.g_h - 1;

		xy[4] = r2.g_x;
		xy[5] = r2.g_y;
		xy[6] = r2.g_x + r2.g_w - 1;
		xy[7] = r2.g_y + r2.g_h - 1;;
		was_hidden = mouse_off();
		vro_cpyfm(myhandle,S_ONLY,xy,&savemap,&screen);
		if (!was_hidden)
			mouse_on();

		/*
		 * Bconin(2); printf("gmap x= %d y= %d w= %d h= %d\n",gmap.g_x,gmap.g_y,gmap.g_w,gmap.g_h);
		 * printf("rect x= %d y= %d w= %d h= %d\n",rect.g_x,rect.g_y,rect.g_w,rect.g_h); printf("r2
		 * x= %d y= %d w= %d h= %d\n",r2.g_x,r2.g_y,r2.g_w,r2.g_h); printf("source x= %d y= %d w=
		 * %d h= %d\n",xy[0],xy[1],xy[2],xy[3]); printf("dest x= %d y= %d w= %d h=
		 * %d\n",xy[4],xy[5],xy[6],xy[7]); fflush(stdout); Bconin(2);
		 */
	}
}

/***************************/
PUBLIC void set_clip()
{
short xoff;

	pxyarray[0] = workx;
	pxyarray[1] = worky;
	pxyarray[2] = workx + workw - 1;
	pxyarray[3] = worky + workh - 1;
	vs_clip(myhandle,TRUE,pxyarray);
	vst_color(myhandle,BLACK);
	vst_height(myhandle,ychar-rez-1,&xoff,&xoff,&xoff,&xoff);
}

/***************************/
PUBLIC void new_icon()
{
short obj;
CICONBLK *icon;

	if (dev < 2) {
		bottom[BACTIVEF].ob_flags &= ~HIDETREE;
		bottom[BACTIVEH].ob_flags |= HIDETREE;
		obj = BACTIVEF;
	} else {
		bottom[BACTIVEF].ob_flags |= HIDETREE;
		bottom[BACTIVEH].ob_flags &= ~HIDETREE;
		obj = BACTIVEH;
	}

	icon = geticon(bottom,obj);
	icon->monoblk.ib_char = 0x1000 | (dev + 'A');	//MAGNUM

	bottom[obj].ob_state &= ~SELECTED;
	redraw_xobject(bottom,obj);
}

/***************************/
PRIVATE void icon_medy(OBJECT *tree,short idex,short amount)
{
CICONBLK *icon;

	icon = geticon(tree,idex);
	icon->monoblk.ib_ychar -= amount;
}

/***************************/
PUBLIC void toggle_bottom(short obj)
{
int i;

	for (i = BDINFO; i <= BPRINT; i++)
		objc_xdsel(bottom,i);

	objc_xsel(bottom,obj);
}

/*****************************************************/
PUBLIC void myform_center(OBJECT *tree)
{
	tree->ob_x = workx + (workw - tree->ob_width) / 2;
	tree->ob_y = worky + (workh - tree->ob_height) / 2;
}
