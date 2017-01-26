/*
 *	undel.c: performs the file undelete function
 *
 *
 *	version 2.10 (december/2002)		roger burrows
 *		a highly cleaned up and reorganised version of
 *		the DE v2.04 source; no new function
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <ctype.h>
#include <dos.h>
#include <portab.h>
#include <string.h>
#include <vdi.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"

#define MODULE_NUMBER	14		/* used by error() macro */

#define SIM		1
#define MIR		0

typedef struct {
	DIRENTRY *entry;
	short tot;
	short recover;
	short undel;
} DELENTRY;


extern char ascii[256];
extern STATUS status;
extern short dev, win1, workx, worky, workw, ychar;
extern short fullx, fully, fullw, fullh;
extern short rez, myhandle, edpos, edit_obj;
extern short dirlevel;
extern DIRLIST dirlist[MAX_DIR_DEPTH];
extern DIRENTRY direntry;
extern OPT opt;
extern MYBPB dpb;
extern FILEINFO finfo;
extern OBJECT *bottom, *menu, *text, *undel, *undname;


/*
 *	private variables
 */
MLOCAL MFDB screen = { NULL } ;		/* i.e. screen display memory */
MLOCAL int display_lines;			/* number of lines to display in window */
MLOCAL short tlines[MAX_DIR_DEPTH];
MLOCAL short topline;
MLOCAL short fdirlevel, foundl, nfound, nskip, cstart, pstart;
MLOCAL short lastsel, numentry;
MLOCAL short ubarrange, utype;
MLOCAL long findoff;
MLOCAL char ssearch[MAX_FILENAME];
MLOCAL char *curdirbuf;		/* ptr to buffer containing directory currently being examined */
MLOCAL char *endpoint;
MLOCAL DELENTRY *ddir;
MLOCAL TEDINFO *umask;
MLOCAL char *fat12;					/* FATs */
MLOCAL unsigned short *fat16;
MLOCAL unsigned short *mirrorfat;


/*
 *	function prototypes
 */
PRIVATE void calc_prob(DIRENTRY *entry,short *total,short *recover);
PRIVATE void cleanup_old_directory(void);
PRIVATE int dirok(void);
PRIVATE int garbage(DIRENTRY *dir);
PRIVATE int get_uinfo(DIRENTRY *dir);
PRIVATE void load_finfo_to_string(unsigned char *temp,DELENTRY *de);
PRIVATE unsigned short *load_mirror(void);
PRIVATE void udesel_all(void);
PRIVATE void udo_child(DIRENTRY *f);
PRIVATE short ufind(unsigned char *path,short sclus);
PRIVATE int uload_dir(unsigned short sclus);
PRIVATE void undelete_one_file(DIRENTRY *entry);
PRIVATE void update_barmask(unsigned char *str);
PRIVATE void uw_fat(void);
PRIVATE int wildc(unsigned char *s,unsigned char *p);


/*
 *	fixup undelete form/objects - must be called prior to any other function
 */
PUBLIC void init_ud_form()
{
short dummy, window_height, udrop_offset, udrop_bottom;
int adjust, j;
TEDINFO *temp;

	temp = (TEDINFO *) undel[USMASK].ob_spec;
	strcpy(temp->te_ptext,"");
	ubarrange = undel[UBARBACK].ob_height - undel[UBARTOP].ob_height;
	for (j = ULINE0; j <= ULINE15; j++)
		strcpy(undel[j].ob_spec,"");
	ddir = NULL;

	/*
	 *	calculate number of lines that we can display,
	 *	and adjust objects appropriately
	 */
	display_lines = ULINE15 - ULINE0 + 1;						/* default */
	wind_calc(WC_WORK,PARTS,fullx,fully,fullw,fullh,&dummy,&dummy,&dummy,&window_height);	/* height of window work area */
	window_height -= bottom[ROOT].ob_height + ychar;			/* allow for bottom + margin */
	objc_offset(undel,UDROP,&dummy,&udrop_offset);
	udrop_bottom = udrop_offset + undel[UDROP].ob_height;		/* bottom of default display area */
	if (udrop_bottom > window_height) {							/* if it overlaps, */
		adjust = (udrop_bottom-window_height+ychar-1) / ychar;	/* calc adjustment in lines */
		display_lines -= adjust;
		adjust *= ychar;										/* adjustment in pixels */
		undel[UDOWN].ob_y -= adjust;
		undel[UBARBACK].ob_height -= adjust;
		undel[UDROP].ob_height -= adjust;
		undel[UNDELBCK].ob_height -= adjust;
	}

	if ((undel[UDOWN].ob_flags & FL3DMASK) && (ychar == 8)) {	/* picayune Falcon adjustment */
		undel[USTR5].ob_y -= 2;
		undel[USTR6].ob_y -= 2;
	}

#if 0
	if (fullh > 400) {			/* e.g. TT medium, TT high */			//MAGNUM
		display_lines = 16;												//MAGNUM
	} else if (fullh > 240) {	/* e.g. ST high */						//MAGNUM
		display_lines = 12;												//MAGNUM
		undel[UDOWN].ob_y -= 64;										//MAGNUM	probably 4*charh
		undel[UBARBACK].ob_height -= 64;								//MAGNUM
		undel[UDROP].ob_height -= 64;									//MAGNUM
		undel[UNDELBCK].ob_height -= 64;								//MAGNUM
	} else if (fullh > 200) {	/* e.g. Falcon */						//MAGNUM
		display_lines = 16;												//MAGNUM
		if (undel[UDOWN].ob_flags & FL3DMASK) {
			form_alert(1,"[1][Adjusting USTR5/6 ][ OK ]");
			undel[USTR5].ob_y -= 2;
			undel[USTR6].ob_y -= 2;
		}
	} else {					/* e.g ST medium */
		display_lines = 11;												//MAGNUM
		undel[UDOWN].ob_y -= 40;										//MAGNUM	probably 5*charh
		undel[UBARBACK].ob_height -= 40;								//MAGNUM
		undel[UDROP].ob_height -= 40;									//MAGNUM
		undel[UNDELBCK].ob_height -= 40;								//MAGNUM
	}
#endif

	for (j = ULINE0+display_lines; j <= ULINE15; j++)
		undel[j].ob_flags |= HIDETREE;
}

