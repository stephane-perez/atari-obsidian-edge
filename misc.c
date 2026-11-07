/*
 *	misc.c: contains many utility functions
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

/*
 *	version 2.10 (december/2002)		roger burrows
 *		a highly cleaned up and reorganised version of
 *		the DE v2.04 source; added the following public
 *		routines:
 *			max_allocatable_clusters()
 *			decode_date_time()
 *			change_mode()
 *			CDmap()
 *
 *	version 2.53 (july/2003)			roger burrows
 *		replaced max_allocatable_clusters() with allocate_max_units(); this
 *		should circumvent a very peculiar incident noted by Wayne Martz:
 *			do_defrag() called max_allocatable_clusters(), which returned a
 *			value of 256 because it had successfully malloced (then freed)
 *			256*(2*dpb.bpc+4) bytes.  however, when do_defrag() immediately
 *			tried to malloc the same number of bytes itself, the malloc
 *			failed !?!  this is probably something strange in the malloc code,
 *			but since the new routine doesn't free up the memory after
 *			allocating it (and therefore the caller doesn't have to malloc it
 *			again), we should avoid this problem ...
 *
 *	version 2.54 (august/2003)			roger burrows
 *		. add DEMO support to get_src_dest() [used by copy image & copy defrag]
 *
 *	version 2.57 (january/2017)			roger burrows
 *		prepare for public release
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <cookie.h>
#include <ctype.h>
#include <dos.h>
#include <extendos.h>
#include <portab.h>
#include <string.h>
#include <vdi.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"

#define MODULE_NUMBER	9		/* used by error() macro */


/*
 * date/time display format stuff
 */
#define _IDT_COOKIE		0x5f494454		/* '_IDT' */
#define	_IDT_MDY		0				/* date format:	month-day-year */
#define	_IDT_DMY		1				/*				day-month-year */
#define	_IDT_YMD		2				/*				year-month-day */
#define	_IDT_YDM		3				/*				year-day-month */
#define	_IDT_12H		0				/* time format:	12-hour */
#define	_IDT_24H		1				/*				24-hour */

#define	DEFAULT_DT_SEPARATOR	'/'
#define DEFAULT_US_FORMAT	((_IDT_12H<<12) + (_IDT_MDY<<8) + DEFAULT_DT_SEPARATOR)
#define DEFAULT_UK_FORMAT	((_IDT_12H<<12) + (_IDT_DMY<<8) + DEFAULT_DT_SEPARATOR)

#define _hz_200			(*((long *) 0x4ba))

#define MAX_UNITS		256				/* arbitrary: see allocate_max_units() */



/*************externally referenced**************/

extern short dev;
extern short myhandle, nplanes, rez, ychar;
extern short win1, windhidden;
extern short fullx, fully, fullw, fullh;
extern short workx, worky, workh, workw;
extern char *fat12;
extern unsigned short *fat16;
extern STATUS status;
extern OPT opt;
extern OBJECT *bar, *bottom, *dup, *menu, *pcsel, *text;

/*
 *	private variables
 */
MLOCAL int ceon = TRUE;
MLOCAL void (*cehandler)();

MLOCAL short barx, bary, totbar;	/* for managing the task bar */
MLOCAL long barmax;

MLOCAL int m_hidden = FALSE;		/* remember current mouse state */


/*
 *	function prototypes
 */
PRIVATE long nullfunction(long error);

/*
 *	private macros
 */
#define sup_on()	{ void *savessp = Super(0L);
#define sup_off()	Super(savessp); }

/*
 *	allocates max available memory (in 'unit_size' units)
 *	returns # units allocated & ptr to allocated memory
 */
PUBLIC long allocate_max_units(int min_units,long unit_size,char **ptr)
{
long n;

	for (n = MAX_UNITS; n >= min_units; n = (n*3)/4)
		if (*ptr=malloc(n*unit_size))
			return n;

	return 0L;		/* *ptr is already NULL ... */
}

/*
 *	returns number of floppies available
 */
PUBLIC short nflop(void)
{
	return get_sys(_nflops);
}

/*
 *	get a short system variable
 */
PUBLIC short get_sys(short *p)
{
short n;

	sup_on();
	n = *p;
	sup_off();

	return n;
}

