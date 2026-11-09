/*
 *	part1.c
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

//NOTE: at the moment, a Load of SCSI info overwrites the original data in orgbuf,
// which may have been loaded from the hard disk. this means that a subsequent Reset
// will restore the Loaded info rather than the hard disk info: this differs from
// the documentation.  we may choose to live with this, or we could retain the
// original info specifically for a Reset.  with the current design, this would
// require the allocation of another 32K of memory; therefore it's probably best
// to leave it until we reimplement the save/load file format.
// suggested new format of DAT file:
//		bytes 0-511		root sector image: from this we can deduce whether it's
//						ICD/Atari and XGM/non-XGM
//		bytes 512-		copy of PART[] array from SCSIINFO, i.e. info for 64 partitions
//
// the following changes are required at the same time:
//		load_root_sectors()	needs to do decoding (move logic from load_parts_orgbuf())
//		save_parts()		write new data (as above)
//		write_scsi_info()	create sector images 'on the fly' from SCSIINFO (move logic from load_newbuf())
//		read_info_file()	[info.c] modify appropriately
//
// this is quite messy ...
//

/*
 *	version 2.10 (august/2002)	roger burrows
 *		a highly cleaned up and reorganised version of
 *		the DE v2.04 source; no new function
 *
 *	version 2.53 (july/2003)	roger burrows
 *		fix bug in display of total/available space in MB, caused by
 *		arithmetic overflow when the size exceeded 2GB.  fixed by
 *		using secs_to_cmegs() routine to perform conversion
 *		
 *	version 2.54 (augudt/2003)	roger burrows
 *		. add DEMO support
 *
 *	version 2.57 (january/2017)	roger burrows
 *		prepare for public release
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <ctype.h>
#include <dos.h>
#include <portab.h>
#include <string.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"
#include "lib/driver.h"

#define MODULE_NUMBER	11		/* used by error() macro */


typedef struct {				/* structure to keep track of partitions */
	char *orgbuf;
	int orgnsec0;
	long size;
	long nbad;
	long wherebad;
	char *newbuf;
	long numrootsec;				/* number of entries in secnum[] */
	long secnum[MAX_PARTITIONS-2];	/* sector numbers for root + extended root sectors */
	PART part[MAX_PARTITIONS];
} SCSIINFO;


extern short dev;
extern short nplanes, win1;
extern short workx, worky, workw, workh;
extern short MagiC;
extern OPT opt;
extern STATUS status;
extern OBJECT *bone, *newsize, *printstr, *rebone, *scsiedit, *selscsi, *split, *text;



/*
 *	private variables 
 */
#define GEM partypes[0]
#define BGM partypes[1]
#define XGM partypes[2]
MLOCAL char *partypes[] = { "GEM", "BGM", "XGM" };

MLOCAL char *mode[] = { "ATARI", " ICD " };	/* partitioning mode */
#define PARTMODE_ATARI	0
#define PARTMODE_ICD	1
MLOCAL int partmode = PARTMODE_ATARI;		/* Atari or ICD/Supra partitioning */

#define SECTORS			0					/* size display mode */
#define MEGABYTES		1
MLOCAL short sizemode = SECTORS;			/* sizes in sectors or MB */

#define TOSVER_10	0x0100					/* TOS versions */
#define TOSVER_12	0x0102
#define TOSVER_14	0x0104
#define TOSVER_36	0x0306
#define TOSVER_40	0x0400
MLOCAL short rommode = TOSVER_10;

#define PSECTORS	0						/* editing mode */
#define PSIZE		1
#define PTYPE		2
MLOCAL short editmode = PSECTORS;

MLOCAL short force_atari_mode = FALSE;		/* TRUE => XGM partition found in partitions 0-3 (set by load_parts_orgbuf()) */
MLOCAL short force_newbuf = FALSE;
MLOCAL short need_to_reboot = FALSE;

MLOCAL short toppart, npart, barrange, bootpart;

MLOCAL long orgsize = 0L;					/* original size of disk in sectors */

MLOCAL long secavail = 0L;

MLOCAL SCSIINFO scsi;

MLOCAL short scsiob[DISPLAY_PARTITIONS]
	 = {ESTART1, ESTART2, ESTART3, ESTART4, ESTART5, ESTART6,
	    ESTART7, ESTART8, ESTART9, ESTART10, ESTART11, ESTART12};

MLOCAL char *sectmplt[] = {				/* used for alternate 'size' displays */
	"_______",
	"9999999"
};
MLOCAL char *megtmplt[] = {
	" ____.__",
	"999999"
};

MLOCAL XGMINFO xgminfo;


/*
 *	useful (?) private macros
 */
#define part(a,i)		(PART *)((a)+PART_OFFSET+i*sizeof(PART))
#define disksize(a)		*((long *)((a)+0x1c2L))
#define nbadsec(a)		*((long *)((a)+0x1faL))
#define wherebadsec(a)	*((long *)((a)+0x1f6L))
#define secpt(a)		*((a)+0x1c1L)
#define nheads(a)		*((a)+0x1b8L)

#define isactive(i)		((i)->exists & 0x01)
#define makeactive(i)	((i)->exists |= 0x01)
#define makeinactive(i)	((i)->exists &= ~0x01)
#define isbootable(i)	((i)->exists & 0x80)
#define makebootable(i)	((i)->exists |= 0x80)
#define makeunbootable(i) ((i)->exists &= ~0x80)


/*
 *	function prototypes
 */
PRIVATE void adjust_tos_ptype(PART *part);
PRIVATE int build_xgminfo(void);
PRIVATE long calc_pstart(int numparts);
PRIVATE long cmegs_to_secs(long cmegs);
PRIVATE void complete_part_table(long pstart);
PRIVATE void convert_atari_to_icd(void);
PRIVATE void convert_icd_to_atari(void);
PRIVATE void convert_size(char *s,long size,int dot);
PRIVATE int do_max(void);
PRIVATE int do_split(void);
PRIVATE void edit_ptype(void);
PRIVATE void edit_sectors(void);
PRIVATE void edit_size(void);
PRIVATE int icd_xgm_count(int numparts);
PRIVATE int isgarbage(PART *pinfo);
PRIVATE int isxgm(PART *p);
PRIVATE int iszero(PART *p);
PRIVATE int is_valid_ptype(char *type);
PRIVATE int is_valid_tos_ptype(char *type);
PRIVATE void load_and_edit_root(short id,short lun);
PRIVATE void load_and_update(int num);
PRIVATE void load_newbuf(void);
PRIVATE void load_obj(short obnum,PART *p,short pnum);
PRIVATE void load_parts_orgbuf(void);
PRIVATE void load_scsiedit(int start);
PRIVATE void loadob_topart(void);
PRIVATE void oneboot(short id,short lun);
PRIVATE void print_parts(short id,short lun);
PRIVATE int rebuild(short id,short lun,PART *part,short all,short bootat);
PRIVATE void rebuild_all(short id,short lun);
PRIVATE void rebuild_one(short id,short lun);
PRIVATE void recalc(void);
PRIVATE void recolour_white(OBJECT *tree,short idex);
PRIVATE void rotate_tos_mode(void);
PRIVATE void save_parts(short id,short lun);
PRIVATE long secs_to_cmegs(long sectors);
PRIVATE void set_tos_ptype(PART *part);
PRIVATE void update_avail(void);
PRIVATE void update_total(void);
PRIVATE int write_scsi_info(short id,short lun);



/*********************************************************/
PUBLIC void do_part(void)
{
	strcpy(selscsi[SELSTOP].ob_spec,text[PART_SELECT].ob_spec);
	strcpy(selscsi[SELSGO].ob_spec,text[PART_BUTTON].ob_spec);
	handle_scsiinfo_dialog(load_and_edit_root);
}

/*
 *	read in the root sector(s) from the device, and edit them
 */
PRIVATE void load_and_edit_root(short id,short lun)
{
char *rootbuf;
long size;

	if ((rootbuf=calloc(1,ROOTSEC_BUFSIZE)) == NULL) {
		error(ROOTSEC_BUFSIZE,MEMALLOC,"");
		return;
	}
	size = load_root_sectors(id,lun,rootbuf);
	if (size > 0L)
		do_scsi_edit(id,lun,rootbuf,size);
	else xform_alert(1,PT_ROOTSEC_ALERT);

	free(rootbuf);
}

/*
 *	read in and store the root sector, plus any extended root sectors
 *
 *	if there are any extended root sectors, initialise the structure 'xgminfo'
 *	with the starting sector number of the XGM partition
 *
 *	returns	-1 i/o error
 *			otherwise the total number of bytes read
 *			(i.e. PHYSICAL_SECSIZE * # sectors stored)
 */
