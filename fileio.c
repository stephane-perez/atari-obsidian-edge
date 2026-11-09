/*
 *	fileio.c
 *
 *	performs the following major functions:
 *		print:    print information from diskinfo/diskmap/all info
 *		handles preferences
 *		utility:  print file
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

/*
 *	version 2.20 (january/2003)	roger burrows
 *		moved view_file() function to new module
 *
 *	version 2.54 (august/2003)		roger burrows
 *		. support new option to preserve partition label when zeroing
 *		  or wiping.  in order to avoid changing the resource when we
 *		  are planning to rewrite preference handling anyway, the
 *		  option must currently be set manually in the INF file.
 *		. add DEMO support
 *
 *	version 2.57 (january/2017)		roger burrows
 *		prepare for public release
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <portab.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"

#define MODULE_NUMBER	4		/* used by error() macro */

#define PREFMAGIC	12596		/* "14", magic number for old-style prefs */


/*
 *	external references
 */
extern OPT opt;
extern STATUS status;
extern short dev;
extern long drivemap;
extern OBJECT *chart, *dinfo, *dmap, *menu, *mirror, *multi, *prefs, *printstr, *text, *undel;
extern MYBPB dpb;
extern short FOLDER, FILES, FRAG, FRAGPFF, FRAG100;
extern char app_path[MAX_PATHNAME];
extern char ascii[256];

/*
 *	private variables
 */
MLOCAL TEDINFO *tval, *tmirror;

/*
 *	function prototypes
 */
PRIVATE int fgets_nl(char *temp,int maxlen,FILE *t);
PRIVATE void get_newprefs(OPT *opt);
PRIVATE int get_oldprefs(OPT *opt);
PRIVATE int get_path_length(char *path);
PRIVATE int print_line(char *str);
PRIVATE int print_long(char *s,long n);
PRIVATE int print_text(char *s,char *t);
PRIVATE void set_prefs(OPT *opt);
PRIVATE void update_menu_prefs(OPT *opt);
PRIVATE void validate_options(OPT *opt);


/*****************************************/
PUBLIC void init_prefs(void)
{
	tval = (TEDINFO *) prefs[VALPATH].ob_spec;
	tmirror = (TEDINFO *) mirror[MIRPATH].ob_spec;

	if (get_oldprefs(&opt))				/* if old-style file exists */
		save_prefs(&opt);				/* replace with new         */

	get_newprefs(&opt);					/* read in new-style preferences (if they exist) */

	update_menu_prefs(&opt);			/* update menu items / dialogs from preferences */
}

PRIVATE int get_oldprefs(OPT *opt)
{
int i;
FILE *t;
char temp[MAX_PATHNAME];

	sprintf(temp,"%s%s",app_path,PREFS_FILE);
	if (!(t=fopen(temp,"rb")))			/* no prefs file */
		return 0;
	if (fgetw(t) != PREFMAGIC) {		/* not old style */
		fclose(t);
		return 0;
	}

	opt->novice = fgetw(t);
	opt->warn = fgetw(t);
	opt->mpause = fgetw(t);
	fgets_nl(temp,MAX_PATHNAME,t);		/* throw away mirror path */
	fgets_nl(temp,MAX_PATHNAME,t);
	copy_to_edgepath(opt->filepath,temp);	/* use validation path for everything */
	fgets_nl(temp,MAX_PATHNAME,t);		/* we throw this one away ... */
	opt->visual = fgetw(t);
	opt->noheader = fgetw(t);
	opt->datemode = fgetw(t);
	opt->optfull = fgetw(t);
	opt->priority = fgetw(t);
	opt->rmdel = fgetw(t);
	opt->autofix = fgetw(t);
	opt->ignore = fgetw(t);
	opt->savelost = fgetw(t);
	opt->autofix |= fgetw(t);			/* compensate for old prefs file design error */
	opt->ignore |= fgetw(t);			/* likewise */
	opt->savelost |= fgetw(t);			/* and again */
	opt->usemirror = fgetw(t);
	opt->disablezero = fgetw(t);
	for (i = 0, opt->multi = 0UL; i < 16; i++)	/* 16 is the number of devices in the old file ... */
		if (fgetw(t))
			opt->multi |= 1<<i;
	fclose(t);

	sprintf(temp,"%s%s",app_path,PREFS_FILE);
	remove(temp);

	return 1;
}