/*
 *	disable critical error handler
 *	(points vector to null function)
 */
PUBLIC void ce_off(void)
{
	if (ceon) {
		cehandler = Setexc(CRIT_ERR_VEC,(void (*)())&nullfunction);
		ceon = FALSE;
	}
}

/*
 *	we actually get passed a WORD, but since we're using default 32-bit ints,
 *	we pretend that it's 32 bits & then just use the first 16 ...
 */
PRIVATE long nullfunction(long error)
{
	return error>>16;
}

/*
 *	enable critical error handler
 *	(restores vector to point to original function)
 */
PUBLIC void ce_on(void)
{
	if (!ceon) {
		Setexc(CRIT_ERR_VEC,cehandler);
		ceon = TRUE;
	}
}

/*
 *	hide mouse pointer, return prior hidden status
 */
PUBLIC int mouse_off(void)
{
int hidden = m_hidden;

	if (!m_hidden) {
		graf_mouse(M_OFF,NULL);
		m_hidden = TRUE;
	}
	return hidden;
}

/*
 *	unhide mouse pointer, return prior hidden status
 */
PUBLIC int mouse_on(void)
{
int hidden = m_hidden;

	if (m_hidden) {
		graf_mouse(M_ON,NULL);
		m_hidden = FALSE;
	}
	return hidden;
}

/*
 *	store an unsigned short in Intel byte order
 */
PUBLIC void putword(IWORD *w,unsigned short val)
{
	w->b[0] = val & 0xff;
	w->b[1] = (val>>8) & 0xff;
}

/*
 *	store a long in Intel byte order
 */
PUBLIC void putlong(ILONG *l,long val)
{
	l->b[0] = val & 0xff;
	l->b[1] = (val>>8) & 0xff;
	l->b[2] = (val>>16) & 0xff;
	l->b[3] = (val>>24) & 0xff;
}

/*
 *	convert an unsigned short from Intel byte order
 */
PUBLIC unsigned short get_word(char *in)
{
unsigned short n;

	n = *in++;
	n += *in << 8;

	return n;
}

/*
 *	convert a long from Intel byte order
 */
PUBLIC long get_long(char *in)
{
long n;

	n = *in++;
	n += *in++ << 8;
	n += *in++ << 16;
	n += *in << 24;

	return n;
}

/****************************************************************/
PUBLIC void blank(short color)
{
short temp[4];
int extra;

	set_clip();
	vswr_mode(myhandle,MD_REPLACE);
	vsf_interior(myhandle,FIS_PATTERN);
	temp[0] = workx;
	temp[1] = worky;
	temp[2] = workx + workw - 1;
	if (bottom[0].ob_flags & FL3DMASK)
		extra = 4;
	else extra = 2;
	temp[3] = worky + workh - 4 * ychar - extra;

	vsf_style(myhandle,8);
	if (nplanes == 1)
		vsf_color(myhandle,WHITE);
	else vsf_color(myhandle,color);

	v_bar(myhandle,temp);
}


/************************************************************************/
PUBLIC void refresh_screen(OBJECT * tree)
{
GRECT r;

	r.g_x = tree->ob_x - 3;
	r.g_y = tree->ob_y - 3 * rez;
	r.g_w = tree->ob_width + 6;
	r.g_h = tree->ob_height + 6 * rez;
	form_dial(FMD_FINISH,0,0,0,0,r.g_x,r.g_y,r.g_w,r.g_h);
}


/************************************************************************/
/* the following routines manipulate the task bar display				*/
/************************************************************************/
PUBLIC void init_bar(long max)
{
	barmax = max;
	objc_offset(bar,TOTALBAR,&barx,&bary);
	totbar = bar[TOTALBAR].ob_width;
}

PUBLIC void start_bar(char *str1)
{
	strcpy(bar[BARTOP].ob_spec,str1);
	bar[TASKBAR].ob_width = 0;
	draw_dial(bar,0);
}

PUBLIC void update_bar(long num)
{
short old, new;

	if (num > barmax) {
		debug_alert("[1][Progress bar > 100%|Please report to Anodyne!][ Fix ]");
		num = barmax;
	}
	old = bar[TASKBAR].ob_width;
	new = (totbar * num + barmax/2) / barmax;		/* new width */
	if (new > old+2) {
		bar[TASKBAR].ob_width = new;
		objc_draw(bar,TASKBAR,MAX_DEPTH,barx+old,bary-3*rez,new-old+3,bar[TASKBAR].ob_height+6*rez);
	}
}

