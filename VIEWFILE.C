/*
 *	viewfile.c: performs file viewing function
 *
 *	new in edge 2.50
 *
 *	version 2.50 (january/2003)	roger burrows
 *		allow file scrolling in both directions without
 *		loading entire file into memory ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <limits.h>
#include <portab.h>
#include <string.h>
#include <vdi.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "keycodes.h"
#include "proto.h"

#define MODULE_NUMBER	16		/* used by error() macro */

/*
 *	local defines
 */
#define QUIT_VIEW	-1			/* returned by get_fkey() */
#define LINE_UP		1
#define LINE_DOWN	2
#define PAGE_UP		3
#define PAGE_DOWN	4
#define TOP_OF_FILE	5

#define LINES_PER_BLOCK	512		/* for TEXTBLOCK (big enough for largest HLP) */

typedef struct textblock TEXTBLOCK;
struct textblock {
	TEXTBLOCK *tb_next;
	TEXTBLOCK *tb_prev;
	long tb_offset[LINES_PER_BLOCK];
};


/*
 *	external references
 */
extern short workx, worky, workh, workw;
extern short ychar, win1, myhandle;
extern OBJECT *bottom, *chart, *dinfo, *dmap, *menu, *mirror, *multi, *prefs, *printstr, *text, *undel;
extern char app_path[MAX_PATHNAME];


/*
 *	local variables
 */
MLOCAL int pagedepth = -1;			/* lines per page */
MLOCAL long filelines;				/* #lines in file */
MLOCAL long curline;				/* current position in file (as line#) */
MLOCAL long hwm;					/* furthest we have read so far in file */
MLOCAL TEXTBLOCK *root;
MLOCAL int internal_error;


/*
 *	function prototypes
 */
PRIVATE int do_line(FILE *f,long linenum);
PRIVATE void do_page(FILE *f,int lines);
PRIVATE void free_textblock(void);
PRIVATE int get_fkey(void);
PRIVATE int moveto(FILE *f,long linenum);
PRIVATE int update_textblock(FILE *f);
PRIVATE int vgets_nl(char *temp,FILE *t);


/*
 *	view a file
 *		input:	filename (NULL => prompt for file)
 */
PUBLIC void view_file(char *namein,short draw)
{
int rc;
FILE *f;
static GRECT rect1, rect2;				/* we only evaluate them once */
char fname[MAX_PATHNAME];
static char title[MAX_DISPLAYSIZE];		/* it's a window title */
long topline;							/* line # of line displayed at top of page */
long saveline;

	if (pagedepth < 0) {
		pagedepth = (workh - 4 * ychar) / ychar;		//MAGNUM
		if (bottom[0].ob_flags & FL3DMASK)
			pagedepth--;
		rect1.g_x = workx;			/* all but top line */
		rect1.g_y = worky + ychar;
		rect1.g_w = workw;
		rect1.g_h = ychar * (pagedepth - 1);
		rect2.g_x = workx;			/* all but bottom line */
		rect2.g_y = worky;
		rect2.g_w = workw;
		rect2.g_h = ychar * (pagedepth - 1);
	}


	if (namein)
		strcpy(fname,namein);
	else if (get_existing_fname(fname,app_path,"",text[VIEW_FSEL].ob_spec) == 0)
		return;

	if ((f=fopen(fname,"ra")) == NULL) {
		error(-1L,OPEN,fname);
		return;
	}

	sprintf(title,text[VIEW_TITLE].ob_spec,fname);
	wind_set(win1,WF_NAME,ADDR(title));
	wind_set(win1,WF_INFO,ADDR(text[VIEW_INFO].ob_spec));

	wind_update(BEG_UPDATE);
	wind_update(BEG_MCTRL);
	mouse_off();
	set_clip();
	rc = PAGE_DOWN;				/* force new page first time thru */
	topline = -pagedepth;
	curline = 0L;
	hwm = -1L;
	filelines = LONG_MAX;
	internal_error = FALSE;
	do {
		switch(rc) {
		case LINE_UP:
			if (topline > 0) {
				blit(&rect2,&rect1);
				saveline = curline;				/* remember where we are */
				topline--;
				if (moveto(f,topline) < 0)
					internal_error = TRUE;
				do_line(f,0);
				if (topline+pagedepth < filelines)
					saveline--;					/* reset to one line earlier */
				if (moveto(f,saveline) < 0)
					internal_error = TRUE;
			}
			break;
		case LINE_DOWN:
			if (curline < filelines) {
				blit(&rect1,&rect2);
				topline++;
				do_line(f,pagedepth-1);
			}
			break;
		case PAGE_UP:
			if (topline > 0) {
				topline = max(0,topline-pagedepth);
				if (moveto(f,topline) < 0)
					internal_error = TRUE;
				do_page(f,pagedepth);
			}
			break;
		case PAGE_DOWN:
			if (curline < filelines) {
				topline += pagedepth;
				do_page(f,pagedepth);
			}
			break;
		case TOP_OF_FILE:
			if (topline > 0) {
				topline = 0;
				if (moveto(f,topline) < 0)
					internal_error = TRUE;
				do_page(f,pagedepth);
			}
			break;
		}
	} while(((rc=get_fkey()) != QUIT_VIEW) && !internal_error);

	mouse_on();
	wind_update(END_MCTRL);
	wind_update(END_UPDATE);
	fclose(f);

	free_textblock();

	if (internal_error)
		error(-1L,READ,fname);

	if (draw)
		force_update(-1);
}