PRIVATE int fgets_nl(char *temp,int maxlen,FILE *t)
{
int i;
char *p;

	temp[maxlen-1] = '\0';
	if (!fgets(temp,maxlen-1,t)) {
		*temp = '\0';
		return -1;
	}

	for (i = 0, p = temp; *p && (i < maxlen-2); i++, p++)
		if (*p == '\n')
			break;
	*p = '\0';

	return p-temp;
}

PRIVATE void get_newprefs(OPT *opt)
{
char temp[MAX_PATHNAME];
	
	opt->novice = -1;	/* initialise to invalid values */
	opt->warn = -1;
	opt->multi = -1;
	opt->filepath[0] = '\0';
	opt->autofix = -1;
	opt->ignore = -1;
	opt->savelost = -1;
	opt->visual = -1;
	opt->mpause = -1;
	opt->noheader = -1;
	opt->optfull = -1;
	opt->priority = -1;
	opt->rmdel = -1;
	opt->usemirror = -1;
	opt->datemode = -1;
	opt->disablezero = -1;

	if (open_section(PREFS_FILE,"ra",GENERAL_SECTION) == 0) {
		read_section_line(INF_NOVICE," %d",&opt->novice);
		read_section_line(INF_WARN," %d",&opt->warn);
		read_section_line(INF_MULTI," %08lx",&opt->multi);
		temp[0] = '\0';
		read_section_line(INF_FILEPATH," %[ -�]",temp);
		copy_to_edgepath(opt->filepath,temp);
		close_section();
	}

	if (open_section(PREFS_FILE,"ra",MEDIC_SECTION) == 0) {
		read_section_line(INF_MEDAUTOFIX," %d",&opt->autofix);
		read_section_line(INF_MEDIGNORE," %d",&opt->ignore);
		read_section_line(INF_MEDSAVE," %d",&opt->savelost);
		read_section_line(INF_MEDVISUAL," %d",&opt->visual);
		read_section_line(INF_MEDPAUSE," %d",&opt->mpause);
		read_section_line(INF_MEDNOHDR," %d",&opt->noheader);
		close_section();
	}

	if (open_section(PREFS_FILE,"ra",OPTIMIZE_SECTION) == 0) {
		read_section_line(INF_OPTFULL," %d",&opt->optfull);
		read_section_line(INF_OPTREAD," %d",&opt->priority);
		read_section_line(INF_OPTRMDEL," %d",&opt->rmdel);
		close_section();
	}

	if (open_section(PREFS_FILE,"ra",UNDELETE_SECTION) == 0) {
		read_section_line(INF_UNDMIRROR," %d",&opt->usemirror);
		read_section_line(INF_UNDDATE," %d",&opt->datemode);
		close_section();
	}

	if (open_section(PREFS_FILE,"ra",UTILITY_SECTION) == 0) {
		read_section_line(INF_UTILZERO," %d",&opt->disablezero);
		read_section_line(INF_UTILKEEP," %d",&opt->keeplabel);
		close_section();
	}

	/*
	 *	validate all options
	 */
	validate_options(opt);
}

PRIVATE void validate_options(OPT *opt)
{
	if (opt->novice != FALSE)
		opt->novice = TRUE;
	if (opt->warn != FALSE)
		opt->warn = TRUE;
	if ((opt->multi < 0L) || (opt->multi >= (1<<NUM_DEVICES)))
		opt->multi = 0L;
	if (!opt->filepath[0])
		copy_to_edgepath(opt->filepath,app_path);

	if (opt->autofix != TRUE)			/* default is NO autofix */
		opt->autofix = FALSE;
	if (opt->ignore != FALSE)
		opt->ignore = TRUE;
	if (opt->savelost != FALSE)
		opt->savelost = TRUE;
	if (opt->visual != FALSE)
		opt->visual = TRUE;
	if ((opt->mpause < 0) || (opt->mpause > 9))
		opt->mpause = PAUSE_FOREVER;
	if (opt->noheader != FALSE)
		opt->noheader = TRUE;

	if (opt->optfull != FALSE)
		opt->optfull = TRUE;
	if (opt->priority != FALSE)
		opt->priority = TRUE;
	if (opt->rmdel != FALSE)
		opt->rmdel = TRUE;

	if (opt->usemirror != TRUE)			/* default is simple undelete */
		opt->usemirror = FALSE;
	if (opt->datemode != US)
		opt->datemode = UK;

	if (opt->disablezero != FALSE)
		opt->disablezero = TRUE;

	if (opt->keeplabel != TRUE)			/* default is don't preserve */
		opt->keeplabel = FALSE;
}