PUBLIC long load_root_sectors(short id,short lun,char *buf)
{
long offset = 0L;
PART *xgm;

	memset(&xgminfo,0x00,sizeof(xgminfo));

	if (phys_read(id,lun,1,0L,buf))
		return -1L;

	while((xgm=find_xgm(buf+offset)) != NULL) {
		offset += PHYSICAL_SECSIZE;
		if (phys_read(id,lun,1,xgm->firstsector+xgminfo.rootsec,buf+offset))
			return -1L;
		if (!xgminfo.exists) {
			xgminfo.exists = 1;
			xgminfo.rootsec = xgm->firstsector;
		}
	}
	offset += PHYSICAL_SECSIZE;

	return offset;
}

/*******************************************************************************************/

/**************************************************************************/
PUBLIC void do_scsi_edit(short id,short lun,char *buf,long bufsize)
{
short exitobj, txt, pnum;
int j, laststat, done = 0;
PART *p;
TEDINFO *tted;
long n;
static char info[MAX_DISPLAYSIZE];

	memset(&scsi,0x00,sizeof(scsi));
	memset(&xgminfo,0x00,sizeof(xgminfo));

	if ((scsi.newbuf=calloc(1,ROOTSEC_BUFSIZE)) == NULL) {
		error(ROOTSEC_BUFSIZE,MEMALLOC,"");
		return;
	}
	scsi.orgbuf = buf;
	scsi.orgnsec0 = bufsize / PHYSICAL_SECSIZE;

	txt = ESIZE1;
	editmode = PSIZE;
	laststat = status.action;
	change_mode(ESCSI);
	edit_size();

	orgsize = 0L;
	if (get_capacity(id,lun,&n) == 0)
		orgsize = scsi.size = n;
	if (orgsize == 0L) {
		scsi.size = disksize(scsi.orgbuf);		/* fix up part pointers */
		if ((scsi.size > MAX_PARTSIZE) || (scsi.size < 0L))
			scsi.size = 0L;
		orgsize = scsi.size;
	}

	strcpy(scsiedit[ESCANCEL].ob_spec,text[PART_CANCEL].ob_spec);
	load_parts_orgbuf();
	load_scsiedit(0);
	wind_set(win1,WF_NAME,ADDR(text[PART_TITLE].ob_spec));
	sprintf(info,text[PART_INFO].ob_spec,id,(id<8)?lun:0);
	wind_set(win1,WF_INFO,ADDR(info));
	objc_draw(scsiedit,0,MAX_DEPTH,workx,worky,workw,workh);
	wind_update(BEG_UPDATE);
	wind_update(BEG_MCTRL);

	while(!done) {
		exitobj = scsi_form_do(scsiedit,&txt) & 0x7fff;
		if ((exitobj != ESSECTOR) && (exitobj != ESMEG))
			objc_xdsel(scsiedit,exitobj);

		switch(exitobj) {
		case ESSPLIT:
			if (do_split() == 0)
				load_and_update(0);
			break;

		case ESMAX:
			if (do_max() == 0)
				load_and_update(0);
			break;

		case ESCLEAR:
			memset(&scsi.part[0],0x00,MAX_PARTITIONS*sizeof(PART));
			memset(&xgminfo,0x00,sizeof(xgminfo));
			npart = toppart = 0;
			update_avail();
			load_and_update(0);
			break;

		case ESRESET:
			memset(&scsi.part[0],0x00,MAX_PARTITIONS*sizeof(PART));
			memset(&xgminfo,0x00,sizeof(xgminfo));
			scsi.orgnsec0 = bufsize / PHYSICAL_SECSIZE;
			load_parts_orgbuf();
			redraw_object(scsiedit,ESMODE);
			redraw_object(scsiedit,ESTOS);
			load_and_update(0);
			break;

		case ESMODE:
			if (partmode == PARTMODE_ATARI) {
				if (force_atari_mode)
					if (xform_alert(2,PT_ICDWARN_ALERT) == 2)
						break;
				convert_atari_to_icd();
			} else convert_icd_to_atari();
			redraw_object(scsiedit,ESMODE);
			update_avail();
			load_and_update(toppart);
			break;

		case ESTOS:
			rotate_tos_mode();
			redraw_object(scsiedit,ESTOS);
			load_and_update(toppart);
			break;

		case ESSECTOR:
			for (j = 0; j < DISPLAY_PARTITIONS; j++) {
				tted = (TEDINFO *) (scsiedit[scsiob[j]+2].ob_spec);
				strcpy(tted->te_ptmplt,sectmplt[0]);
				strcpy(tted->te_pvalid,sectmplt[1]);
			}
			sizemode = SECTORS;
			update_avail();
			update_total();
			load_and_update(toppart);
			redraw_object(scsiedit,ESTOTAL);
			break;

		case ESMEG:
			for (j = 0; j < DISPLAY_PARTITIONS; j++) {
				tted = (TEDINFO *) (scsiedit[scsiob[j]+2].ob_spec);
				strcpy(tted->te_ptmplt,megtmplt[0]);
				strcpy(tted->te_pvalid,megtmplt[1]);
			}
			sizemode = MEGABYTES;
			update_avail();
			update_total();
			load_and_update(toppart);
			redraw_object(scsiedit,ESTOTAL);
			break;

		case ESRECALC:
			loadob_topart();
			recalc();
			update_avail();
			if (secavail < 0)
				xform_alert(1,PT_SPCWARN_ALERT);
			load_and_update(toppart);
			break;

		case ESLOAD:
			n = read_info_file(buf,ROOTSEC_BUFSIZE);
			if (n < 0L)
				break;
			bufsize = n;	/* necessary since we have modified the data in scsi.orgbuf */
			memset(&scsi.part[0],0x00,MAX_PARTITIONS*sizeof(PART));
			memset(&xgminfo,0x00,sizeof(xgminfo));
			scsi.orgnsec0 = bufsize / PHYSICAL_SECSIZE;
			load_parts_orgbuf();
			redraw_object(scsiedit,ESMODE);
			redraw_object(scsiedit,ESTOS);
			load_and_update(0);
			break;

		case ESPRINT:
			loadob_topart();
			load_and_update(toppart);
			if (secavail < 0) {
				xform_alert(1,PT_SPCERR_ALERT);
				break;
			}
			print_parts(id,lun);
			break;

		case ESSAVE:
			loadob_topart();
			load_and_update(toppart);
			if (secavail < 0) {
				xform_alert(1,PT_SPCERR_ALERT);
				break;
			}
			save_parts(id,lun);
			break;

		case ESCANCEL:
			done = 1;
			break;

		case PARTONLY:
			loadob_topart();
			load_and_update(toppart);
			if (secavail < 0) {
				xform_alert(1,PT_SPCERR_ALERT);
				break;
			}
			if (opt.warn || opt.novice)
				if (xform_alert(2,PT_INSPART_ALERT) == 2)
					break;
			if (write_scsi_info(id,lun) == 0) {
				strcpy(scsiedit[ESCANCEL].ob_spec,text[PART_EXIT].ob_spec);
				redraw_object(scsiedit,ESCANCEL);
				need_to_reboot = TRUE;
			}
			break;

		case PARTNREB:
			loadob_topart();
			load_and_update(toppart);
			if (secavail < 0) {
				xform_alert(1,PT_SPCERR_ALERT);
				break;
			}
			if (opt.warn || opt.novice)
				if (xform_alert(2,PT_REBUILD_ALERT) == 2)
					break;
			rebuild_all(id,lun);
			break;

		case REBUILD1:
			loadob_topart();
			load_and_update(toppart);
			if (secavail < 0) {
				xform_alert(1,PT_SPCERR_ALERT);
				break;
			}
			if (opt.warn || opt.novice)
				if (xform_alert(2,PT_REBPART_ALERT) == 2)
					break;
			rebuild_one(id,lun);
			break;

		case ONEBOOT:
			loadob_topart();
			load_and_update(toppart);
			if (secavail < 0) {
				xform_alert(1,PT_SPCERR_ALERT);
				break;
			}
			oneboot(id,lun);
			break;

		case ESTART1:
		case ESTART2:
		case ESTART3:
		case ESTART4:
		case ESTART5:
		case ESTART6:
		case ESTART7:
		case ESTART8:
		case ESTART9:
		case ESTART10:
		case ESTART11:
		case ESTART12:
		case EEND1:
		case EEND2:
		case EEND3:
		case EEND4:
		case EEND5:
		case EEND6:
		case EEND7:
		case EEND8:
		case EEND9:
		case EEND10:
		case EEND11:
		case EEND12:
			loadob_topart();
			load_and_update(toppart);
			edit_sectors();
			redraw_object(scsiedit,EEDITMOD);
			txt = exitobj;
			break;

		case ESIZE1:
		case ESIZE2:
		case ESIZE3:
		case ESIZE4:
		case ESIZE5:
		case ESIZE6:
		case ESIZE7:
		case ESIZE8:
		case ESIZE9:
		case ESIZE10:
		case ESIZE11:
		case ESIZE12:
			loadob_topart();
			load_and_update(toppart);
			edit_size();
			redraw_object(scsiedit,EEDITMOD);
			txt = exitobj;
			break;

		case ETYPE1:
		case ETYPE2:
		case ETYPE3:
		case ETYPE4:
		case ETYPE5:
		case ETYPE6:
		case ETYPE7:
		case ETYPE8:
		case ETYPE9:
		case ETYPE10:
		case ETYPE11:
		case ETYPE12:
			loadob_topart();
			load_and_update(toppart);
			edit_ptype();
			redraw_object(scsiedit,EEDITMOD);
			txt = exitobj;
			break;

		case EON1:
		case EON2:
		case EON3:
		case EON4:
		case EON5:
		case EON6:
		case EON7:
		case EON8:
		case EON9:
		case EON10:
		case EON11:
		case EON12:
			pnum = toppart + (exitobj - EON1) / (EON2 - EON1);
			p = &scsi.part[pnum];
			if (!iszero(p)) {
				if (isactive(p)) {
					makeinactive(p);
					strcpy(scsiedit[exitobj].ob_spec,"_");
				} else {
					makeactive(p);
					sprintf(scsiedit[exitobj].ob_spec,"%c",CHECK_MARK);
				}
				redraw_object(scsiedit,exitobj);
			}
			break;

		case EBOOT1:
		case EBOOT2:
		case EBOOT3:
		case EBOOT4:
		case EBOOT6:
		case EBOOT5:
		case EBOOT7:
		case EBOOT8:
		case EBOOT9:
		case EBOOT10:
		case EBOOT11:
		case EBOOT12:
			pnum = toppart + (exitobj - EBOOT1) / (EBOOT2 - EBOOT1);
			p = &scsi.part[pnum];
			if (!iszero(p)) {
				if (isbootable(p)) {
					makeunbootable(p);
					strcpy(scsiedit[exitobj].ob_spec,"_");
					bootpart = -1;
				} else {
					if ((bootpart >= 0) && (bootpart != pnum)) {/* if there's already a different boot partition */
						xform_alert(1,PT_BOOTCNT_ALERT);		/* disallow this one */
						break;
					}
					if (xgminfo.flag[pnum]) {					/* if this is in an XGM partition */
						xform_alert(1,PT_BOOTXGM_ALERT);		/* disallow it */
						break;
					}
					makebootable(p);
					sprintf(scsiedit[exitobj].ob_spec,"%c",CHECK_MARK);
					bootpart = pnum;
				}
				redraw_object(scsiedit,exitobj);
			}
			break;

		}
	}
	wind_update(END_MCTRL);
	wind_update(END_UPDATE);
	if (need_to_reboot) {
		xform_alert(1,PT_REBOOT_ALERT);
		cold_boot();
	}
	change_mode(laststat);

	refresh_screen(scsiedit);
	free(scsi.newbuf);
	scsi.newbuf = NULL;
}