/*
 *	initialise undelete processing
 */
PUBLIC int init_undel()
{
	change_mode(UNDELETE);

	umask = (TEDINFO *)undel[USMASK].ob_spec;
	strcpy(umask->te_ptext,"*      *");
	ssearch[0] = DELETION_MARK;
	strcpy(&ssearch[1],umask->te_ptext);
	update_barmask("*.* ");
	undel[FINDNEXT].ob_state |= DISABLED;	/* reset find/findnext when switching drives */
	memset(tlines,0x00,MAX_DIR_DEPTH*sizeof(short));

	if (init_uroot(dev,0) < 0) {
		cleanup_undelete();			/* ensure no memory leak */
		if (edit_obj) {
			objc_edit(undel,edit_obj,0,&edpos,EDEND);
			edit_obj = 0;
		}
		return -1;
	}
	objc_draw(undel,0,MAX_DEPTH,workx,worky,workw,undel->ob_height);

	return 0;
}

/*
 *	the following routine is called:
 *		. whenever we switch away from UNDELETE
 *		. at termination
 *	and whenever we call init_uroot()
 */
PUBLIC void cleanup_undelete()
{
	cleanup_old_directory();
	if (fat16) {
		free(fat16);
		fat16 = NULL;
	}
	if (fat12) {
		free(fat12);
		fat12 = NULL;
	}
	if (mirrorfat) {
		free(mirrorfat);
		mirrorfat = NULL;
	}
}

/*
 *	the following routine is called whenever we load a new directory (uload_dir())
 *	(and also when we switch away from UNDELETE)
 */
PRIVATE void cleanup_old_directory()
{
	if (ddir) {
		free(ddir);
		ddir = NULL;
	}
	if (curdirbuf) {
		free(curdirbuf);
		curdirbuf = NULL;
	}
}

/*
 *	determine recoverability of file
 */
PRIVATE void calc_prob(DIRENTRY *entry,short *total,short *recover)
{
int i, j, nclus, notused;
short st;
unsigned short *f;

	get_uinfo(entry);
	nclus = (finfo.size+dpb.bpc-1) >> dpb.cluster_shift;

	if (finfo.size == 0L) {
		*total = 1;
		*recover = 1;
		return;
	}
	if (finfo.size < 0L) {
		*total = 1;
		*recover = 0;
		return;
	}

	switch(utype) {
	case MIR:
		notused = j = 0;
		st = finfo.start;
		if (mirrorfat[st]) {
			do {
				if (fat16[st] == 0)
					notused++;
				st = mirrorfat[st];
				j++;
			} while (st != dpb.bitmask);
			if (j == nclus)
				break;
		}
		/* otherwise drop thru': file not recoverable from mirror */
	case SIM:
		for (i = 0, notused = 0, f = fat16+finfo.start; i < nclus; i++) {
			if (*f++ == 0)
				notused++;
		}
	}

	*total = nclus;
	*recover = notused;
}

/*
 *	populate form with directory entries
 */
PUBLIC void load_ulines(short start)
{
short j, idx;
int notshown;
char str[MAX_DISPLAYSIZE];

	if (start >= 0)			/* -1 means don't change topline */
		topline = start;

	for (j = 0; j < display_lines; j++) {
		idx = topline + j;
		if (idx < numentry) {
			get_uinfo(ddir[idx].entry);
			load_finfo_to_string(str,&ddir[idx]);
			strcpy(undel[ULINE0+j].ob_spec,str);
			if (ddir[idx].undel)
				undel[ULINE0+j].ob_state |= SELECTED;
			else undel[ULINE0+j].ob_state &= ~SELECTED;
		} else strcpy(undel[ULINE0+j].ob_spec,"");
	}

	notshown = numentry - display_lines;
	if (notshown > 0) {
		j = (numentry / display_lines) + 1;
		undel[UBARTOP].ob_height = undel[UBARBACK].ob_height / j;
		ubarrange = undel[UBARBACK].ob_height - undel[UBARTOP].ob_height;
		undel[UBARTOP].ob_y = topline * ubarrange / notshown;
	} else {
		ubarrange = 0;
		undel[UBARTOP].ob_height = undel[UBARBACK].ob_height;
		undel[UBARTOP].ob_y = 0;
	}
}