PRIVATE void update_menu_prefs(OPT *opt)
{
int i;
TEDINFO *pause;

	if (opt->novice) {								/* general */
		prefs[NOVICEU].ob_state |= SELECTED;
		prefs[EXPERTU].ob_state &= ~SELECTED;
	} else {
		prefs[EXPERTU].ob_state |= SELECTED;
		prefs[NOVICEU].ob_state &= ~SELECTED;
	}

	if (opt->warn) {
		prefs[WARNDL].ob_state |= SELECTED;
		prefs[WARNN].ob_state &= ~SELECTED;
	} else {
		prefs[WARNN].ob_state |= SELECTED;
		prefs[WARNDL].ob_state &= ~SELECTED;
	}

	for (i = 0; i < NUM_DEVICES; i++)
		if ((1<<i) & drivemap & opt->multi)
			multi[AMULTI+i].ob_state |= SELECTED;

	copy_to_tedinfo(tval,opt->filepath);

	if (opt->autofix) {								/* medic */
		menu[MAUTOFIX].ob_state |= CHECKED;
		menu[OAUTOFIX].ob_state |= CHECKED;
	} else {
		menu[MAUTOFIX].ob_state &= ~CHECKED;
		menu[OAUTOFIX].ob_state &= ~CHECKED;
	}

	if (opt->ignore) {
		menu[MIGNORE].ob_state |= CHECKED;
		menu[OIGNORE].ob_state |= CHECKED;
	} else {
		menu[MIGNORE].ob_state &= ~CHECKED;
		menu[OIGNORE].ob_state &= ~CHECKED;
	}

	if (opt->savelost) {
		menu[MSAVLOST].ob_state |= CHECKED;
		menu[OSAVLOST].ob_state |= CHECKED;
	} else {
		menu[MSAVLOST].ob_state &= ~CHECKED;
		menu[OSAVLOST].ob_state &= ~CHECKED;
	}

	if (opt->visual) {
		menu[OVISUAL].ob_state |= CHECKED;
		menu[MVISUAL].ob_state |= CHECKED;
	} else {
		menu[OVISUAL].ob_state &= ~CHECKED;
		menu[MVISUAL].ob_state &= ~CHECKED;
	}

	pause = (TEDINFO *) prefs[PAUSE].ob_spec;

	if ((opt->mpause < 0) || (opt->mpause > 9))
		strcpy(pause->te_ptext,"");
	else sprintf(pause->te_ptext,"%1d",opt->mpause);

	if (opt->noheader)
		menu[IGNOREPH].ob_state |= CHECKED;
	else menu[IGNOREPH].ob_state &= ~CHECKED;

	if (opt->optfull) {								/* optimize */
		menu[OFULL].ob_state |= CHECKED;
		menu[OCOMP].ob_state &= ~CHECKED;
	} else {
		menu[OFULL].ob_state &= ~CHECKED;
		menu[OCOMP].ob_state |= CHECKED;
	}

	if (opt->priority) {
		menu[OREAD].ob_state |= CHECKED;
		menu[OWRITE].ob_state &= ~CHECKED;
	} else {
		menu[OREAD].ob_state &= ~CHECKED;
		menu[OWRITE].ob_state |= CHECKED;
	}

	if (opt->rmdel)
		menu[ORMDEL].ob_state |= CHECKED;
	else menu[ORMDEL].ob_state &= ~CHECKED;

	if (opt->usemirror) {							/* undelete */
		undel[UDMIR].ob_state |= SELECTED;
		undel[UDSIM].ob_state &= ~SELECTED;
	} else {
		undel[UDMIR].ob_state &= ~SELECTED;
		undel[UDSIM].ob_state |= SELECTED;
	}

	if (opt->datemode == US) {
		menu[UDATEUS].ob_state |= CHECKED;
		menu[UDATEUK].ob_state &= ~CHECKED;
	} else {
		menu[UDATEUS].ob_state &= ~CHECKED;
		menu[UDATEUK].ob_state |= CHECKED;
	}

	if (opt->disablezero) {							/* utility */
		menu[ZERO].ob_state |= DISABLED;
		menu[WIPE].ob_state |= DISABLED;
		menu[UNZERO].ob_state |= DISABLED;
		strcpy(menu[EZERO].ob_spec,text[ZERO_ENABLE].ob_spec);
	} else {
		menu[ZERO].ob_state &= ~DISABLED;
		menu[WIPE].ob_state &= ~DISABLED;
		menu[UNZERO].ob_state &= ~DISABLED;
		strcpy(menu[EZERO].ob_spec,text[ZERO_DISABLE].ob_spec);
	}
}