/**************************************************************************/
PRIVATE void load_parts_orgbuf()
{
int i, j, k, l;
PART *pp;
long offset = 0L;
long xgmoff = 0L;

	k = 0;
	partmode = PARTMODE_ATARI;
	scsi.nbad = nbadsec(scsi.orgbuf);
	if (scsi.nbad > scsi.size)
		scsi.nbad = 0;
	scsi.wherebad = wherebadsec(scsi.orgbuf);
	if ((scsi.nbad > scsi.size) || (scsi.nbad < 0)) {
		scsi.nbad = 0;
		scsi.wherebad = 0;
	}
	force_atari_mode = FALSE;
	force_newbuf = FALSE;
	for (j = 0; j < MAX_PARTS_ICD; j++) {
		pp = part(scsi.orgbuf,j);
		if (iszero(pp))
			break;

		if (isgarbage(pp)) {
			if (j < MAX_PARTS_ATARI) {			/* partition info in the common area is screwed up ... */
				force_newbuf = TRUE;
//				if (scsi.size > 3000000L)					//MAGNUM
//					scsi.size = orgsize = 0L;
			}
			break;
		}

		if (isxgm(pp)) {
			xgminfo.exists = 1;
			xgminfo.rootsec = pp->firstsector;
			if (j < MAX_PARTS_ATARI)		/* we have an XGM partition within the Atari area */
				force_atari_mode = TRUE;
			scsiedit[ESCHAIN].ob_flags &= ~HIDETREE;
			for (i = 1; i < scsi.orgnsec0; i++) {
				offset += PHYSICAL_SECSIZE;
				for (l = 0; l < MAX_PARTS_ICD; l++) {
					pp = part(scsi.orgbuf+offset,l);
					if (iszero(pp))
						break;
					if (isxgm(pp)) {
						xgmoff = pp->firstsector;
						break;
					}
					memcpy(&scsi.part[k],pp,sizeof(PART));
					xgminfo.flag[k] = 1;
					scsi.part[k++].firstsector += (xgminfo.rootsec + xgmoff);
				}

			}
			if (j < 11)				//MAGNUM		//this doesn't make sense, we tested earlier for a different value
				force_atari_mode = TRUE;			//we need to decide what to do if the XGM is in e.g. slot 9
		} else {
			strcpy(scsiedit[ESTAR1+k].ob_spec,"");
			memcpy(&scsi.part[k++],pp,sizeof(PART));
			if (j >= MAX_PARTS_ATARI)
				partmode = PARTMODE_ICD;
		}
	}

	offset = 0L;		//rfb: we calculate an 'offset' here but never use it ...

	if (force_newbuf) {
		if (partmode == PARTMODE_ATARI) {
			scsi.nbad = 1;
			scsi.wherebad = 1;
		} else {
			scsi.nbad = 0;
			scsi.wherebad = 0;
		}
	}
	for (j = 0, pp = &scsi.part[0]; j < k; j++, pp++) {
		if (isactive(pp))
			offset += pp->size;
	}
	npart = k;
	toppart = 0;
	offset += scsi.nbad;
	offset += scsi.orgnsec0;
	update_total();
	update_avail();
	sprintf(scsiedit[ESMODE].ob_spec,"%s",mode[partmode]);

	/*
	 *	set default TOS version to highest supported
	 */
	if ((_tos > TOSVER_36) || MagiC)
		rommode = TOSVER_40;
	else if (_tos > TOSVER_12)
		rommode = TOSVER_14;
	else rommode = TOSVER_10;

	sprintf(scsiedit[ESTOS].ob_spec,"TOS %1d.%02d",(rommode>>8)&0xff,rommode&0xff);
}

/*
 *	copy scsi.part[] info to dialog, starting at specified partition, and display
 */
PRIVATE void load_and_update(int num)
{
	load_scsiedit(num);
	redraw_object(scsiedit,SBDROP);
	redraw_object(scsiedit,SBBAR);
	redraw_object(scsiedit,ESAVAIL);
}

/*
 *	copy scsi.part[] info to dialog, starting at specified partition
 */
PRIVATE void load_scsiedit(int start)
{
int j;
PART *p;

	toppart = start;
	for (j = 0, p = &scsi.part[start]; j < DISPLAY_PARTITIONS; j++, p++) {
		sprintf(scsiedit[TOPNUM+j].ob_spec,"%2d",toppart+j+1);
		load_obj(scsiob[j],p,start+j);
	}

	scsiedit[STBAR].ob_y = toppart * barrange / MAX_TOPPART;
}

/*
 *	copy partition info for one partition to dialog
 */
PRIVATE void load_obj(short obnum,PART *p,short pnum)
{
int i;
TEDINFO *temp;
char type[4];
long end;

	bootpart = -1;
	if (iszero(p)) {
		for (i = 0; i < 4; i++) {							//MAGNUM
			temp = (TEDINFO *) (scsiedit[obnum+i].ob_spec);
			strcpy(temp->te_ptext,"");
		}
		strcpy(scsiedit[obnum+4].ob_spec,"");
		strcpy(scsiedit[obnum+5].ob_spec,"_");
		strcpy(scsiedit[obnum+6].ob_spec,"_");
		return;
	}

	temp = scsiedit[obnum].ob_spec;
	sprintf(temp->te_ptext,"%7ld",p->firstsector);

	temp = scsiedit[obnum+1].ob_spec;
	end = p->firstsector + p->size - 1;
	sprintf(temp->te_ptext,"%7ld",end);

	temp = scsiedit[obnum+2].ob_spec;
	convert_size(temp->te_ptext,p->size,FALSE);

	temp = scsiedit[obnum+3].ob_spec;
	memcpy(type,p->type,3);
	type[3] = '\0';
	sprintf(temp->te_ptext,"%s",type);

	strcpy(scsiedit[obnum+4].ob_spec,xgminfo.flag[pnum]?"*":"");
	sprintf(scsiedit[obnum+5].ob_spec,"%c",isactive(p)?CHECK_MARK:'_');

	if (isbootable(p)) {
		bootpart = pnum;
		sprintf(scsiedit[obnum+6].ob_spec,"%c",CHECK_MARK);
	} else strcpy(scsiedit[obnum+6].ob_spec,"_");
}