/*
 *	convert information from 'finfo' structure to display format
 */
PRIVATE void load_finfo_to_string(char *temp,DELENTRY *de)
{
char nm[NODE_LENGTH+1];
char ext[EXT_LENGTH+1];
char *p, *d;
char dt_str[20], recstr[20];

	strcpy(ext,"   ");
	d = nm;
	p = finfo.name;
	while (*p != '\0') {
		if (*p == '.') {
			*d = '\0';
			d = ext;
			p++;
		} else *d++ = *p++;
	}
	*d = '\0';

	decode_date_time(dt_str,finfo.date,finfo.time,opt.datemode);

	if (finfo.attr & FA_SUBDIR) {
		sprintf(temp,"%c %-8s.%3s            %s",
				FOLDER_INDICATOR,nm,ext,dt_str);
	} else {
		if (de->recover == 0)
			sprintf(recstr,"%-6.6s",text[MISC_NO].ob_spec);
		else if (de->tot == de->recover)
			sprintf(recstr,"%-6.6s",text[MISC_YES].ob_spec);
		else sprintf(recstr,"%5d/%-5d",de->recover,de->tot);
		sprintf(temp,"  %-8s.%3s %10ld %s %s",
				nm,ext,finfo.size,dt_str,recstr);
	}
}

/*
 *	populate 'finfo' from directory entry
 */
PRIVATE int get_uinfo(DIRENTRY *dir)
{
	if (!dir)
		return 0;

	if (!get_name(dir,finfo.name))
		return 0;

	finfo.attr = dir->attr;
	finfo.time = get_word((char *)&dir->time);
	finfo.date = get_word((char *)&dir->date);
	finfo.start = get_word((char *)&dir->start);
	finfo.size = get_long((char *)&dir->size);

	return 1;
}

/*
 *	set search mask in window heading bar
 */
PRIVATE void update_barmask(char *str)
{
short xoff, yoff, len;
TEDINFO *barmask;

	barmask = (TEDINFO *)undel[UBARMASK].ob_spec;
	objc_offset(undel,UTOP,&xoff,&yoff);
	len = strlen(str);
	undel[UBARMASK].ob_x = (undel[UTOP].ob_width - len * 8 - 16) / 2;	//MAGNUM
	sprintf(barmask->te_ptext," %c%s",DELETION_MARK,str);
}

/*
 *	scroll undelete display upwards by 1 line
 */
PUBLIC void udo_up1()
{
short xoff, yoff;
GRECT rect1, rect2;

	if (topline == 0)
		return;

	topline--;
	load_ulines(-1);

	objc_offset(undel,ULINE0,&xoff,&yoff);
	rect1.g_x = xoff;
	rect1.g_y = yoff;
	rect1.g_w = undel[ULINE0].ob_width;
	rect1.g_h = undel[ULINE0].ob_height * (display_lines - 1);

	objc_offset(undel,ULINE1,&xoff,&yoff);
	rect2.g_x = xoff;
	rect2.g_y = yoff;
	rect2.g_w = undel[ULINE1].ob_width;
	rect2.g_h = undel[ULINE1].ob_height * (display_lines - 1);

	blit(&rect1,&rect2);
	redraw_object(undel,ULINE0);
	redraw_xobject(undel,UBARBACK);
}

/*
 *	scroll undelete display downwards by 1 line
 */
PUBLIC void udo_down1()
{
short xoff, yoff;
GRECT rect1, rect2;

	if (topline >= (numentry - display_lines))
		return;

	topline++;
	load_ulines(-1);
	objc_offset(undel,ULINE1,&xoff,&yoff);
	rect1.g_x = xoff;
	rect1.g_y = yoff;
	rect1.g_w = undel[ULINE1].ob_width;
	rect1.g_h = undel[ULINE1].ob_height * (display_lines - 1);

	objc_offset(undel,ULINE0,&xoff,&yoff);
	rect2.g_x = xoff;
	rect2.g_y = yoff;
	rect2.g_w = undel[ULINE0].ob_width;
	rect2.g_h = undel[ULINE0].ob_height * (display_lines - 1);

	blit(&rect1,&rect2);
	redraw_object(undel,ULINE0+display_lines-1);
	redraw_xobject(undel,UBARBACK);
}

/*
 *	scroll undelete display upwards by 1 page
 */
PUBLIC void udo_up_page()
{
int i;

	if (topline == 0)
		return;

	topline = max(0,topline-display_lines+1);
	load_ulines(-1);

	redraw_xobject(undel,UBARBACK);
	for (i = 0; i < display_lines; i++)
		redraw_object(undel,ULINE0+i);
}

/*
 *	scroll undelete display downwards by 1 page
 */
PUBLIC void udo_down_page()
{
int i;

	if (topline >= (numentry - display_lines))
		return;

	topline = min(numentry-display_lines,topline+display_lines-1);
	load_ulines(-1);

	redraw_xobject(undel,UBARBACK);
	for (i = 0; i < display_lines; i++)
		redraw_object(undel,ULINE0+i);
}

/*
 *	scroll undelete display to specified position
 */