PUBLIC void copy_to_tedinfo(TEDINFO *t,char *s)
{
int n = t->te_txtlen - 1;

	strncpy(t->te_ptext,s,n);
	t->te_ptext[n] = '\0';
}

PUBLIC void copy_to_edgepath(char *t,char *s)
{
int i;

	for (i = 0; *s && (i < MAX_EDGEPATH-1); i++)
		*t++ = *s++;
	*t = '\0';
}

/***************************************************************************/

PUBLIC void do_prefs(OPT *opt)
{
short exit, edit;
TEDINFO *pause;
char pname[MAX_PATHNAME];

	if (opt->novice) {
		prefs[NOVICEU].ob_state |= SELECTED;
		prefs[EXPERTU].ob_state &= ~SELECTED;
	} else {
		prefs[EXPERTU].ob_state |= SELECTED;
		prefs[NOVICEU].ob_state &= ~SELECTED;
	}
	if (opt->warn) {
		prefs[WARNDL].ob_state |= SELECTED;
		prefs[WARNN].ob_state &= ~SELECTED;
	} else {
		prefs[WARNN].ob_state |= SELECTED;
		prefs[WARNDL].ob_state &= ~SELECTED;
	}

	pause = (TEDINFO *) prefs[PAUSE].ob_spec;
	if ((opt->mpause < 0) || (opt->mpause > 9))
		pause->te_ptext[0] = '\0';
	else sprintf(pause->te_ptext,"%d",opt->mpause);
	copy_to_tedinfo(tval,opt->filepath);

	myform_center(prefs);
	obj_head(prefs);
	edit = PAUSE;

	while(edit >= 0) {
		exit = myform_do(prefs,&edit,0) & 0x7fff;
		prefs[exit].ob_state &= ~SELECTED;
		redraw_object(prefs,exit);
		switch(exit) {
		case CANPREF:
			edit = -1;
			break;
		case PAUSE:
			edit = PAUSE;
			break;
		case VALPATH:
			edit = VALPATH;
			if (get_path(pname,tval->te_ptext,"*.*",text[CONFIG_FSEL_VAL].ob_spec)) {
				copy_to_tedinfo(tval,pname);
				redraw_object(prefs,VALPATH);
			}
			break;
		case SAVEPREF:
			edit = VALPATH;
			if (check_path(tval->te_ptext) >= 0) {
				makedir(tval->te_ptext);
				save_prefs(opt);
				edit = -1;
			}
			break;
		}
	}
	refresh_screen(prefs);
}