/**************************************************************************/
PRIVATE int isgarbage(PART *pinfo)
{
long max_phys_secs;

	if (pinfo->exists&~0x81)
		return 1;
	if (!is_valid_ptype(pinfo->type))
		return 1;
	if ((pinfo->firstsector < 0) || (pinfo->firstsector > scsi.size))
		return 1;

	if ((pinfo->size < 0) || (pinfo->size > scsi.size))
		return 1;

	/*
	 *	here we test for partition size: calculate the maximum number
	 *	of physical sectors allowed, depending on partition type
	 *
	 *	NOTE: to avoid rejecting valid values for some values of 'rommode',
	 *	we may accept some values that are invalid for the current 'rommode'.
	 */
	if (strncmp(pinfo->type,GEM,3) == 0)
		max_phys_secs = MAX_PHYS_SECS_GEM_TOS14;
	else if (strncmp(pinfo->type,BGM,3) == 0)
		max_phys_secs = MAX_PHYS_SECS_BGM_TOS4;
	else if (strncmp(pinfo->type,XGM,3) == 0)
		max_phys_secs = MAX_PARTS_ICD * MAX_PHYS_SECS_BGM_TOS4;
	else max_phys_secs = MAX_PARTSIZE;
	if (pinfo->size > max_phys_secs)
		return 1;

	return 0;
}

/**************************************************************************/
PRIVATE int iszero(PART *p)
{
static PART zp;	/* initialised to zeros by default */

	if (memcmp(&zp,p,sizeof(PART)) == 0)
		return 1;

	return 0;
}

/*
 *	test for XGM partition
 */
PRIVATE int isxgm(PART *p)
{
	if (strncmp(p->type,XGM,3) == 0)
		return 1;

	return 0;
}

/*
 *	search in-memory copy of buffer for XGM partition
 *
 *	return pointer to PART structure, or NULL if not found
 */
PUBLIC PART *find_xgm(char *buf)
{
int i;
PART *p;

	for (i = 1; i <= MAX_PARTS_ICD; i++) {
		p = part(buf,i);
		if (isxgm(p))
			return p;
	}

	return NULL;
}

/**************************************************************************/
PUBLIC void init_scsi_editobject()
{
int i, j;
TEDINFO *temp;
long junkp;

	barrange = scsiedit[SBBAR].ob_height - scsiedit[STBAR].ob_height;
	for (j = 0; j < DISPLAY_PARTITIONS; j++) {
		for (i = 0; i < 4; i++) {					//MAGNUM
			temp = (TEDINFO *) (scsiedit[scsiob[j]+i].ob_spec);
			strcpy(temp->te_ptext,"");
		}
	}

	if (nplanes == 1) {
		scsiedit[SBDROP].ob_spec = 0L;				/* set SBDROP box to: interior=WHITE/pattern=IP_HOLLOW/transparent/border = WHITE */
		junkp = (long)scsiedit[SBBAR].ob_spec;
		junkp |= 0x11L;								//MAGNUM
		scsiedit[SBBAR].ob_spec = (void *)junkp;
	}
	if (nplanes < 4) {
		recolour_white(scsiedit,PARTONLY);
		recolour_white(scsiedit,REBUILD1);
		recolour_white(scsiedit,PARTNREB);
		recolour_white(scsiedit,ONEBOOT);
	}
	sizemode = SECTORS;
}

PRIVATE void recolour_white(OBJECT *tree,short idex)
{
TEDINFO *ted;

	ted = tree[idex].ob_spec;
	ted->te_color &= ~0x0fL;	/* set interior colour to white */
}

/**************************************************************************/

/*
 *	convert (in-memory) ICD-format partition table to Atari-format
 */
PRIVATE void convert_icd_to_atari()
{
long pstart;

	partmode = PARTMODE_ATARI;
	memset(&xgminfo,0x00,sizeof(xgminfo));
	if (force_newbuf) {
		scsi.nbad = 1;
		scsi.wherebad = 1;
	}
	pstart = calc_pstart(npart);
	complete_part_table(pstart);

	strcpy(scsiedit[ESMODE].ob_spec,mode[PARTMODE_ATARI]);
}

/*
 *	convert (in-memory) Atari-format partition table to ICD-format
 */
PRIVATE void convert_atari_to_icd()
{
long pstart;

	partmode = PARTMODE_ICD;
	memset(&xgminfo,0x00,sizeof(xgminfo));
	if (force_newbuf) {
		scsi.nbad = 0;
		scsi.wherebad = 0;
	}
	pstart = calc_pstart(npart);
	complete_part_table(pstart);

	strcpy(scsiedit[ESMODE].ob_spec,mode[PARTMODE_ICD]);
}

/*
 *	calculate starting sector of first partition of disk, depending on:
 *		partitioning mode
 *		number of partitions
 *		size of bad sector list
 *
 *	for ICD-format disks, also initialises xgminfo if extended root sectors are required
 */
PRIVATE long calc_pstart(int numparts)
{
int i;
long pstart;

	if (partmode == PARTMODE_ICD) {
		i = icd_xgm_count(numparts);
		if (i > 0) {
			xgminfo.exists = 1;
			xgminfo.rootsec = 1 + ((scsi.wherebad == 1) ? scsi.nbad : 0);
		} else {
			xgminfo.exists = 0;
			xgminfo.rootsec = 0;
		}
	} else i = 0;

	pstart = 1 + i + ((scsi.wherebad == 1) ? scsi.nbad : 0);

	return pstart;
}

/*
 *	completes the partition table; if necessary, adjusts
 *	the size of the last partition to fit
 *
 *	assumes:
 *		global variable 'npart' is set correctly
 *		the part[].size fields have already been set correctly
 */
PRIVATE void complete_part_table(long pstart)
{
int i;
//long total;
PART *p;

#if 0
	/*
	 *	validate the sizes of the defined partitions
	 */
	for (i = 0, total = pstart, p = &scsi.part[0]; i < npart; i++, p++) {
		if (p->size < 1)
			return -1;
		total += p->size;
	}
	/*
	 *	ensure that every partition (except possibly the last) fits on the disk
	 *	(we adjust the last if necessary)
	 */
	if ((total-(p-1)->size) >= scsi.size)
		return -1;

	/*
	 *	paranoia: verify that the undefined partitions are really zero-size
	 */
	for ( ; i < MAX_PARTITIONS; i++, p++)
		if (p->size != 0)
			return -1;
#endif

	/*
	 *	now assign the partition starts (based on the mode),
	 *	set the partition types, and make them active
	 *	(we also make unbootable any XGM partitions defined as bootable)
	 */
	for (i = 0, p = &scsi.part[0]; i < npart; i++, p++) {
		if (partmode == PARTMODE_ATARI) {
			if ((i == MAX_PARTS_ATARI-1) && (npart > MAX_PARTS_ATARI)) {
				xgminfo.exists = 1;
				xgminfo.rootsec = pstart;
			}
			if (xgminfo.exists) {
				pstart++;
				xgminfo.flag[i] = 1;
			}
		} else {
			if ((xgminfo.exists) && (i >= MAX_PARTS_ICD-1))
				xgminfo.flag[i] = 1;
		}
		if (isbootable(p) && xgminfo.flag[i])	/* presumably icd->atari conversion */
			makeunbootable(p);
		makeactive(p);
		set_tos_ptype(p);
		p->firstsector = pstart;
		pstart += p->size;
	}

	/*
	 *	finally, adjust the last partition as necessary to fit
	 */
	(p-1)->size += scsi.size - pstart;	/* adjust size of last partition */

//	return 0;
}


/*
 *	returns the number of extended root sectors needed for ICD-format disk
 *
 *	#parts	#sectors	#parts in root/ext root sectors
 *	1-12		0				12
 *	13-23		1				11/12
 *	24-34		2				11/11/12
 *	etc
 */
PRIVATE int icd_xgm_count(int numparts)
{
	return (numparts-2)/(MAX_PARTS_ICD-1);
}

/*************************************************************************/
PUBLIC void do_up_one()
{
	if (toppart == 0)
		return;

	loadob_topart();

	toppart--;
	load_scsiedit(toppart);
	redraw_object(scsiedit,SBBAR);
	redraw_object(scsiedit,SBDROP);
}

/*************************************************************************/
PUBLIC void do_down_one()
{
	if (toppart == MAX_TOPPART)
		return;

	loadob_topart();

	toppart++;
	load_scsiedit(toppart);
	redraw_object(scsiedit,SBBAR);
	redraw_object(scsiedit,SBDROP);
}