PUBLIC void udo_random_page(short pos)
{
int i;

	if (numentry <= display_lines)
		return;

	topline = (long) pos * (numentry - display_lines) / 1000L;
	load_ulines(-1L);

	redraw_xobject(undel,UBARBACK);
	for (i = 0; i < display_lines; i++)
		redraw_object(undel,ULINE0+i);
}

/*
 *	load directory (root or otherwise) into memory, then build a table
 *	of DELENTRYs representing deleted and not-deleted entries
 */
PRIVATE int uload_dir(unsigned short sclus)
{
char *curpoint, *startpoint;
short st, ncl, j;
long offset;

	cleanup_old_directory();

	numentry = 0;				/* precaution in case load fails ... */

	if (sclus == 0) {			/* i.e. root */
		if ((curdirbuf=malloc(dpb.rdb)) == NULL) {
			error(dpb.rdb,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.rds,1+2*dpb.spf,curdirbuf,DIR))
			return -1;
		dirlevel = lastsel = topline = 0;
		startpoint = curdirbuf;
		endpoint = sort_root(curdirbuf,0);		/* don't remove deletions! */
	} else {					/* all other directories */
		st = sclus;
		ncl = 1;
		while (fat16[st] != dpb.bitmask) {
			st = fat16[st];
			if (!st)
				return -1;
			ncl++;
		}
		if ((curdirbuf=calloc(ncl,dpb.bpc)) == NULL) {
			error(ncl*dpb.bpc,MEMALLOC,"");
			return -1;
		}
		offset = 0L;
		st = sclus;
		do {
			if (log_read(dev,dpb.spc,logsector(st),curdirbuf+offset,DIR)) {
				free(curdirbuf);
				curdirbuf = NULL;
				return -1;
			}
			st = fat16[st];
			offset += dpb.bpc;
		} while(st!=dpb.bitmask);
		startpoint = curdirbuf + 2*sizeof(DIRENTRY);
		endpoint = sort_dir(curdirbuf,ncl,0);		/* don't remove deletions! */
	}

	for (curpoint = startpoint; curpoint < endpoint; curpoint += sizeof(DIRENTRY)) {
		if (*curpoint == DELETION_MARK) {
			if (!garbage((DIRENTRY *)curpoint))
				numentry++;
		} else if (fattr(curpoint) & FA_SUBDIR)
			numentry++;
	}
	if (numentry == 0)
		return 0;

	if ((ddir=calloc(numentry,sizeof(DELENTRY))) == NULL) {
		error(sizeof(DELENTRY),MEMALLOC,"");
		free(curdirbuf);
		curdirbuf = NULL;
		return -1;
	}

	j = 0;
	for (curpoint = startpoint; curpoint < endpoint; curpoint += sizeof(DIRENTRY)) {
		if (*curpoint == DELETION_MARK) {
			if (!garbage((DIRENTRY *)curpoint)) {
				ddir[j].entry = (DIRENTRY *)curpoint;
				calc_prob((DIRENTRY *)curpoint,&ddir[j].tot,&ddir[j].recover);
				j++;
			}
			continue;
		}
		if (fattr(curpoint) & FA_SUBDIR) {
			ddir[j].entry = (DIRENTRY *)curpoint;
			ddir[j++].tot = -1;
		}
	}

	return 0;
}

/*
 *	display parent of current directory
 */
PUBLIC void udo_parent()
{
short len;
char str[MAX_PATHNAME];
DIRENTRY *d;
char *lastslash;

	if (dirlevel == 0)
		return;				/* can't go higher than root */

	tlines[dirlevel] = 0;
	dirlevel--;
	if (dirlevel == 0) {
		uload_dir(0);				//rfb: need to test rc!!
		cstart = 0;
		*((char *) undel[UDIR].ob_spec+3) = '\0';
	} else {
		uload_dir(pstart);			//rfb: need to test rc!!
		cstart = pstart;
		d = (DIRENTRY *) (curdirbuf + sizeof(DIRENTRY));
		pstart = get_word((char *)&d->start);
		strcpy(str, undel[UDIR].ob_spec);
		len = strlen(str);
		if (str[len - 1] == '\\') {
			str[len - 1] = '\0';
		}
		lastslash = rindex(str, '\\');
		*(lastslash + 1) = '\0';
		strcpy(undel[UDIR].ob_spec, str);
	}
	load_ulines(tlines[dirlevel]);
	udesel_all();
	redraw_object(undel,UDIR);
	redraw_xobject(undel,UBARBACK);
	redraw_object(undel,UDROP);
}

/*
 *	display child of current directory
 */
PRIVATE void udo_child(DIRENTRY *f)
{
char str[MAX_PATHNAME];
char sname[MAX_FILENAME];
short sstart;

	get_uinfo(f);
	sstart = finfo.start;
	strcpy(sname,finfo.name);
	udesel_all();
	if (uload_dir(finfo.start) < 0) {
		if (dirlevel == 0)
			uload_dir(0);
		else uload_dir(cstart);
		load_ulines(-1);
		redraw_object(undel,UDIR);
		redraw_xobject(undel,UBARBACK);
		redraw_object(undel,UDROP);
		return;
	}
	tlines[dirlevel] = topline;
	load_ulines(0);
	dirlevel++;
	pstart = cstart;
	cstart = sstart;
	strcpy(str,undel[UDIR].ob_spec);
	strcat(str,sname);
	strcat(str,"\\");
	strncpy(undel[UDIR].ob_spec,str,72);					//MAGNUM
	redraw_object(undel,UDIR);
	redraw_xobject(undel,UBARBACK);
	redraw_object(undel,UDROP);
}