PRIVATE void set_prefs(OPT *opt)
{
int i;
TEDINFO *pause;

	if (prefs[NOVICEU].ob_state & SELECTED)		/* general */
		opt->novice = TRUE;
	else opt->novice = FALSE;

	if (prefs[WARNDL].ob_state & SELECTED)
		opt->warn = TRUE;
	else opt->warn = FALSE;

	for (i = 0, opt->multi = 0UL; i < NUM_DEVICES; i++)
		if (multi[AMULTI+i].ob_state & SELECTED)
			opt->multi |= 1<<i;

	copy_to_edgepath(opt->filepath,tval->te_ptext);

	if ((menu[MAUTOFIX].ob_state & CHECKED)		/* medic */
	 || (menu[OAUTOFIX].ob_state & CHECKED))
		opt->autofix = TRUE;
	else opt->autofix = FALSE;

	if ((menu[MIGNORE].ob_state & CHECKED)
	 || (menu[OIGNORE].ob_state & CHECKED))
		opt->ignore = TRUE;
	else opt->ignore = FALSE;

	if ((menu[MSAVLOST].ob_state & CHECKED)
	 || (menu[OSAVLOST].ob_state & CHECKED))
		opt->savelost = TRUE;
	else opt->savelost = FALSE;

	if ((menu[MVISUAL].ob_state & CHECKED)
	 || (menu[OVISUAL].ob_state & CHECKED))
		opt->visual = TRUE;
	else opt->visual = FALSE;

	pause = (TEDINFO *) (prefs + PAUSE)->ob_spec;
	if (strlen(pause->te_ptext))
		opt->mpause = atoi(pause->te_ptext);
	else opt->mpause = PAUSE_FOREVER;

	if (menu[IGNOREPH].ob_state & CHECKED)
		opt->noheader = TRUE;
	else opt->noheader = FALSE;

	if (menu[OFULL].ob_state & CHECKED)			/* optimize */
		opt->optfull = TRUE;
	else opt->optfull = FALSE;

	if (menu[OREAD].ob_state & CHECKED)
		opt->priority = TRUE;
	else opt->priority = FALSE;

	if (menu[ORMDEL].ob_state & CHECKED)
		opt->rmdel = TRUE;
	else opt->rmdel = FALSE;

	if (undel[UDMIR].ob_state & SELECTED)		/* undelete */
		opt->usemirror = TRUE;
	else opt->usemirror = FALSE;

	if (menu[UDATEUS].ob_state & CHECKED)
		opt->datemode = US;
	else opt->datemode = UK;

	if (menu[ZERO].ob_state & DISABLED)			/* utility */
		opt->disablezero = TRUE;
	else opt->disablezero = FALSE;
}

/*
 *	save preferences
 *
 *	NOTE: at this time, we don't check for any errors ...
 */
PUBLIC void save_prefs(OPT *opt)
{
	set_prefs(opt);

	open_section(PREFS_FILE,"wa",GENERAL_SECTION);
	write_section_line(INF_NOVICE,"%d",opt->novice);
	write_section_line(INF_WARN,"%d",opt->warn);
	write_section_line(INF_MULTI,"%08lx",opt->multi);
	write_section_line(INF_FILEPATH,"%s",opt->filepath);
	close_section();

	open_section(PREFS_FILE,"wa",MEDIC_SECTION);
	write_section_line(INF_MEDAUTOFIX,"%d",opt->autofix);
	write_section_line(INF_MEDIGNORE,"%d",opt->ignore);
	write_section_line(INF_MEDSAVE,"%d",opt->savelost);
	write_section_line(INF_MEDVISUAL,"%d",opt->visual);
	write_section_line(INF_MEDPAUSE,"%d",opt->mpause);
	write_section_line(INF_MEDNOHDR,"%d",opt->noheader);
	close_section();

	open_section(PREFS_FILE,"wa",OPTIMIZE_SECTION);
	write_section_line(INF_OPTFULL,"%d",opt->optfull);
	write_section_line(INF_OPTREAD,"%d",opt->priority);
	write_section_line(INF_OPTRMDEL,"%d",opt->rmdel);
	close_section();

	open_section(PREFS_FILE,"wa",UNDELETE_SECTION);
	write_section_line(INF_UNDMIRROR,"%d",opt->usemirror);
	write_section_line(INF_UNDDATE,"%d",opt->datemode);
	close_section();

	open_section(PREFS_FILE,"wa",UTILITY_SECTION);
	write_section_line(INF_UTILZERO,"%d",opt->disablezero);
	if (opt->keeplabel)
		write_section_line(INF_UTILKEEP,"%d",opt->keeplabel);
	close_section();
}

/*
 *	check directory-only path string
 *	must start & end with backslash, with valid names
 *
 *	complete reimplementation by roger burrows, using state machine
 *	now returns -1 for error, 0 otherwise
 */