/*************************************************************************/
PUBLIC void do_up_page()
{
	if (toppart == 0)
		return;

	loadob_topart();

	toppart = max(0,toppart-DISPLAY_PARTITIONS);
	load_scsiedit(toppart);
	redraw_object(scsiedit,SBBAR);
	redraw_object(scsiedit,SBDROP);
}

/*************************************************************************/
PUBLIC void do_down_page()
{
	if (toppart == MAX_TOPPART)
		return;

	loadob_topart();

	toppart = min(MAX_TOPPART,toppart+DISPLAY_PARTITIONS);
	load_scsiedit(toppart);
	redraw_object(scsiedit,SBBAR);
	redraw_object(scsiedit,SBDROP);
}

/*************************************************************************/
PUBLIC void do_random_page(short pos)
{
	loadob_topart();

	toppart = (pos * MAX_TOPPART) / 1000L;
	load_scsiedit(toppart);
	redraw_object(scsiedit,SBBAR);
	redraw_object(scsiedit,SBDROP);
}

/*************************************************************************/
PUBLIC void do_edisksize()
{
TEDINFO *dsize;
short txt, exitobj;
long tsize;

	txt = DISKSIZE;
	dsize = (TEDINFO *) newsize[DISKSIZE].ob_spec;

	while(TRUE) {
		sprintf(dsize->te_ptext,"%ld",scsi.size);
		exitobj = handle_dial(newsize,txt,0,0);

		if (exitobj == NSOK) {				/* if the user said OK, */
			tsize = atol(dsize->te_ptext);	/* check the size */
			if (!tsize) {					/* zero isn't acceptable */
				xform_alert(1,PT_NEWSIZE_ALERT);	/* issue a message */
				continue;							/* and retry */
			}
			force_newbuf = TRUE;			/* otherwise we believe her */
			orgsize = scsi.size = tsize;
			update_total();
			update_avail();
			redraw_object(scsiedit,ESAVAIL);
			redraw_object(scsiedit,ESTOTAL);
		}
		break;
	}
}

/*
 *	general error msg for scsi operations
 */
PUBLIC int scsi_op_failed(short id,short lun)
{
	xform_alert(1,PT_SCSIOP_ALERT,id,lun);
	return 1;
}

/*
 *	split disk into multiple equal-size partitions
 */
PRIVATE int do_split()
{
TEDINFO *numsplit;
PART *p;
short txt, exitobj;
int i, numpart;
long totsize, psize, pstart;

	if (scsi.size == 0) {
		xform_alert(1,PT_NEWSIZE_ALERT);	/* issue a message */
		return 1;
	}

	txt = SPLITOB;
	numsplit = (TEDINFO *) split[SPLITOB].ob_spec;
	strcpy(numsplit->te_ptext,"");

	exitobj = handle_dial(split,txt,0,0);

	if (exitobj != SPLITOK)
		return 1;

	numpart = atoi(numsplit->te_ptext);
	if ((numpart < 1) || (numpart > MAX_PARTITIONS)) {
		xform_alert(1,PT_NUMPART_ALERT,MAX_PARTITIONS);
		return 1;
	}
	npart = numpart;

	memset(&scsi.part[0],0x00,MAX_PARTITIONS*sizeof(PART));
	memset(&xgminfo,0x00,sizeof(xgminfo));

	pstart = calc_pstart(npart);
	totsize = scsi.size - pstart;
	psize = totsize / npart;			/* size of all except last */

	/*
	 *	initialise partition sizes in table
	 */
	for (i = 0, p = &scsi.part[0]; i < npart; i++, p++)
		p->size = psize;
	complete_part_table(pstart);

	update_avail();
	return 0;
}

/*
 *	split disk into maximum-size partitions, as many as will fit
 *
 *	if rommode is TOSVER_10, we generate GEM partitions (for backward compatibility)
 *	otherwise we generate BGM partitions (size depending on rommode)
 */
PRIVATE int do_max()
{
int i, numparts;
long totsize, msize, pstart;
PART *p;

	switch(rommode) {
	default:					/* i.e. TOSVER_10 */
		msize = MAX_PHYS_SECS_GEM_TOS10;
		break;
	case TOSVER_14:
		msize = MAX_PHYS_SECS_BGM_TOS14;
		break;
	case TOSVER_40:
		msize = MAX_PHYS_SECS_BGM_TOS4;
		break;
	}

	/*
	 *	we use scsi.size as a proxy for totsize when generating an initial
	 *	estimate of the number of partitions.  in very rare cases, this
	 *	could cause us to overestimate the actual #partitions by 1; this
	 *	*could* therefore overestimate pstart by 1, which *could* reduce
	 *	the number of parts actually allocated ... but in this case the
	 *	unallocated partition would only be one track long anyway ...
	 */
	memset(&xgminfo,0x00,sizeof(xgminfo));
	pstart = calc_pstart(min(MAX_PARTITIONS,scsi.size/msize));
	totsize = scsi.size - pstart;
	numparts = (totsize+msize-1) / msize;

	/*
	 *	for large disks, we need to ensure that we don't allocate more
	 *	partitions than we can handle.
	 */
	if (numparts > MAX_PARTITIONS) {
		xform_alert(1,PT_NUMPART_ALERT,MAX_PARTITIONS);
		return 1;
	}
	npart = numparts;

	memset(&scsi.part[0],0x00,MAX_PARTITIONS*sizeof(PART));
	memset(&xgminfo,0x00,sizeof(xgminfo));
	pstart = calc_pstart(npart);

	/*
	 *	initialise partition sizes in table
	 */
	for (i = 0, p = &scsi.part[0]; i < npart; i++, p++)
		p->size = msize;
	complete_part_table(pstart);

	update_avail();
	return 0;
}

/*********************************************************************/
PRIVATE void rotate_tos_mode(void)
{
int i;
PART *p;

	/*
	 *	rotate among possible modes
	 */
	switch(rommode) {
	case TOSVER_10:
		rommode = TOSVER_14;
		break;
	case TOSVER_14:
		rommode = TOSVER_40;
		break;
	default:				/* i.e. TOSVER_40 */
		rommode = TOSVER_10;
		break;
	}

	for (i = 0, p = &scsi.part[0]; i < npart; i++, p++)
		adjust_tos_ptype(p);		/* adjust type if necesssary */

	sprintf(scsiedit[ESTOS].ob_spec,"TOS %1d.%02d",(rommode>>8)&0xff,rommode&0xff);
}

/*
 *	update the 'sectors available' count
 */
PRIVATE void update_avail()
{
int i;
long used;
PART *p;

	for (i = 0, p = &scsi.part[0], used = 0L; i < npart; i++, p++) {
		if (partmode == PARTMODE_ATARI) {
			if (xgminfo.flag[i])	/* allow for extended root sector */
				used++;
		}
		used += p->size;
	}

	if (partmode == PARTMODE_ICD)
		used += icd_xgm_count(npart);

	secavail = scsi.size - scsi.nbad - used - 1L;
	if ((scsi.size == 0) && (npart == 0))
		secavail = 0;

	convert_size(scsiedit[ESAVAIL].ob_spec,secavail,TRUE);
}

/****************************************************************************/
PRIVATE void update_total()
{
	convert_size(scsiedit[ESTOTAL].ob_spec,scsi.size,TRUE);
}

/*
 *	copy data from displayed partition entries to 'scsi.part[]' array
 */
PRIVATE void loadob_topart()
{
int i, j, len, phigh, plow;
char *t1;
TEDINFO *temp;
PART *p, *start, *end;
long centimegs;		/* i.e. hundredths of MB */
char str[5];

	for (i = 0, p = &scsi.part[toppart]; i < DISPLAY_PARTITIONS; i++, p++) {
		temp = (TEDINFO *) scsiedit[scsiob[i]].ob_spec;
		p->firstsector = atol(temp->te_ptext);

		if (editmode == PSIZE) {
			temp = (TEDINFO *) scsiedit[scsiob[i]+2].ob_spec;
			t1 = temp->te_ptext;
			if (sizemode == SECTORS) {
				p->size = atol(t1);
			} else {
				len = strlen(t1);
				if (len <= 4) {			/* e.g. 1___.__, 12__.__, 123_.__, 1234.__ */
					phigh = atoi(t1);
					plow = 0;
				} else {				/* e.g 1234.5_ or 1234.56 */
					memcpy(str,t1,4);
					str[4] = '\0';
					phigh = atoi(str);
					plow = atoi(t1+4);
					if (len == 5)		/* e.g. 1234.5_ */
						plow *= 10;
				}
				centimegs = phigh * 100 + plow;		/* units of 0.01 MB */
				if (secs_to_cmegs(p->size) != centimegs)
					p->size = cmegs_to_secs(centimegs);
			}
		} else {
			if (p->firstsector) {
				temp = (TEDINFO *) scsiedit[scsiob[i]+1].ob_spec;
				p->size = atol(temp->te_ptext) - p->firstsector + 1;
			} else p->size = 0;
		}

		if (p->size <= 0L) {
			memset(p,0x00,sizeof(PART));
			continue;
		}

		if ((p->size > 0L) && (p->size < MIN_PARTSIZE))
			p->size = MIN_PARTSIZE;

		temp = (TEDINFO *) scsiedit[scsiob[i]+3].ob_spec;
		strncpy(p->type,temp->te_ptext,3);

		if (*((char *) scsiedit[scsiob[i]+5].ob_spec) == CHECK_MARK)
			makeactive(p);
		else makeinactive(p);

		if (*((char *) scsiedit[scsiob[i]+6].ob_spec) == CHECK_MARK)
			makebootable(p);
	}

	/*
	 *	eliminate any 'holes' in the partition info
	 */
	for (i = 0, start = &scsi.part[0]; i < MAX_PARTITIONS; i++, start++) {
		if (iszero(start)) {						/* look for start of hole */
			for (j = i+1, end = start+1; j < MAX_PARTITIONS; j++, end++)
				if (!iszero(end))					/* look for end of hole */
					break;
			if (j >= MAX_PARTITIONS)				/* trailing empty slots, not a hole */
				break;
			memmove(start,end,(MAX_PARTITIONS-j)*sizeof(PART));
		}
	}
}