/*************************************************************************/
PUBLIC void udo_line(short obj,short clicks,short shift)
{
short curentry, j, k;
DIRENTRY *f;

	curentry = topline + obj - ULINE0;
	if (curentry >= numentry)
		return;

	f = ddir[curentry].entry;
	if (f->attr & FA_SUBDIR) {
		if (f->name[0] != DELETION_MARK) {
			udo_child(f);
			return;
		}
	}

	if (ddir[curentry].recover) {
		if (shift) {
			undel[obj].ob_state |= SELECTED;
			redraw_object(undel,obj);
			ddir[curentry].undel = 1;
		} else {
			k = min(topline+display_lines,numentry);
			for (j = topline; j < k; j++) {
				if (ddir[j].undel) {
					undel[j - topline + ULINE0].ob_state &= ~SELECTED;
					redraw_object(undel, j - topline + ULINE0);
				}
			}
			for (j = 0; j < numentry; j++)
				ddir[j].undel = 0;
			undel[obj].ob_state |= SELECTED;
			redraw_object(undel,obj);
			ddir[curentry].undel = 1;
		}
	}
}

/*
 *	deselect all lines in the undelete window
 */
PRIVATE void udesel_all()
{
int j;

	for (j = 0; j < display_lines; j++)
		undel[ULINE0+j].ob_state &= ~SELECTED;

	lastsel = 0;
}

/*
 *	find deleted file
 *
 *	fnext==0	find first
 *	else		find next
 */
PUBLIC void udo_find(int fnext)
{
short clus, j;
char temp[MAX_FILENAME];
char temp2[MAX_FILENAME];
char path[4];
DIRENTRY *d;

	nfound = 0;
	temp[0] = DELETION_MARK;
	strcpy(&temp[1],umask->te_ptext);
	get_name((DIRENTRY *)temp,temp2);
	memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));
	if (fnext) {
		if (strcmp(ssearch,temp2) != 0)
			nskip = 0;
	} else nskip = 0;

	get_name((DIRENTRY *)temp,ssearch);
	sprintf(path,"%c:\\",'A'+dev);
	graf_mouse(BUSY_BEE,NULL);
	clus = ufind(path,0);
	graf_mouse(ARROW,NULL);
	if (clus >= 0) {
		if (nskip == 0) {
			undel[FINDNEXT].ob_state &= ~DISABLED;
			redraw_xobject(undel,FINDNEXT);
		}
		cstart = clus;			/* this works for the special 0 (root) case too! */
		uload_dir(clus);		//rfb: need to test rc!!
		if (clus > 0) {
			d = (DIRENTRY *) (curdirbuf + sizeof(DIRENTRY));
			pstart = get_word((char *)&d->start);
		}
		udesel_all();
		nskip++;
		d = (DIRENTRY *) (curdirbuf + findoff);
		for (j = 0; j < numentry; j++) {
			if (ddir[j].entry == d) {
				ddir[j].undel = 1;
				if (j < display_lines)
					load_ulines(0);
				else load_ulines(j-display_lines/2);	/* scroll by a half screen */
			}
		}
		dirlevel = foundl;
		redraw_object(undel,UDIR);
		redraw_xobject(undel,UBARBACK);
		redraw_object(undel,UDROP);
	} else {
		xform_alert(1,UD_NOMATCH_ALERT);
		undel[FINDNEXT].ob_state |= DISABLED;
		redraw_xobject(undel,FINDNEXT);
	}
}