PRIVATE int check_dirs(char *dirpath)
{
enum states { START, DONODE, DOEXT, ENDEXT, ENDDIR, END, DIROK, DIRERROR };
int state = START;
int nodecount, extcount;

	while(TRUE) {
		switch(state) {
		case START:
			nodecount = 0;
			extcount = 0;
			if (*dirpath != '\\')
				state = DIRERROR;
			else state = DONODE;
			break;
		case DONODE:
			dirpath++;
			if (*dirpath == '\0')
				state = END;
			else if (*dirpath == '\\')
				state = ENDDIR;
			else if (*dirpath == '.')
				state = DOEXT;
			else if (!ascii[*dirpath])
				state = DIRERROR;
			else nodecount++;
			break;
		case DOEXT:
			dirpath++;
			if (*dirpath == '\0')
				state = END;
			else if (*dirpath == '\\')
				state = ENDEXT;
			else if (*dirpath == '.')
				state = DIRERROR;
			else if (!ascii[*dirpath])
				state = DIRERROR;
			else extcount++;
			break;
		case ENDEXT:
			if ((extcount == 0) || (extcount > EXT_LENGTH))
				state = DIRERROR;
			else state = ENDDIR;
			break;
		case ENDDIR:
			if ((nodecount == 0) || (nodecount > NODE_LENGTH))
				state = DIRERROR;
			else state = START;
			break;
		case END:
			if (nodecount + extcount)
				state = DIRERROR;
			else state = DIROK;
			break;
		case DIROK:
			return 0;
			break;
		default:				/* the DIRERROR state or an unexpected one ... */
			return -1;
		}
	}

	return -1;		/* can't get here ... */
}

/*
 *	validate path, issue alert if error
 *		if X:, modify to X:\
 *		return length of path, -ve for error
 */
PUBLIC int check_path(char *path)
{
int rc;

	if ((rc=get_path_length(path)) < 0)
		xform_alert(1,FI_INVPATH_ALERT);

	return rc;
}

/*
 *	get path length, return -1 if error
 */
PRIVATE int get_path_length(char *path)
{
int slen, j;

	j = *path - 'A';
	if ((j < 0) || (j >= NUM_DEVICES))
		return -1;

	if (!((1L<<j) & drivemap))
		return -1;

	if (*(path+1) != ':')
		return -1;

	if (strpbrk(path+2,":*?") != NULL)
		return -1;

	slen = strlen(path);
	if (*(path+slen-1) != '\\') {		/* ensure trailing backslash */
		*(path+slen) = '\\';
		slen++;
		*(path+slen) = '\0';
	}

	if (*(path+2) != '\\')
		return -1;

	if (check_dirs(path+2) < 0)
		return -1;

	return slen;
}

/****************************************************************/
PUBLIC void makedir(char *todir)
{
int i, len;
char tempdir[MAX_PATHNAME];

	len = strlen(todir);
	for (i = 3; i < len; ) {
		while ((*(todir + i) != '\\') && (i < len))
			i++;
		if (i < len) {
			strncpy(tempdir,todir,i);
			tempdir[i] = '\0';
			i++;
			mkdir(tempdir,0);
		}
	}
}

/*
 *	get name of existing file
 *
 *	returns:
 *		0	cancel from file selector
 *		1	file exists
 */
PUBLIC int get_existing_fname(char *pfname,const char *path,const char *fmask,const char *prompt)
{
short button = 0;
int n, was_hidden;
char temp[MAX_FILENAME], pathname[MAX_PATHNAME], *lastslash;

	save_screen();
	was_hidden = mouse_on();

	strcpy(pathname,path);
	strcat(pathname,fmask);

	while(TRUE) {
		temp[0] = '\0';
		if (fsel_exinput(pathname,temp,&button,prompt) == 0)
			button = 0;
		if (button == 0)
			break;
		if (strlen(temp) && (lastslash = strrchr(pathname,'\\'))) {
			n = lastslash - pathname + 1;
			strncpy(pfname,pathname,n);
			pfname[n] = '\0';
			strcat(pfname,temp);
			if (access(pfname,0) == 0)			/* file exists */
				break;
			xform_alert(1,FI_MISSING_ALERT,temp);
		} else xform_alert(1,FI_INVPATH_ALERT);
	}

	if (was_hidden)
		mouse_off();
	restore_screen();

	return button;
}

/*
 *	get name of file to write to
 *
 *	returns:
 *		0	cancel from file selector
 *		1	file doesn't exist, or file exists & user wants to overwrite
 *		2	file exists & user wants to append
 */