/*************************************************************************/
PRIVATE void edit_sectors()
{
int i;
TEDINFO *tted;

	tted = (TEDINFO *) scsiedit[EEDITMOD].ob_spec;
	editmode = PSECTORS;
	for (i = 0; i < DISPLAY_PARTITIONS; i++) {
		scsiedit[scsiob[i]].ob_flags = EDITABLE;
		scsiedit[scsiob[i]+1].ob_flags = EDITABLE;
		scsiedit[scsiob[i]+2].ob_flags = TOUCHEXIT;
		scsiedit[scsiob[i]+3].ob_flags = TOUCHEXIT;
	}
	strcpy(tted->te_ptext,text[PART_EDITSEC].ob_spec);
}

/*************************************************************************/
PRIVATE void edit_size()
{
int i;
TEDINFO *tted;

	tted = (TEDINFO *) scsiedit[EEDITMOD].ob_spec;
	editmode = PSIZE;
	for (i = 0; i < DISPLAY_PARTITIONS; i++) {
		scsiedit[scsiob[i]].ob_flags = TOUCHEXIT;
		scsiedit[scsiob[i]+1].ob_flags = TOUCHEXIT;
		scsiedit[scsiob[i]+2].ob_flags = EDITABLE;
		scsiedit[scsiob[i]+3].ob_flags = TOUCHEXIT;
	}
	strcpy(tted->te_ptext,text[PART_EDITSIZE].ob_spec);
}

/*************************************************************************/
PRIVATE void edit_ptype()
{
int i;
TEDINFO *tted;

	tted = (TEDINFO *) scsiedit[EEDITMOD].ob_spec;
	editmode = PTYPE;
	for (i = 0; i < DISPLAY_PARTITIONS; i++) {
		scsiedit[scsiob[i]].ob_flags = TOUCHEXIT;
		scsiedit[scsiob[i]+1].ob_flags = TOUCHEXIT;
		scsiedit[scsiob[i]+2].ob_flags = TOUCHEXIT;
		scsiedit[scsiob[i]+3].ob_flags = EDITABLE;
	}
	strcpy(tted->te_ptext,text[PART_EDITTYPE].ob_spec);
}

/*************************************************************************/
PRIVATE void recalc()
{
int i;
PART *p;

	/*
	 *	count partitions: must do this first!
	 */
	for (npart = 0, p = &scsi.part[0]; !iszero(p); npart++, p++)
		;

	if (editmode == PTYPE) {
		for (i = 0, p = &scsi.part[0]; i < npart; i++, p++) {
			if (!is_valid_ptype(p->type)) {
				xform_alert(1,PT_BADTYPE_ALERT);
				adjust_tos_ptype(p);
			}
		}
		return;
	}

	/*
	 *	editmode is PSECTORS or PSIZE
	 */
	while(build_xgminfo() < 0)
		;
}

/*
 *	checks partition type for validity (all alphanumeric characters)
 */
PRIVATE int is_valid_ptype(char *type)
{
int i;

	for (i = 0; i < 3; i++, type++)
		if (!isalnum(*type))
			return FALSE;

	return TRUE;
}

/*
 *	checks partition type for GEM or BGM
 */
PRIVATE int is_valid_tos_ptype(char *type)
{
	if (strncmp(type,GEM,3) == 0)
		return TRUE;
	if (strncmp(type,BGM,3) == 0)
		return TRUE;

	return FALSE;
}

PRIVATE void adjust_tos_ptype(PART *part)
{
int j;

	/*
	 *	check for standard partition type & adjust as required
	 */
	for (j = 0; j < 3; j++) {								//MAGNUM
		if (strncmp(partypes[j],part->type,3) == 0) {
			set_tos_ptype(part);
			return;
		}
	}
	/* oddball type ... leave it alone */
}

/*
 *	sets partition type to either BGM or GEM as appropriate
 */
PRIVATE void set_tos_ptype(PART *part)
{
char *ptype;

	if (part->size > MAX_LOGSECS_TOS14)
		ptype = BGM;
	else if ((rommode == TOSVER_10) && (part->size > MAX_LOGSECS_TOS10))
		ptype = BGM;
	else ptype = GEM;

	strncpy(part->type,ptype,3);
}

/*
 *	builds an appropriate XGMINFO table, depending on partmode
 *	and force_atari_mode
 *
 *	returns 0 for OK, -1 for retry
 *		retry can only happen if partmode == ATARI_MODE and force_atari_mode == TRUE
 *		and there are more than 4 partitions
 */
PRIVATE int build_xgminfo(void)
{
int i, j;
int nnorm, xgmi;
long pstart;
PART *p;

	if (!force_atari_mode)
		memset(&xgminfo,0x00,sizeof(xgminfo));	/* reset if we haven't found an appropriate XGM partition */

	pstart = calc_pstart(npart);

	nnorm = 0;
	xgmi = 0;
	for (i = 0, p = &scsi.part[0]; i < npart; i++, p++) {
		if (partmode == PARTMODE_ATARI) {
			if (!force_atari_mode) {
				if ((i == MAX_PARTS_ATARI-1) && (npart > MAX_PARTS_ATARI)) {
					xgminfo.exists = 1;
					xgminfo.rootsec = pstart;
				}
				if (xgminfo.exists) {
					pstart++;
					xgminfo.flag[i] = 1;
				}
			} else {				/* force_atari_mode == TRUE */
				if (xgminfo.flag[i]) {
					xgmi = i;
					pstart++;
				} else if ((nnorm == MAX_PARTS_ATARI-1) && (npart > MAX_PARTS_ATARI)) {
					if (xgmi) {
						for (j = xgmi; xgminfo.flag[j]; j++)
							;
						xgminfo.flag[j] = 1;
						scsi.part[j-1].size--;
						return -1;
					} else {
						xgminfo.exists = 1;
						xgmi = i;
						xgminfo.rootsec = pstart++;
						xgminfo.flag[i] = 1;
					}
				} else {
					nnorm++;
				}
			}
		} else {					/* ICD is simple ... */
			if ((xgminfo.exists) && (i >= MAX_PARTS_ICD-1))
				xgminfo.flag[i] = 1;
		}

		adjust_tos_ptype(p);		/* adjust partition type if necessary */
		p->firstsector = pstart;

		pstart += p->size;
	}

	memset(p,0x00,(MAX_PARTITIONS-i)*sizeof(PART));

	return 0;
}

/*******************************************************************/
PRIVATE void print_parts(short id,short lun)
{
int i;
char type[4];
char line[MAX_PRINTSIZE], sizestr[10];
PART *p;

	sprintf(line,printstr[PRINT_PARTHDR].ob_spec,id,lun);
	if (!print_string(line))
		return;
	if (!print_string(printstr[PRINT_PARTHDR2].ob_spec))
		return;
	if (!print_string(""))
		return;
	if (!print_string(printstr[PRINT_PARTHDR3].ob_spec))
		return;
	if (!print_string(printstr[PRINT_PARTHDR4].ob_spec))
		return;

	for (i = 0, p = &scsi.part[0]; i < npart; i++, p++) {
		convert_size(sizestr,p->size,TRUE);
		memcpy(type,p->type,3);
		type[3] = '\0';
		sprintf(line," %2d   %7ld   %7ld   %s   %s%c    %c      %c",
				i,p->firstsector,p->firstsector+p->size-1,
				sizestr,type,xgminfo.flag[i]?'*':' ',
				isactive(p)?CHECK_MARK:' ',isbootable(p)?CHECK_MARK:' ');
		if (!print_string(line))
			return;
	}

	if (xgminfo.exists) {
		if (!print_string(""))
			return;
		if (!print_string(printstr[PRINT_PARTNOTE].ob_spec))
			return;
	}
}