/***********************************************************************/
PRIVATE short ufind(char *path,short sclus)
{
short st, ncl;
long offset;
char *curpoint, *endp;
char newpath[MAX_PATHNAME];
short ret = -1;

	if (sclus == 0) {				/* do root */
		fdirlevel = 0;
		if ((dirlist[0].dirbuf=malloc(dpb.rdb)) == NULL) {
			error(dpb.rdb,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.rds,1+2*dpb.spf,dirlist[0].dirbuf,DIR)) {
			free(dirlist[0].dirbuf);
			return -1;
		}
		endp = sort_root(dirlist[0].dirbuf,0);		/* don't remove deletions! */
	} else {						/* other directories */
		fdirlevel++;
		st = sclus;
		ncl = 1;
		while (fat16[st] != dpb.bitmask) {
			st = fat16[st];
			if (!st) {
				fdirlevel--;
				return -1;
			}
			ncl++;
		}
		if ((dirlist[fdirlevel].dirbuf=calloc(ncl,dpb.bpc)) == NULL) {
			fdirlevel--;
			error(ncl*dpb.bpc,MEMALLOC,"");
			return -1;
		}
		offset = 0L;
		st = sclus;
		do {
			if (log_read(dev,dpb.spc,logsector(st),dirlist[fdirlevel].dirbuf+offset,DIR)) {
				free(dirlist[fdirlevel].dirbuf);
				memset(&dirlist[fdirlevel],0,sizeof(DIRLIST));
				fdirlevel--;
				return -1;
			}
			st = fat16[st];
			offset += (long) dpb.bpc;
		} while (st != dpb.bitmask);

		endp = sort_dir(dirlist[fdirlevel].dirbuf,ncl,0);	/* don't remove deletions! */
	}

	for (curpoint = dirlist[fdirlevel].dirbuf; curpoint < endp; curpoint += sizeof(DIRENTRY)) {
		if (fattr(curpoint) & FA_VOLUME)
			continue;
		if (*curpoint == DELETION_MARK) {
			get_uinfo((DIRENTRY *)curpoint);
			if (wildc(finfo.name,ssearch)) {
				nfound++;
				if (nfound > nskip) {
					strcpy(undel[UDIR].ob_spec,path);
					findoff = curpoint - dirlist[fdirlevel].dirbuf;	/* remember for caller */
					ret = sclus;
					foundl = fdirlevel;
					break;		/* exit for() loop */
				}
			}
		} else if (fattr(curpoint) & FA_SUBDIR) {
			get_uinfo((DIRENTRY *)curpoint);
			if (strcmp(finfo.name,".") && strcmp(finfo.name,"..")) {
				sprintf(newpath,"%s%s\\",path,finfo.name);
				if ((ret = ufind(newpath,finfo.start)) > 0)
					break;		/* exit for() loop */
			}
		}
	}

	free(dirlist[fdirlevel].dirbuf);
	memset(&dirlist[fdirlevel],0,sizeof(DIRLIST));
	fdirlevel--;

	return ret;
}


/*
 * Wildcard matching routine.  Karl Heuer, 04-Nov-1989.  Public domain.
 *
 * Test whether string s is matched by wildcard pattern p.
 * `?' matches any character, `*' matches any string, `[...]' matches any
 * character in the set, `[!...]' or `[^...]' matches any character not in the
 * set.  Sets may include ranges like `a-z'.  Other characters are matched
 * literally.  Backslash is not magic.
 */
PRIVATE int wildc(char *s,char *p)
{
register char c;

	while ((c = *p++) != '\0') {
		if (c == '?') {
			if (*s++ == '\0')
				return FALSE;
		} else if (c == '[') {
			register short wantit = TRUE;
			register short seenit = FALSE;

			if (*p == '^' || *p == '!') {
				wantit = FALSE;
				++p;
			}
			while ((c = *p++) != ']') {
				if (c == '\0')
					return FALSE;
				if (*p == '-' && p[1] != '\0') {
					if (*s >= c && *s <= p[1])
						seenit = TRUE;
					p += 2;
				} else {
					if (c == *s)
						seenit = TRUE;
				}
			}
			if (wantit != seenit)
				return FALSE;
			++s;
		} else if (c == '*') {
			do {
				if (wildc(s, p))
					return TRUE;
			} while (*s++ != '\0');
			return FALSE;
		} else {
			if (c != *s++)
				return FALSE;
		}
	}
	return (*s == '\0');
}

/*
 *	undelete multiple files
 *
 */
PUBLIC void undelete_files()
{
unsigned short cluster;
short j, txt;
short xoff, yoff, exitobj;
TEDINFO *uname;
long offset;
char *c, *curpoint;
short writedir = 0;
char fname1[MAX_FILENAME];
char fname2[MAX_FILENAME];

	txt = UNDENAME;
	myform_center(undname);
	uname = (TEDINFO *) undname[txt].ob_spec;
	c = uname->te_ptext;
	for (j = 0; j < numentry; j++) {
		if (ddir[j].undel) {
			strcpy(c,"");
			get_uinfo(ddir[j].entry);
			sprintf(uname->te_ptmplt+2,"%-12s",&finfo.name[1]);
			draw_dial(undname,0);
			exitobj = myform_do(undname,&txt,0) & 0x7fff;
			undname[exitobj].ob_state &= ~SELECTED;
			objc_offset(undname,0,&xoff,&yoff);
			objc_draw(undel,0,MAX_DEPTH,xoff-3,yoff-3*rez,undname[0].ob_width+6,undname[0].ob_height+6*rez);
			if (exitobj == UNDNCAN)
				continue;
			if ((strlen(c) != 1) || !isalnum(*c)) {
				j--;							/* retry same name */
				continue;
			}

			get_name(ddir[j].entry,fname1);
			*fname1 = *c;
			for (curpoint = curdirbuf; curpoint < endpoint; curpoint += sizeof(DIRENTRY)) {
				get_name((DIRENTRY *)curpoint,fname2);
				if (strcmp(fname1,fname2) == 0)	/* this name already exists ... */
					break;
			}
			if (curpoint < endpoint) {
				xform_alert(1,UD_EXISTS_ALERT);
				j--;							/* retry same name */
				continue;
			}

			if (finfo.attr == FA_SUBDIR) {
				if (dirok() < 0) {
					xform_alert(1,UD_UNREC_ALERT);
					continue;					/* don't retry this */
				}
			}

			ddir[j].entry->name[0] = *c;
			undelete_one_file(ddir[j].entry);
			writedir = 1;
		}
	}

	if (!writedir)					/* no undeletes done, we can exit now */
		return;

	/*
	 *	we did at least one undelete
	 */
	if (dirlevel > 0) {
		cluster = cstart;
		offset = 0;

		do {
			must_write(dev,dpb.spc,logsector(cluster),curdirbuf+offset,DIR);
			cluster = fat16[cluster];
			offset += dpb.bpc;
		} while (cluster != dpb.bitmask);

	} else {
		must_write(dev,dpb.rds,1+2*dpb.spf,curdirbuf,DIR);
	}

	if (dirlevel == 0)
		uload_dir(0);
	else uload_dir(cstart);
	load_ulines(0);
	udesel_all();
	redraw_xobject(undel,UBARBACK);
	redraw_object(undel,UDROP);
	uw_fat();
}