PUBLIC int get_writeable_fname(char *pfname,const char *path,const char *fmask,const char *prompt)
{
short button = 0;
int n, was_hidden;
char temp[MAX_FILENAME], pathname[MAX_PATHNAME], *lastslash;

	save_screen();
	was_hidden = mouse_on();

	strcpy(pathname,path);
	strcat(pathname,fmask);

	while(TRUE) {
		temp[0] = '\0';
		if (fsel_exinput(pathname,temp,&button,prompt) == 0)
			button = 0;
		if (button == 0)
			break;
		if (strlen(temp) && (lastslash = strrchr(pathname,'\\'))) {
			n = lastslash - pathname + 1;
			strncpy(pfname,pathname,n);
			pfname[n] = '\0';
			strcat(pfname,temp);
			if (access(pfname,0) == 0) {			/* file exists */
				if ((button=xform_alert(2,FI_EXISTS_ALERT,temp)) == 3)	/* Overwrite/Append/Cancel */
					continue;
			} else button = 1;						/* file doesn't exist */
			break;
		}
		xform_alert(1,FI_INVPATH_ALERT);
	}

	if (was_hidden)
		mouse_off();
	restore_screen();

	return button;
}

/**************************************************************/
/*
 *	get_path(): prompt for path
 *		args:	pfname	ptr to string to receive selected path
 *						note: trailing backslash NOT removed
 *				path	ptr to initial pathname
 *				fmask	ptr to initial filename mask
 *				prompt	prompt to display
 */
PUBLIC int get_path(char *pfname,const char *path,const char *fmask,const char *prompt)
{
int n, was_hidden;
short button = 0;
char temp[MAX_FILENAME], pathname[MAX_PATHNAME], *lastslash;

	save_screen();
	was_hidden = mouse_on();

	strcpy(pathname,path);
	strcat(pathname,fmask);

	while(TRUE) {
		temp[0] = '\0';
		if (fsel_exinput(pathname,temp,&button,prompt) == 0)	/* precaution */
			button = 0;
		if (button == 0)
			break;
		if ((lastslash=strrchr(pathname,'\\'))) {
			n = lastslash - pathname + 1;
			strncpy(pfname,pathname,n);
			pfname[n] = '\0';
			break;
		}
		xform_alert(1,FI_INVPATH_ALERT);
	}

	if (was_hidden)
		mouse_off();
	restore_screen();

	return button;
}

/*
 *	print null-terminated string, followed by cr/lf
 *	if error occurs, issues xform_alert()
 *
 *	returns 1 if ok, 0 if error
 */
PUBLIC int print_string(char *str)
{
	do {
		if (print_line(str))
			return 1;
	} while(xform_alert(1,PR_OFFLINE_ALERT) == 1);

	return 0;
}

/*
 *	print null-terminated string, followed by cr/lf
 *
 *	returns 1 if ok, 0 if error
 */
PRIVATE int print_line(char *str)
{
clock_t end;

	end = clock() + MAX_PRINT_WAIT;
	while(Cprnos() == 0)
		if (clock() > end)
			return 0;

	while(*str)
		if (!Cprnout(*str++))
			return 0;

	if (!Cprnout('\r'))
		return 0;
	if (!Cprnout('\n'))
		return 0;

	return 1;
}

/***************************************************************************/
PUBLIC void print_file()
{
FILE *f;
char temp[MAX_PATHNAME], fname[MAX_PATHNAME];

	if (get_existing_fname(fname,app_path,"",text[PRINT_FSEL].ob_spec) == 0)
		return;

	if ((f=fopen(fname,"ra")) == NULL) {
		error(-1L,OPEN,fname);
		return;
	}

	while(fgets_nl(temp,MAX_PATHNAME,f) >= 0)
		if (!print_string(temp))
			break;

	fclose(f);
}