/*
 *	save partition info for disk to file
 */
PRIVATE void save_parts(short id,short lun)
{
char fbootf[MAX_PATHNAME];

	load_newbuf();
	if (get_path(fbootf,opt.filepath,PART_MASK,text[SCSIINFO_FSEL_PT].ob_spec))
		write_partinfo_file(fbootf,id,lun,scsi.newbuf,scsi.numrootsec*PHYSICAL_SECSIZE);
}

/*
 *	write the partinfo file from the specified buffer
 */
PUBLIC void write_partinfo_file(char *path,short id,short lun,char *buf,long size)
{
long fid;
char fname[MAX_PATHNAME];

	sprintf(fname,"%s" PART_FILE,path,id,lun);
	if ((fid=Fcreate(fname,0)) < 0L) {
		error(fid,OPEN,fname);
	} else {
		if (file_write(fid,size,buf,fname))
			error(fid,WRITE,fname);
		Fclose(fid);
	}
}

/*******************************************************************/
PRIVATE void load_newbuf()
{
int i, j, n, xcnt;
unsigned short boot, cnt;
long num;
char *buf;
long offset, temp, cumroot;
long icdnx[MAX_ICD_XGMPARTS+1];	/* size of ICD XGM partitions (icdnx[0] is unused) */
PART *pp, tpart, xpart;

	memset(scsi.newbuf,0x00,ROOTSEC_BUFSIZE);
	boot = addchk(0,(unsigned short *)scsi.orgbuf,PHYSICAL_SECSIZE/2);
	buf = scsi.newbuf;

	if (!force_newbuf)
		fast_copy(buf,scsi.orgbuf,PHYSICAL_SECSIZE);

	disksize(buf) = scsi.size;

	for (i = 0; i <= MAX_ICD_XGMPARTS; i++)
		icdnx[i] = 0L;

	n = (partmode==PARTMODE_ATARI)?MAX_PARTS_ATARI:MAX_PARTS_ICD;

	for (i = 0; i < n; i++)
		memset(part(buf,i),0x00,sizeof(PART));

	scsi.secnum[0] = 0L;		/* something that atari & icd have in common ... */

	if (partmode == PARTMODE_ICD) {
		scsi.numrootsec = 1 + icd_xgm_count(npart);
		for (j = 1; j < scsi.numrootsec; j++)
			scsi.secnum[j] = xgminfo.rootsec + j;
	}

	cnt = 0;
	num = 0;
	offset = 0L;
	for (i = 0; i < npart; i++) {
		if (partmode == PARTMODE_ATARI) {
			if (xgminfo.flag[i]) {
				memset(&xpart,0x00,sizeof(PART));
				makeactive(&xpart);
				memcpy(xpart.type,XGM,3);
				xpart.firstsector = xgminfo.rootsec;
				temp = offset = 0L;
				for (j = i; xgminfo.flag[j]; j++) {
					offset += PHYSICAL_SECSIZE;
					num++;
					scsi.secnum[num] = scsi.part[j].firstsector - 1L;
					temp += scsi.part[j].size + 1;
					memcpy(&tpart,&scsi.part[j],sizeof(PART));
					tpart.firstsector = 1;
					memcpy(part(buf+offset,0),&tpart,sizeof(PART));
					if (xgminfo.flag[j+1]) {
						makeactive(&tpart);
						memcpy(tpart.type,XGM,3);
						tpart.firstsector = scsi.part[j+1].firstsector - xgminfo.rootsec - 1;
						tpart.size = scsi.part[j+1].size + 1;
						memcpy(part(buf+offset,1),&tpart,sizeof(PART));
					}
				}
				xpart.size = temp;
				memcpy(part(buf,cnt++),&xpart,sizeof(PART));
				i = j - 1;
			} else {
				memcpy(part(buf,cnt++),&scsi.part[i],sizeof(PART));
			}
		} else {
			if (xgminfo.flag[i]) {
				memset(&xpart,0x00,sizeof(PART));
				makeactive(&xpart);
				memcpy(xpart.type,XGM,3);
				xpart.firstsector = xgminfo.rootsec;
				temp = 1L;
				offset = PHYSICAL_SECSIZE;
				num++;
				xcnt = 0;
				cumroot = scsi.part[i].firstsector - xgminfo.rootsec;

				for (j = i; xgminfo.flag[j]; j++) {
					temp += scsi.part[j].size;
					icdnx[num] += scsi.part[j].size;
					memcpy(&tpart,&scsi.part[j],sizeof(PART));
					tpart.firstsector = cumroot;
					memcpy(part(buf+offset,xcnt++),&tpart,sizeof(PART));
					cumroot += scsi.part[j].size;

					if ((xcnt == MAX_PARTS_ICD-1) && (xgminfo.flag[j+1])) {
						makeactive(&tpart);
						memcpy(tpart.type,XGM,3);
						tpart.firstsector = xgminfo.rootsec + num;
						tpart.size = 0;
						memcpy(part(buf+offset,MAX_PARTS_ICD-1),&tpart,sizeof(PART));

						num++;
						offset += PHYSICAL_SECSIZE;
						cumroot = scsi.part[j+1].firstsector - tpart.firstsector;
						icdnx[num] = 1;
						temp++;
						xcnt = 0;
					}
				}
				i = j - 1;
				for (j = 2; j <= num; j++) {
					offset = (j - 1) * PHYSICAL_SECSIZE;
					pp = part(buf+offset,MAX_PARTS_ICD-1);
					pp->size = icdnx[j];
				}
				xpart.size = temp;

				memcpy(part(buf,cnt++),&xpart,sizeof(PART));
			} else {
				memcpy(part(buf,cnt++),&scsi.part[i],sizeof(PART));
			}
		}
	}
	if (partmode == PARTMODE_ATARI)
		scsi.numrootsec = ++num;

	if (boot == 0x1234) {		/* fix up checksum */						//MAGNUM
		buf = scsi.newbuf;
		cnt = addchk(0L,(unsigned short *)buf,PHYSICAL_SECSIZE/2-1);
		i = 0x1234 - cnt;													//MAGNUM
		*((short *) buf + PHYSICAL_SECSIZE/2 - 1) = i;
	}
}

/*******************************************************************/
PRIVATE void rebuild_one(short id,short lun)
{
short part, exitobj;
TEDINFO *reb1num;

	reb1num = (TEDINFO *)rebone[REB1NUM].ob_spec;
	strcpy(reb1num->te_ptext,"");

	while(TRUE) {
		exitobj = handle_dial(rebone,REB1NUM,0,0);

		if (exitobj == REB1OK) {
			part = atoi(reb1num->te_ptext);
			if ((part < 1) || (part > npart)) {
				xform_alert(1,PT_INVAL_ALERT);
				continue;
			}
			if (write_scsi_info(id,lun) == 0) {
				need_to_reboot = TRUE;
				strcpy(scsiedit[ESCANCEL].ob_spec,text[PART_EXIT].ob_spec);
				redraw_object(scsiedit,ESCANCEL);
				rebuild(id,lun,&scsi.part[part-1],1,0);
			}
		}
		break;
	}
}

/*******************************************************************/
PRIVATE void oneboot(short id,short lun)
{
short part, exitobj, bootat;
TEDINFO *boot1num;

	boot1num = (TEDINFO *) bone[BOOTPART].ob_spec;
	strcpy(boot1num->te_ptext,"");

	while(TRUE) {
		exitobj = handle_dial(bone,BOOTPART,0,0);

		if (exitobj == BOOTOK) {
			part = atoi(boot1num->te_ptext);
			if ((part < 1) || (part > npart)) {
				xform_alert(1,PT_INVAL_ALERT);
				continue;
			}
			if (write_scsi_info(id,lun) == 0) {
				strcpy(scsiedit[ESCANCEL].ob_spec,text[PART_EXIT].ob_spec);
				redraw_object(scsiedit,ESCANCEL);
				if (bone[BOOTAT].ob_state & SELECTED)
					bootat = 1;						/* set for rebuild() */
				else bootat = 0;
				rebuild(id,lun,&scsi.part[part-1],0,bootat);
				need_to_reboot = TRUE;
			}
		}
		break;
	}
}

/*******************************************************************/
PRIVATE void rebuild_all(short id,short lun)
{
int i;
PART *p;

	if (write_scsi_info(id,lun) == 0) {
		need_to_reboot = TRUE;
		strcpy(scsiedit[ESCANCEL].ob_spec,text[PART_EXIT].ob_spec);
		redraw_object(scsiedit,ESCANCEL);
		for (i = 0, p = &scsi.part[0]; i < npart; i++, p++)
			rebuild(id,lun,p,1,0);
	}
}