/*
 *	read & display a line of text
 */
PRIVATE int do_line(FILE *f,int linenum)
{
char temp[MAX_DISPLAYSIZE], *p;

	if (vgets_nl(temp,f) < 0)
		return -1;

	for (p = temp; *p; p++)
		;
	for ( ; p < temp+sizeof(temp)-1; )
		*p++ = ' ';
	*p = '\0';
	v_gtext(myhandle,workx,worky+linenum*ychar,temp);

	return 0;
}

/*
 *	read & display a page of text
 */
PRIVATE void do_page(FILE *f,int lines)
{
int i;

	blank(0);
	for (i = 0; i < lines; i++)
		if (do_line(f,i) < 0)
			return;
}

/*
 *	free textblock memory
 */
PRIVATE void free_textblock(void)
{
TEXTBLOCK *current, *next;

	for (current = root; current; current = next) {
		next = current->tb_next;
		free(current);
	}
	root = NULL;
}

/*
 *	get a key / mouseclick from the user
 */
PRIVATE int get_fkey()
{
short which, keycode, button;
short dummy;

	which = evnt_multi(MU_KEYBD | MU_BUTTON,
						256+NUM_CLICKS, LEFT_BUTTON+RIGHT_BUTTON, 0,	/* look for left OR right button */
						0, 0, 0, 0, 0,
						0, 0, 0, 0, 0,
						NULL, 0, 0,
						&dummy, &dummy, &button, &dummy, &keycode, &dummy);

	if (which & MU_KEYBD) {
		if ((keycode & 0xff) == 3)	/* control-C */
			return QUIT_VIEW;
		switch(keycode) {
		case ESC:
			return QUIT_VIEW;
		case UP_ARROW:
			return LINE_UP;
		case DN_ARROW:
		case RETRN:
		case ENTR:
			return LINE_DOWN;
		case SUP_ARROW:
			return PAGE_UP;
		case SDN_ARROW:
		case SPACEBAR:
			return PAGE_DOWN;
		case HOME:
			return TOP_OF_FILE;
		}
	}

	if (which & MU_BUTTON) {
		if (button == LEFT_BUTTON)
			return LINE_DOWN;
		return PAGE_DOWN;
	}

	return 0;		/* uninteresting ... */
}

/*
 *	move to specified line number within file
 */
PRIVATE int moveto(FILE *f,long linenum)
{
long offset;
int blocknum, blockpos;
TEXTBLOCK *t;

	blocknum = linenum / LINES_PER_BLOCK;
	blockpos = linenum - (blocknum * LINES_PER_BLOCK);

	for (t = root; (blocknum > 0) && t; blocknum--, t = t->tb_next)
		;
	if (t) {
		offset = t->tb_offset[blockpos];
		if (offset >= 0) {
			if (fseek(f,offset,SEEK_SET) >= 0) {
				curline = linenum;
				return 0;
			}
		}
	}

	//	form_alert(1,"[3][moveto() failed!][Ouch]");
	return -1;
}

/*
 *	update entry in TEXTBLOCK chain
 *		returns	0	ok
 *				-1	error occurred
 */
PRIVATE int update_textblock(FILE *f)
{
long offset, *lptr;
int n, blocknum, blockpos;
TEXTBLOCK *current, *prev;

	offset = ftell(f);
	if (offset < 0) {
		//	form_alert(1,"[3][ftell() failed!][Ouch]");
		return -1;
	}

	blocknum = curline / LINES_PER_BLOCK;
	blockpos = curline - (blocknum * LINES_PER_BLOCK);

	for (n = 0, current = prev = root; (n < blocknum) && current; n++, prev = current, current = current->tb_next)
		;
	if (n <= blocknum) {
		if ((n < blocknum) && (blockpos > 0)) {
			//	form_alert(1,"[3][bad curline!][Ouch]");
			return -1;
		}
	}
	if (!current) {
		current = malloc(sizeof(TEXTBLOCK));
		if (!current) {
			error(sizeof(TEXTBLOCK),MEMALLOC,"");
			return -1;
		}
		if (prev)
			prev->tb_next = current;
		else root = current;
		current->tb_prev = prev;
		current->tb_next = NULL;
		for (n = 0, lptr = current->tb_offset; n < LINES_PER_BLOCK; n++)
			*lptr = -1L;
	}

	current->tb_offset[blockpos] = offset;
	return 0;
}

/*
 *	get a line of text from file, updating the position table(s) if necessary
 *		returns	-1	eof reached
 *				>=0	length of string
 */
PRIVATE int vgets_nl(char *text,FILE *f)
{
int len;

	*text = '\0';

	if (curline > hwm) {
		if (update_textblock(f) < 0) {
			filelines = hwm;		/* truncate internally */
			internal_error = TRUE;
			return -1;
		}
		hwm = curline;
	}
	if (!fgets(text,MAX_DISPLAYSIZE-1,f)) {
		filelines = curline;
		return -1;
	}
	curline++;

	text[MAX_DISPLAYSIZE-1] = '\0';

	if ((len=strlen(text)) > 0)
		if (text[len-1] == '\n')
			text[--len] = '\0';

	return len;
}