/***************************************************************************/
PUBLIC void do_infoprint()
{
int i, device, icon = 0;
char pstr[MAX_PRINTSIZE];

	toggle_bottom(BPRINT);

	switch(status.action) {
	case ALLINFO:
		icon = BALLINFO;
		if (!print_string(printstr[PRINT_ALLHDR].ob_spec))
			break;
		if (!print_string(printstr[PRINT_ALLHDR2].ob_spec))
			break;
		for (device = FIRST_HD_DEVICE, i = 0; device < NUM_DEVICES; device++, i++) {
			sprintf(pstr,"   %c:    %11s      %11s      %11s    %4s",
					device+'A',chart[CTOT+i].ob_spec,chart[CUSED+i].ob_spec,
					chart[CFREE+i].ob_spec,chart[CFREEP+i].ob_spec);
			if (!print_string(pstr))
				break;
		}
		if (!print_string(printstr[PRINT_ALLHDR2].ob_spec))
			break;
		sprintf(pstr,"%8.8s %11s      %11s      %11s    %4s",
				printstr[PRINT_ALLTOTAL].ob_spec,chart[TTOT].ob_spec,chart[TUSED].ob_spec,
				chart[TFREE].ob_spec,chart[TFREEP].ob_spec);
		if (!print_string(pstr))
			break;
		form_feed();
		break;

	case DISKINFO:
		icon = BDINFO;
		sprintf(pstr,printstr[PRINT_HDR].ob_spec,dev+'A');
		if (!print_string(pstr))
			break;
		if (!print_string(printstr[PRINT_HDR2].ob_spec))
			break;
		if (!print_string(""))
			break;
		if (!print_long(printstr[PRINT_BPS].ob_spec,dpb.bps))
			break;
		if (!print_long(printstr[PRINT_SPC].ob_spec,dpb.spc))
			break;
		if (!print_long(printstr[PRINT_BPC].ob_spec,dpb.bpc))
			break;
		if (!print_long(printstr[PRINT_SPT].ob_spec,dpb.spt))
			break;
		if (!print_long(printstr[PRINT_TPC].ob_spec,dpb.tpc))
			break;
		if (!print_long(printstr[PRINT_TSEC].ob_spec,dpb.tsec))
			break;
		if (!print_long(printstr[PRINT_TCTL].ob_spec,dpb.firstd-1))
			break;
		if (!print_long(printstr[PRINT_TOTCLUS].ob_spec,dpb.tsec/2L))
			break;
		if (!print_long(printstr[PRINT_DATACLUS].ob_spec,dpb.cdata))
			break;

		if (!print_string(""))
			break;
		if (!print_text(printstr[PRINT_FATTYPE].ob_spec,dpb.isfat16?"16 bit":"12 bit"))	//MAGNUM
			break;
		if (!print_long(printstr[PRINT_SPF].ob_spec,dpb.spf))
			break;
		if (!print_long(printstr[PRINT_NFAT].ob_spec,dpb.nfat))
			break;
		if (!print_long(printstr[PRINT_FAT2START].ob_spec,1+dpb.spf))
			break;
		if (!print_long(printstr[PRINT_RDS].ob_spec,dpb.rds))
			break;
		if (!print_long(printstr[PRINT_NDIR].ob_spec,dpb.ndir))
			break;
		if (!print_long(printstr[PRINT_ROOTSTART].ob_spec,1+2*dpb.spf))
			break;
		if (!print_long(printstr[PRINT_DATASTART].ob_spec,dpb.firstd))
			break;
		if (!print_string(""))
			break;
		if (!print_text(printstr[PRINT_TOTBYTES].ob_spec,dinfo[MAPTBYTE].ob_spec))
			break;
		if (!print_text(printstr[PRINT_USEDBYTES].ob_spec,dinfo[MAPUBYTE].ob_spec))
			break;
		if (!print_text(printstr[PRINT_FREEBYTES].ob_spec,dinfo[MAPFBYTE].ob_spec))
			break;
		form_feed();
		break;

	case DISKMAP:
		icon = BFRAGMAP;
		sprintf(pstr,printstr[PRINT_FRAGHDR].ob_spec,'A'+dev);
		if (!print_string(pstr))
			break;
		if (!print_string(printstr[PRINT_FRAGHDR2].ob_spec))
			break;
		if (!print_string(""))
			break;

		if (!print_text(printstr[PRINT_FOLDER].ob_spec,dmap[FOLDER].ob_spec))
			break;
		if (!print_text(printstr[PRINT_FILES].ob_spec,dmap[FILES].ob_spec))
			break;
		if (!print_text(printstr[PRINT_FRAG].ob_spec,dmap[FRAG].ob_spec))
			break;
		if (!print_text(printstr[PRINT_FRAGPFF].ob_spec,dmap[FRAGPFF].ob_spec))
			break;
		if (!print_text(printstr[PRINT_FRAG100].ob_spec,dmap[FRAG100].ob_spec))
			break;
		form_feed();
		break;

	case UNDELETE:
		icon = BUNDEL;
		break;
	}

	if (icon)
		toggle_bottom(icon);
}

PRIVATE int print_long(char *s,long n)
{
char pstr[MAX_PRINTSIZE];

	sprintf(pstr,s,n);
	return print_string(pstr);
}

PRIVATE int print_text(char *s,char *t)
{
char pstr[MAX_PRINTSIZE];

	sprintf(pstr,s,t);
	return print_string(pstr);
}