/*
 *	undelete one file, using mirror information if requested & available
 *
 *	bugs:
 *		. when recovering from mirror, code assumes that the mirrored FAT has no errors
 *		. when doing simple recovery, code assumes at least one free cluster
 */
PRIVATE void undelete_one_file(DIRENTRY *entry)
{
short lastc, num2, unumclus, ucclus, unextc, unum, dotype;

	unumclus = (finfo.size+dpb.bpc-1) >> dpb.cluster_shift;
	ucclus = finfo.start;
	dotype = utype;					/* default = user request */

	if (utype == MIR) {
		if (mirrorfat[ucclus] == 0) {	/* start cluster was free in mirror ... */
			dotype = SIM;
		} else {
			while(fat16[ucclus]) {			/* look for first free cluster in current FAT */
				ucclus = mirrorfat[ucclus];	/* while following chain in mirror */
				if (ucclus == dpb.bitmask) {/* end of chain: start of deleted file has been reused ... */
					dotype = SIM;
					break;
				}
			}
		}
	}

	unum = 0;						/* count clusters written */
	if (dotype == MIR) {
		putword(&entry->start,ucclus);
		for (unextc = mirrorfat[ucclus]; unextc != dpb.bitmask; unextc = mirrorfat[unextc]) {
			if (fat16[unextc] == 0) {
				fat16[ucclus] = unextc;
				ucclus = unextc;
				unum++;
			}
		}
		fat16[ucclus] = dpb.bitmask;
		unum++;
	}

	if (dotype == SIM) {
		for (num2 = 0; fat16[ucclus]; num2++) {	/* find first free cluster starting at 'ucclus' */
			ucclus++;							//rfb: we assume there is one ...
		}
		putword(&entry->start,ucclus);
		for (unextc = ucclus+1; num2 < unumclus-1; unextc++, num2++) {
			if (fat16[unextc] == 0) {		/* this cluster is free: */
				fat16[ucclus] = unextc;			/* set prev -> this */
				lastc = ucclus;					/* remember prev */
				ucclus = unextc;				/* set prev = this */
				unum++;
			}
		}

		/*
		 *	i don't understand the following test.
		 *	as far as I can see, fat16[ucclus] can only be non-zero if we didn't
		 *	find ANY free clusters, in which case lastc isn't valid anyway ...
		 *
		 *	maybe we should do an assert() or equivalent here ...
		 */
		if (fat16[ucclus]) {
			fat16[lastc] = dpb.bitmask;
		} else {
			fat16[ucclus] = dpb.bitmask;
			unum++;
		}
	}

	if ((finfo.attr != FA_SUBDIR) && (unum != unumclus))
		putlong(&entry->size,(long)dpb.bpc*unum);
}

/*
 *	initialise for undelete by scanning the root directory of the specified device
 */