/*******************************************************************/
PRIVATE int write_scsi_info(short id,short lun)
{
long offset;
int i, n, error;
long max_partsize_gem, max_partsize_bgm;
PART *part;

	load_newbuf();		/* populates scsi.newbuf */

	/*
	 *	check GEM and BGM partition sizes for:
	 *		too big for any TOS version
	 *		too big for current mode
	 */
	switch(rommode) {
	default:					/* i.e. TOSVER_10 */
		max_partsize_gem = MAX_PHYS_SECS_GEM_TOS10;
		max_partsize_bgm = MAX_PHYS_SECS_BGM_TOS10;
		break;
	case TOSVER_14:
		max_partsize_gem = MAX_PHYS_SECS_GEM_TOS14;
		max_partsize_bgm = MAX_PHYS_SECS_BGM_TOS14;
		break;
	case TOSVER_40:
		max_partsize_gem = MAX_PHYS_SECS_GEM_TOS14;
		max_partsize_bgm = MAX_PHYS_SECS_BGM_TOS4;
		break;
	}
	for (i = 0, part = scsi.part, n = error = 0; i < npart; i++, part++) {
		if (strncmp(part->type,GEM,3) == 0) {
			if (part->size > MAX_PHYS_SECS_GEM_TOS14)
				error++;
			else if (part->size > max_partsize_gem)
				n++;
		} else if (strncmp(part->type,BGM,3) == 0) {
			if (part->size > MAX_PHYS_SECS_BGM_TOS4)
				error++;
			else if (part->size > max_partsize_bgm)
				n++;
		}
	}

	if (error) {
		xform_alert(1,PT_BIGERR_ALERT,error);	/* too big for any TOS version */
		return -1;
	}
 
	if (n) {
		if (xform_alert(2,PT_BIGWARN_ALERT,n) == 2)
			return -1;
	}

	/*
	 *	check number of active partitions
	 */
	for (i = 0, n = 0; i < npart; i++)
		if (scsi.part[i].exists)
			n++;
	if ((n < 1) || (n > NUM_HD_DEVICES)) {
		if (xform_alert(2,PT_NUMENBL_ALERT,NUM_HD_DEVICES) == 2)
			return -1;
	}
	if (n < npart) {
		if (xform_alert(2,PT_DISABLE_ALERT) == 2)
			return -1;
	}

	for (i = 0, offset = 0L; i < scsi.numrootsec; i++, offset += PHYSICAL_SECSIZE)
		if (phys_write(id,lun,1,scsi.secnum[i],scsi.newbuf+offset) != 0)
			return scsi_op_failed(id,lun);

	return 0;
}

/*******************************************************************/
PRIVATE int rebuild(short id,short lun,PART *part,short all,short bootat)
{
long nbytes, maxsec;
int secsize, max_logsecsize;
int nps, nwrite, num2write, nout;
unsigned short logsecs;
char *buf, *buf2;
MYBPB mybpb, dummybpb;

	nbytes = part->size * PHYSICAL_SECSIZE;

	if (strncmp(part->type,BGM,3) == 0) {
		if (bootat)
			maxsec = (rommode==TOSVER_10) ? MAX_LOGSECS_TOS10_A : MAX_LOGSECS_TOS14_A;
		else maxsec = (rommode==TOSVER_10) ? MAX_LOGSECS_TOS10 : MAX_LOGSECS_TOS14;
		max_logsecsize = (rommode==TOSVER_40) ? MAX_LOGSECSIZE_TOS4 : MAX_LOGSECSIZE;
		for (secsize = MIN_LOGSECSIZE; secsize < max_logsecsize; secsize *= 2)
			if (maxsec*secsize >= nbytes)
				break;
	} else secsize = MIN_LOGSECSIZE;

	logsecs = (unsigned short) (nbytes/secsize);

	nps = secsize / PHYSICAL_SECSIZE;
	buf = scsi.newbuf;
	memset(buf,0x00,ROOTSEC_BUFSIZE);

	Protobt(buf,0x01000000L,-1,0);
	memcpy(buf+2,BOOT_NAME,6);
	mybpb.bps = secsize;
	mybpb.nres = DEFAULT_NRES;
	mybpb.nfat = DEFAULT_NFAT;
	mybpb.spf = (logsecs/2*sizeof(WORD) + secsize-1) / secsize;		/* round up */

	if (!all) {
		buf2 = calloc(1,PHYSICAL_SECSIZE);
		if (!buf2)
			return scsi_op_failed(id,lun);
		if (phys_read(id,lun,1,part->firstsector,buf2)) {
			free(buf2);
			return scsi_op_failed(id,lun);
		}
		load_mybpb(buf2,&dummybpb,NULL);
		free(buf2);
		if (dummybpb.spc == MIN_SPC)		/* rationalise sectors per cluster */
			mybpb.spc = MIN_SPC;
		else mybpb.spc = DEFAULT_SPC;

		if ((dummybpb.ndir < MIN_RDE) || (dummybpb.ndir > MAX_RDE))
			mybpb.ndir = DEFAULT_RDE;		/* rationalise # root directory entries */
		else mybpb.ndir = dummybpb.ndir;

		if ((dummybpb.spf >= (mybpb.spf - 1)) && (dummybpb.spf <= mybpb.spf + 1))
			mybpb.spf = dummybpb.spf;
	} else {
		mybpb.spc = DEFAULT_SPC;
		mybpb.ndir = DEFAULT_RDE;
	}

	mybpb.tsec = logsecs;
	mybpb.media = DEFAULT_MEDIA;
	mybpb.spt = (force_newbuf) ? 0 : secpt(scsi.orgbuf);
	mybpb.tpc = (force_newbuf) ? 0 : nheads(scsi.orgbuf);
	mybpb.nhide = 1;											//MAGNUM
	loadboot(buf,&mybpb);

	if (all)
		num2write = (1+mybpb.nfat*mybpb.spf)*nps + (mybpb.ndir*sizeof(DIRENTRY)+PHYSICAL_SECSIZE-1)/PHYSICAL_SECSIZE;
	else num2write = 1;

	for (nout = 0; nout < num2write; nout += nwrite) {
		nwrite = min(ROOTSEC_BUFSIZE/PHYSICAL_SECSIZE,num2write-nout);
		if (phys_write(id,lun,nwrite,part->firstsector+nout,buf))
			return scsi_op_failed(id,lun);
		if (nout == 0)							/* first time? */
			memset(buf,0x00,PHYSICAL_SECSIZE);	/* yes - reset sector */
	}

	return 0;
}

/*******************************************************************/

/*
 *	convert_size: convert specified size (in sectors) to string
 *
 *	the external variable 'sizemode' controls the contents of the
 *	output string: sectors, or MB with 2 decimal places.  if MB,
 *	the argument 'dot' controls whether a decimal point is inserted.
 */
PRIVATE void convert_size(char *s,long size,int dot)
{
long n;
int neg = FALSE;
int phigh, plow;

	if (sizemode == SECTORS) {
		sprintf(s,"%7ld",size);
		return;
	}

	if (size < 0L) {
		size = -size;
		neg = TRUE;
	}

	n = secs_to_cmegs(size);
	phigh = n / 100;
	plow = n - phigh * 100;
	if (neg)
		phigh = -phigh;
	if (dot)
		sprintf(s,"%4d.%02d",phigh,plow);
	else sprintf(s,"%4d%02d",phigh,plow);
}

/*
 *	conversion routines between MB display format (0.01 MB units) and sectors
 *		convert sectors to 0.01MB
 *		convert 0.01MB to sectors
 *
 *	the code allows values of up to about 128 GB before arithmetic overflow
 *	occurs, while maintaining accuracy whether KBYTE is 1000 or 1024 ...
 */
#define SCALE_FACTOR	64
#define SCALED_SECSIZE	(PHYSICAL_SECSIZE/SCALE_FACTOR)
#define SCALED_MBYTE	(MBYTE/SCALE_FACTOR)

PRIVATE long secs_to_cmegs(long sectors)
{
long n, phigh, plow;

	n = sectors * SCALED_SECSIZE;
	phigh = n / SCALED_MBYTE;
	plow = ((n-phigh*SCALED_MBYTE) * 100 + SCALED_MBYTE/2) / SCALED_MBYTE;

	return phigh * 100 + plow;
}

PRIVATE long cmegs_to_secs(long cmegs)
{
long n, phigh, plow;

	phigh = cmegs / 100;			/* split into megs and hundredths ... */
	plow = cmegs - phigh * 100;

	n = (phigh * SCALED_MBYTE) + (plow * SCALED_MBYTE / 100);

	return (n + SCALED_SECSIZE/2) / SCALED_SECSIZE;
}