PUBLIC void update_barmsg(char *strng)
{
	strcpy(bar[BARTOP].ob_spec,strng);
	redraw_object(bar,BARTOP);
}

PUBLIC void full_bar(void)
{
short old;

	old = bar[TASKBAR].ob_width;
	bar[TASKBAR].ob_width = totbar;
	objc_draw(bar,TASKBAR,MAX_DEPTH,barx+old,bary-3*rez,totbar-old+3,bar[TASKBAR].ob_height+6*rez);
}

PUBLIC void wrap_bar(void)
{
	refresh_now(bar,0);
}

/*
 *	the defined way of switching modes
 *
 *	functions:
 *		call cleanup() routine for existing mode ...
 *		toggle the 'bottom' indicator to show the new mode
 *		set new window title/info lines
 *		modify mode
 */
PUBLIC void change_mode(int new_mode)
{
int icon;
static char title[MAX_DISPLAYSIZE];		/* static because it's a window title */

	/*
	 *	first, handle any cleanup required
	 *
	 *	at this time, we do not call any cleanup routine for entry or exit from
	 *	MAKEVAL, CHECKVAL, or ESCSI, since these are not 'persistent' modes.
	 *	for entry into IDLE, which is only done at termination, we call *all*
	 *	cleanup routines.
	 */
	switch(new_mode) {
	case IDLE:
		cleanup_allinfo();		/* the sequence is immaterial (Sir James Immaterial) */
		cleanup_diskinfo();
		cleanup_diskmap();
		cleanup_undelete();
		break;
	case DISKINFO:
	case DISKMAP:
	case ALLINFO:
	case UNDELETE:
		switch(status.action) {
		case DISKINFO:
			cleanup_diskinfo();
			break;
		case DISKMAP:
			cleanup_diskmap();
			break;
		case ALLINFO:
			cleanup_allinfo();
			break;
		case UNDELETE:
			cleanup_undelete();
			break;
		}
	}

	/*
	 *	next, toggle the 'bottom' indicator if required
	 */
	if (!windhidden) {
		icon = 0;
		switch(new_mode) {
		case DISKINFO:
			icon = BDINFO;
			break;
		case DISKMAP:
			icon = BFRAGMAP;
			break;
		case ALLINFO:
			icon = BALLINFO;
			break;
		case UNDELETE:
			icon = BUNDEL;
			break;
		}
		if (icon)
			toggle_bottom(icon);
	}

	/*
	 *	set appropriate title/info
	 */
	switch(new_mode) {
	case DISKINFO:
		sprintf(title,text[DISKINFO_TITLE].ob_spec,dev+'A');
		wind_set(win1,WF_NAME,ADDR(title));
		wind_set(win1,WF_INFO,ADDR(text[DISKINFO_INFO].ob_spec));
		break;
	case DISKMAP:
		sprintf(title,text[DISKMAP_TITLE].ob_spec,'A'+dev);
		wind_set(win1,WF_NAME,ADDR(title));
		wind_set(win1,WF_INFO,ADDR(text[DISKMAP_INFO].ob_spec));
		break;
	case ALLINFO:
		wind_set(win1,WF_NAME,ADDR(text[ALLINFO_TITLE].ob_spec));
		wind_set(win1,WF_INFO,ADDR(text[ALLINFO_INFO].ob_spec));
		break;
	case UNDELETE:
		sprintf(title,text[UNDELETE_TITLE].ob_spec,dev+'A');
		wind_set(win1,WF_NAME,ADDR(title));
		wind_set(win1,WF_INFO,ADDR(text[UNDELETE_INFO].ob_spec));
		break;
	}

	status.action = new_mode;
}

/*
 *	rebuild window according to current 'action'
 *
 *	'device' is the partition that has changed (-1 for no change)
 */
PUBLIC void force_update(int device)
{
	switch(status.action) {
	case DISKINFO:
		if (device == dev)
			status.infodev = -1;
		do_diskinfo();
		break;
	case DISKMAP:
		if (device == dev)
			status.mapdev = -1;
		do_diskmap();
		break;
	case UNDELETE:
		init_undel();
		break;
	default:
		do_chart(device);
		break;
	}
}