PUBLIC int init_uroot(short device,int draw)
{
	cleanup_undelete();		/* need to do this now in case we were already in undelete mode,    */
							/* and have just switched modes from Simple to Mirror or vice versa */
	dev = device;
	if (get_mybpb(&dpb,dev) < 0) {
		xform_alert(1,GEN_BPB_ALERT);
		return -1;
	}

	sprintf(undel[UDIR].ob_spec,"%c:\\",'A'+dev);

	if (dpb.isfat16) {								/* fat16 */
		if ((fat16=malloc(dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.spf,1,fat16,FAT))
			return -1;
		swab((char *)fat16,(char *)fat16,dpb.bpf);
	} else {										/* fat12 */
		if ((fat12=malloc(dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.spf,1,fat12,FAT))
			return -1;
		if ((fat16=calloc(2,dpb.bpf)) == NULL) {
			error(2*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		fat12_to_fat16(fat12,fat16);
	}

	utype = SIM;								/* default is simple undelete */
	if (undel[UDMIR].ob_state & SELECTED) {		/* user wants mirror undelete */
		mirrorfat = load_mirror();
		if (!mirrorfat) {						/* mirror file is unavailable */
			xform_alert(1,UD_NOMIR_ALERT);
			if (draw) {
				objc_xdsel(undel,UDMIR);
				objc_xsel(undel,UDSIM);
			} else {
				undel[UDSIM].ob_state |= SELECTED;
				undel[UDMIR].ob_state &= ~SELECTED;
			}
		} else utype = MIR;						/* mirror file found */
	}

	pstart = -1;
	cstart = dirlevel = 0;
	memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));

	if (uload_dir(0) < 0)
		return -1;

	load_ulines(0);
	udesel_all();
	if (draw) {
		redraw_object(undel,UDIR);
		redraw_xobject(undel,UBARBACK);
		redraw_object(undel,UDROP);
	}

	return 0;
}

/*
 *	load saved FAT into memory
 *	returns pointer to malloc'd memory
 */
PRIVATE unsigned short *load_mirror()
{
short from;
char mirfile[MAX_PATHNAME];
long count, rc;
unsigned short *mfat16;
char *mfat12;

	sprintf(mirfile,"%s" MIRROR_FILE,opt.filepath,dev+'A');
	if ((rc=Fopen(mirfile,0)) < 0L)			/* mirror file is unavailable */
		return NULL;

	from = (short)rc;

	if (dpb.isfat16) {						/* fat16 */
		if ((mfat16=malloc(dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			Fclose(from);
			return NULL;
		}
		if (Fseek((long)dpb.bps,from,FSEEK_SET) == dpb.bps) {		/* new with Mirror 1.1 */
			if ((count=Fread(from,dpb.bpf,mfat16)) == dpb.bpf) {
				swab(mfat16,mfat16,dpb.bpf);
				Fclose(from);
				return mfat16;
			}
		}
	} else {								/* fat12 */
		if ((mfat16=calloc(3,dpb.bpf)) == NULL) {
			error(3*dpb.bpf,MEMALLOC,"");
			Fclose(from);
			return NULL;
		}
		mfat12 = (char *)mfat16 + 2 * dpb.bpf;

		if (Fseek((long)dpb.bps,from,FSEEK_SET) == dpb.bps) {		/* new with Mirror 1.1 */
			if ((count=Fread(from,dpb.bpf,mfat12)) == dpb.bpf) {
				fat12_to_fat16(mfat12,mfat16);
				Fclose(from);
				return mfat16;
			}
		} else count = -1;
	}

	error(count,READ,mirfile);
	Fclose(from);
	free(mfat16);
	return NULL;
}

/*
 *	write updated FAT(s)
 */
PRIVATE void uw_fat()
{
char *fatptr;

	if (dpb.isfat16) {						/* fat16 */
		swab(fat16,fat16,dpb.bpf);
		fatptr = (char *)fat16;
	} else {								/* fat12 */
		fat16_to_fat12(fat16,fat12);
		fatptr = fat12;
	}

	must_write(dev,dpb.spf,1,fatptr,FAT);
	if (dpb.nfat == 2)
		must_write(dev,dpb.spf,1+dpb.spf,fatptr,FAT);

	/* new */
	if (dpb.isfat16)
		swab(fat16,fat16,dpb.bpf);

	_mediach(dev);
}

/*
 *	basic validity check of contents of directory
 *	returns 0 if ok, -1 if error
 */
PRIVATE int dirok()
{
char *buff;
char dname[MAX_FILENAME];
DIRENTRY *temp;
short j;
int fail = 0;

	if ((buff=calloc(1,dpb.bpc)) == NULL) {
		error(dpb.bpc,MEMALLOC,"");
		return -1;
	}
	if (log_read(dev,dpb.spc,logsector(finfo.start),buff,DIR)) {
		free(buff);
		return -1;
	}
	temp = (DIRENTRY *)buff;
	get_name(temp,dname);
	j = get_word((char *)&temp->start);
	if ((strcmp(dname,".") != 0) || (j != finfo.start))
		fail = -1;

	temp++;
	get_name(temp,dname);
	if (strcmp(dname,"..") != 0)
		fail = -1;

	free(buff);

	return fail;
}

/*
 *	blit from one grect to another
 */
PUBLIC void blit(GRECT *src,GRECT *dst)
{
short xy[8];
int was_hidden;

	set_clip();
	grect_to_array(src,&xy[0]);
	grect_to_array(dst,&xy[4]);
	was_hidden = mouse_off();
	vswr_mode(myhandle,MD_REPLACE);
	vro_cpyfm(myhandle,S_ONLY,xy,&screen,&screen);
	if (!was_hidden)
		mouse_on();
}

/*
 *	checks for garbage in a (deleted) directory entry
 *		returns 0 if OK, count of errors otherwise
 *		NOTE: we could just return as soon as we find an error ...
 */
PRIVATE int garbage(DIRENTRY *dir)
{
char *temp;
int i, n, ngarbage = 0;
short start;
long fsize;

	if (dir->attr & BADATTR_MASK)
		ngarbage++;

	for (i = 0, n = 0, temp = dir->reserved; i < sizeof(dir->reserved); i++)
		if (*temp++)
			n++;
	if (n)
		ngarbage++;

	for (i = 1, temp = (char *)dir; i < NAME_LENGTH; i++)
		if (!ascii[*temp++])
			ngarbage++;

	start = get_word((char *)&dir->start);
	fsize = get_long((char *)&dir->size);
	if (start || fsize) {
		if (!valid_cluster(start))
			ngarbage++;
		if ((fsize < 0L) || (fsize > dpb.tbyte))
			ngarbage++;
	}

	return ngarbage;
}