/********************************************************/
PUBLIC void delay(long time)
{
long limit;

	sup_on();
	limit = time + _hz_200;
	while (limit > _hz_200)
		;
	sup_off();
}

/***************************************************************************/
PUBLIC void browse(void)
{
char fpath[MAX_PATHNAME], fmask[MAX_FILENAME];
short dummy;

	_mediach(dev);
	sprintf(fpath,"%c:\\*.*",'A'+dev);
	strcpy(fmask,"");
	fsel_exinput(fpath,fmask,&dummy,text[BROWSE_FSEL].ob_spec);
	form_dial(FMD_FINISH,0,0,0,0,fullx,fully,fullw,fullh);
}

/*
 *	handle quit/pause/resume keys
 */
PUBLIC int get_key()
{
short which, kr;
short dummy;

	which = evnt_multi(MU_KEYBD | MU_TIMER,
					   0, 0, 0,
					   0, 0, 0, 0, 0,
					   0, 0, 0, 0, 0,
					   NULL, 0, 0,		/* Zero timer delay */
					   &dummy, &dummy, &dummy, &dummy, &kr, &dummy);

	if (which & MU_KEYBD) {
		if ((kr & 0xff) == 3)					//MAGNUM	control-c
			return -1;
		if ((kr & 0xff) == 19) {				//MAGNUM	control-q
			while (get_key() > 0)
				;
		} else if ((kr & 0xff) == 17) {			//MAGNUM	control-s
			return 0;
		}
	}

	return 1;
}


/*********************************************/
PUBLIC int get_newname(char *fname,char *path,char *name)
{
TEDINFO *tdup;
char *temp1, *temp2;
int i, n, ret, was_hidden;

	myform_center(dup);
	tdup = (TEDINFO *) dup[DUPNAMEE].ob_spec;
	strncpy(tdup->te_ptext,name,NAME_LENGTH);
	strcpy(dup[DUPFILE].ob_spec,fname);
	sprintf(dup[DUPDIR].ob_spec,"%-56s",path);
	save_screen();
	was_hidden = mouse_on();

	while(TRUE) {
		draw_dial(dup,0);
		n = edit_form_do(dup,DUPNAMEE) & 0x7fff;
		dup[n].ob_state &= ~SELECTED;
		if (n == DUPDEL) {
			*name = DELETION_MARK;
			ret = 0;
			break;
		}
		if (memcmp(tdup->te_ptext,name,NAME_LENGTH) == 0)
			continue;
		n = strlen(tdup->te_ptext);
		if (n == 0)
			continue;
		temp1 = name;
		temp2 = tdup->te_ptext;
		for (i = 0; i < n; i++)
			*temp1++ = *temp2++;
		for ( ; i < NAME_LENGTH; i++)
			*temp1++ = ' ';
		ret = 1;
		break;
	}

	if (was_hidden)
		mouse_off();
	restore_screen();

	return ret;
}

/***************************/
/*
 *	prompt for source/destination; if WARN mode, asks user for confirmation
 *
 *	returns -1 if user cancels when warned, otherwise (src<<8)+dest
 */
PUBLIC int get_src_dest()
{
int i, obj, src, dest;

	for (i = 0; i < NUM_DEVICES; i++)		/* deselect all objects */
		pcsel[PCA+i].ob_state &= ~SELECTED;
	myform_center(pcsel);

	strcpy(pcsel[PCNAME].ob_spec,text[PCSEL_SOURCE].ob_spec);
	if ((obj=handle_dial(pcsel,0,0,1)) <= 0)
		return -1;
	src = obj - PCA;
	pcsel[obj].ob_state &= ~SELECTED;
	pcsel[obj].ob_state |= DISABLED;		/* prevent reselection of src */
	pcsel[obj].ob_flags &= ~TOUCHEXIT;

	strcpy(pcsel[PCNAME].ob_spec,text[PCSEL_DEST].ob_spec);
	if ((obj=handle_dial(pcsel,0,0,1)) <= 0)
		return -1;
	dest = obj - PCA;
	pcsel[obj].ob_state &= ~SELECTED;
	pcsel[src+PCA].ob_state &= ~DISABLED;	/* reset src for next time */
	pcsel[src+PCA].ob_flags |= TOUCHEXIT;

	if (opt.warn)
		if (xform_alert(2,CP_WARN_ALERT,dest+'A',src+'A',dest+'A') == 2)
			return -1;

#ifdef DEMO
	form_alert(1,DEMO_UNAVAIL);
	return -1;
#else
	return (src<<8) + dest;
#endif
}

/*
 *	decode_date_time - generate string with date/time in specified format
 *
 *	if the _IDT cookie is present, it will override the specified format;
 *	note that, although dynamic changes to 'format' are possible, dynamic
 *	changes to the _IDT cookie are *not* monitored
 *
 *	returns length of formatted string
 */
PUBLIC int decode_date_time(char *s,unsigned short date,unsigned short time,int format)
{
static long idt_value = -1L;	/* date/time display format */
static int idt_present = -1;
int year, month, day, hour, minute;
char d[20], t[20], date_sep, ampm;

	if (idt_present < 0) {						/* first time */
		if (getcookie(_IDT_COOKIE,&idt_value) == 0)	/* try to get _IDT format */
			idt_present = 0;						/* failed ... */
		else idt_present = 1;
	}

	if (!idt_present)
		idt_value = (format==US)?DEFAULT_US_FORMAT:DEFAULT_UK_FORMAT;

	date_sep = idt_value & 0xff;			/* date separator */
	if (!isprint(date_sep))
		date_sep = DEFAULT_DT_SEPARATOR;	/* default if all else fails */

	year = (1980 + (date>>9)) % 100;		/* two-digit year ... */
	month = (date>>5) & 0x0f;
	day = date & 0x1f;

	switch((idt_value>>8)&0x03) {
	case _IDT_MDY:
		sprintf(d,"%02d%c%02d%c%02d ",month,date_sep,day,date_sep,year);
		break;
	case _IDT_DMY:
		sprintf(d,"%02d%c%02d%c%02d ",day,date_sep,month,date_sep,year);
		break;
	case _IDT_YDM:
		sprintf(d,"%02d%c%02d%c%02d ",year,date_sep,day,date_sep,month);
		break;
	default:						/* i.e. _IDT_YMD or some kind of bug ... */
		sprintf(d,"%02d%c%02d%c%02d ",year,date_sep,month,date_sep,day);
		break;
	}

	hour = time >> 11;
	minute = (time>>5) & 0x3f;

	switch((idt_value>>12)&0x01) {
	case _IDT_12H:
		if (hour < 12)				/* figure out am/pm */
			ampm = 'a';
		else ampm = 'p';
		if (hour > 12)				/* figure out noon/midnight */
			hour -= 12;
		else if (hour == 0)
			hour = 12;
		sprintf(t,"%02d:%02d%cm",hour,minute,ampm);
		break;
	default:						/* i.e. _IDT_24H or some kind of bug ... */
		sprintf(t,"%02d:%02d  ",hour,minute);
		break;
	}

	strcpy(s,d);
	strcat(s,t);
	return strlen(s);
}

/*
 *	return bitmap of CD devices
 *
 *	this routine returns a bitmap of CD devices controlled by ExtenDOS/MetaDOS.
 *	the bitmap is organised like the bitmap returned by Drvmap(),
 *	i.e. bit 0 => 'A', bit 1 = > 'B' etc
 */
PUBLIC unsigned long CDmap()
{
struct edinit_return init;
struct _met_ext *m;
unsigned long bit, bitmap = 0L;
char *p;
int i;

	init.metados_id = NULL;		/* initialise before calling ExtenDOS (in case it's not there) */

	EDinit(&init);				/* get (indirect) ptr to MetaDOS BOS/DOS mapping stuff */

	if (init.metados_id) {		/* i.e. DOS/BOS mapping supported (MetaDOS v2.30, EPro v2.2) */
		m = init._met_ptr;
		if (m->_met_id == _MET_MAGIC) {
			for (i = 0, p = m->metados_map, bit = 1L; i < 32; i++, p++, bit <<= 1)
				if ((*p >= 'A') && (*p <= 'Z'))
					bitmap |= bit;
		}
	}

	return bitmap;
}
