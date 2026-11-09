/*
 *	mirror.c: implements the Diamond Mirror configuration function
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

/*
 *	version 2.10 (december/2002)	roger burrows
 *		rewritten to a use windform-style INF file
 *
 *	version 2.54 (august/2003)		roger burrows
 *		. add DEMO support (no save for prefs)
 *
 *	version 2.57 (january/2017)		roger burrows
 *		prepare for public release
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <dos.h>
#include <limits.h>
#include <portab.h>
#include <string.h>
#include <unistd.h>

#include "edge.h"
#include "edgedefs.h"
#include "proto.h"

#define MODULE_NUMBER	8		/* used by LOCATION macro */

/*
 *	local structure/typedef definitions
 */
struct control {
	unsigned long partitions;
	int frequency;
};

typedef struct {
	struct control test;
	int test_detail;
	int test_rotate;
	struct control mirror;
	char mirror_path[MAX_EDGEPATH];
} MIRROR_PREFS;


/*
 *	external variables
 */
extern long drivemap;
extern OPT opt;
extern char app_path[MAX_PATHNAME];
extern OBJECT *mirror, *text;

/*
 *	private variables
 */
MLOCAL TEDINFO *tmirror;
MLOCAL MIRROR_PREFS mprefs;


/*
 *	private function prototypes
 */
PRIVATE void get_prefs(MIRROR_PREFS *mprefs);
PRIVATE void save_mirror_prefs(MIRROR_PREFS *mprefs);
PRIVATE void set_dm_app_path(void);
PRIVATE void set_mirror_prefs(MIRROR_PREFS *mprefs);
PRIVATE void update_dialog(MIRROR_PREFS *mprefs);
PRIVATE void validate_options(MIRROR_PREFS *mprefs);


/*
 *	initialise mirror dialog
 *	always returns 0
 */
PUBLIC int init_mirror_prefs()
{
	tmirror = (TEDINFO *) mirror[MIRPATH].ob_spec;

	get_prefs(&mprefs);

	return 0;
}

/*
 *	read in Diamond Mirror prefs & validate
 */
PRIVATE void get_prefs(MIRROR_PREFS *mprefs)
{
int n;
char temp[200], *p;
char savepath[MAX_PATHNAME];

	/*
	 *	temporarily set new app_path for open_section()
	 */
	strcpy(savepath,app_path);			/* save existing */
	set_dm_app_path();

	/*
	 *	initialise
	 */
	mprefs->test.partitions = 0L;
	mprefs->test.frequency = -1;
	mprefs->test_detail = 0;
	mprefs->test_rotate = 0;
	mprefs->mirror.partitions = 0L;
	mprefs->mirror.frequency = -1;
	mprefs->mirror_path[0] = '\0';

	if (open_section(DM_PREFS_FILE,"ra",TEST_SECTION) == 0) {
		temp[0] = '\0';
		read_section_line(INF_PARTITIONS," %[ -�]",temp);
		for (p = temp; *p; p++) {
			n = *p - 'A';
			if ((n >= FIRST_HD_DEVICE) && (n < NUM_DEVICES))
				mprefs->test.partitions |= (1L << n);
		}
		read_section_line(INF_FREQUENCY," %d",&mprefs->test.frequency);
		read_section_line(INF_DETAIL," %d",&mprefs->test_detail);
		read_section_line(INF_ROTATE," %d",&mprefs->test_rotate);
		close_section();
	}

	if (open_section(DM_PREFS_FILE,"ra",MIRROR_SECTION) == 0) {
		temp[0] = '\0';
		read_section_line(INF_PARTITIONS," %[ -�]",temp);
		for (p = temp; *p; p++) {
			n = *p - 'A';
			if ((n >= FIRST_HD_DEVICE) && (n < NUM_DEVICES))
				mprefs->mirror.partitions |= (1L << n);
		}
		read_section_line(INF_FREQUENCY," %d",&mprefs->mirror.frequency);
		read_section_line(INF_PATH," %[ -�]",mprefs->mirror_path);
		close_section();
	}

	strcpy(app_path,savepath);		/* restore previous path */

	/*
	 *	validate all options
	 */
	validate_options(mprefs);
}

PRIVATE void validate_options(MIRROR_PREFS *mprefs)
{
	/*
	 *	check Test options
	 */
	switch(mprefs->test.frequency) {
	case 1:
	case 7:
		break;
	default:					/* default is every boot ... */
		mprefs->test.frequency = 0;
		break;
	}
	if (mprefs->test_detail != 1)
		mprefs->test_detail = 0;
	if (mprefs->test_rotate != 1)
		mprefs->test_rotate = 0;

	/*
	 *	check Mirror options
	 */
	switch(mprefs->mirror.frequency) {
	case 1:
	case 7:
		break;
	default:					/* default is every boot ... */
		mprefs->mirror.frequency = 0;
		break;
	}
	if (!mprefs->mirror_path[0])
		copy_to_edgepath(mprefs->mirror_path,app_path);
}


/*********************************************************/

PUBLIC void config_mirror()
{
int n, obj;

char pname[MAX_PATHNAME];
int ret, j;
short edit;

	update_dialog(&mprefs);		/* according to mprefs ... */

	myform_center(mirror);
	obj_head(mirror);
	edit = MIRPATH;

	while(edit >= 0) {
		j = 0;
		ret = myform_do(mirror,&edit,0) & 0x7fff;
		mirror[ret].ob_state &= ~SELECTED;
		redraw_object(mirror,ret);
		switch(ret) {
		case MIRCAN:
			edit = -1;
			break;
		case MIRPATH:
			if (get_path(pname,tmirror->te_ptext,"*.*",text[CONFIG_FSEL_MIR].ob_spec)) {
				copy_to_tedinfo(tmirror,pname);
				redraw_object(mirror,MIRPATH);
			}
			break;
		case SELALL:
			for (n = FIRST_HD_DEVICE, obj = MTC; n < NUM_DEVICES; n++, obj++)
				if (!(mirror[obj].ob_state & DISABLED))
					objc_xsel(mirror,obj);
			for (n = FIRST_HD_DEVICE, obj = MMC; n < NUM_DEVICES; n++, obj++)
				if (!(mirror[obj].ob_state & DISABLED))
					objc_xsel(mirror,obj);
			break;
		case CLRALL:
			for (n = FIRST_HD_DEVICE, obj = MTC; n < NUM_DEVICES; n++, obj++)
				if (!(mirror[obj].ob_state & DISABLED))
					objc_xdsel(mirror,obj);
			for (n = FIRST_HD_DEVICE, obj = MMC; n < NUM_DEVICES; n++, obj++)
				if (!(mirror[obj].ob_state & DISABLED))
					objc_xdsel(mirror,obj);
			break;
		case MIRSAVE:
			if (check_path(tmirror->te_ptext) >= 0) {
				makedir(tmirror->te_ptext);
				save_mirror_prefs(&mprefs);
				edit = -1;
			}
			break;
		}
	}
	refresh_now(mirror,0);
}

PRIVATE void update_dialog(MIRROR_PREFS *mprefs)
{
int n, obj;

	/*
	 *	first disable and unselect all device objects;
	 *	unselect all frequency objects
	 */
	for (n = FIRST_HD_DEVICE, obj = MTC; n < NUM_DEVICES; n++, obj++) {
		mirror[obj].ob_state |= DISABLED;
		mirror[obj].ob_state &= ~SELECTED;
	}
	for (n = FIRST_HD_DEVICE, obj = MMC; n < NUM_DEVICES; n++, obj++) {
		mirror[obj].ob_state |= DISABLED;
		mirror[obj].ob_state &= ~SELECTED;
	}
	mirror[TONCEAWK].ob_state &= ~SELECTED;
	mirror[TONCEAD].ob_state &= ~SELECTED;
	mirror[TALWAYS].ob_state &= ~SELECTED;
	mirror[MONCEAWK].ob_state &= ~SELECTED;
	mirror[MONCEAD].ob_state &= ~SELECTED;
	mirror[MALWAYS].ob_state &= ~SELECTED;

	/*
	 *	next, if device exists, enable corresponding object;
	 *	if device is also specified, select it
	 */
	for (n = FIRST_HD_DEVICE, obj = MTC; n < NUM_DEVICES; n++, obj++) {
		if (drivemap&(1L<<n)) {
			mirror[obj].ob_state &= ~DISABLED;
			if (mprefs->test.partitions&(1L<<n))
				mirror[obj].ob_state |= SELECTED;
			else mirror[obj].ob_state &= ~SELECTED;
		}
	}
	for (n = FIRST_HD_DEVICE, obj = MMC; n < NUM_DEVICES; n++, obj++) {
		if (drivemap&(1L<<n)) {
			mirror[obj].ob_state &= ~DISABLED;
			if (mprefs->mirror.partitions&(1L<<n))
				mirror[obj].ob_state |= SELECTED;
			else mirror[obj].ob_state &= ~SELECTED;
		}
	}

	/*
	 *	set frequency objects appropriately
	 */
	switch(mprefs->test.frequency) {
	case 1:
		mirror[TONCEAD].ob_state |= SELECTED;
		break;
	case 7:
		mirror[TONCEAWK].ob_state |= SELECTED;
		break;
	default:
		mirror[TALWAYS].ob_state |= SELECTED;
		break;
	}
	switch(mprefs->mirror.frequency) {
	case 1:
		mirror[MONCEAD].ob_state |= SELECTED;
		break;
	case 7:
		mirror[MONCEAWK].ob_state |= SELECTED;
		break;
	default:
		mirror[MALWAYS].ob_state |= SELECTED;
		break;
	}

	/*
	 *	last, set path
	 */
	copy_to_tedinfo(tmirror,mprefs->mirror_path);
}

/*
 *	save mirror preferences
 *
 *	NOTE: at this time, we don't check for any errors ...
 */
PRIVATE void save_mirror_prefs(MIRROR_PREFS *mprefs)
{
int n;
char *p;
char savepath[MAX_PATHNAME], temp[MAX_PATHNAME];

	set_mirror_prefs(mprefs);

	/*
	 *	temporarily set new app_path for open_section()
	 */
	strcpy(savepath,app_path);			/* save existing */
	set_dm_app_path();

	if (open_section(DM_PREFS_FILE,"wa",TEST_SECTION) == 0) {
		for (n = 0, p = temp; n < 32; n++)
			if (mprefs->test.partitions&(1L<<n))
				*p++ = 'A' + n;
		*p = '\0';
		write_section_line(INF_PARTITIONS,"%s",temp);
		write_section_line(INF_FREQUENCY,"%d",mprefs->test.frequency);
		write_section_line(INF_DETAIL,"%d",mprefs->test_detail);
		write_section_line(INF_ROTATE,"%d",mprefs->test_rotate);
		close_section();
	}

	if (open_section(DM_PREFS_FILE,"wa",MIRROR_SECTION) == 0) {
		for (n = 0, p = temp; n < 32; n++)
			if (mprefs->mirror.partitions&(1L<<n))
				*p++ = 'A' + n;
		*p = '\0';
		write_section_line(INF_PARTITIONS,"%s",temp);
		write_section_line(INF_FREQUENCY,"%d",mprefs->mirror.frequency);
		write_section_line(INF_PATH,"%s",mprefs->mirror_path);
		close_section();
	}

	if (open_section(DM_PREFS_FILE,"wa",LASTRUN_SECTION) == 0) {
		write_section_line(INF_TESTRUN,"%c %ld",'A'+NUM_DEVICES-1,0L);	/* allow for rotation */
		write_section_line(INF_MIRRORRUN,"%c %ld",'A'+NUM_DEVICES-1,0L);
		close_section();
	}

	/*
	 *	let user know if he's wasted his time :-)
	 */
	sprintf(temp,"%s%s",app_path,DM_PROGRAM_FILE);
	if (access(temp,0) < 0)
		xform_alert(1,MI_WARN_ALERT,app_path,DM_PROGRAM_FILE);

	strcpy(app_path,savepath);		/* restore previous path */
}

/*
 *	set mirror prefs from dialog
 */
PRIVATE void set_mirror_prefs(MIRROR_PREFS *mprefs)
{
int n, obj;

	/*
	 *	check devices
	 */
	for (n = FIRST_HD_DEVICE, obj = MTC, mprefs->test.partitions = 0UL; n < NUM_DEVICES; n++, obj++)
		if (mirror[obj].ob_state & SELECTED)
			mprefs->test.partitions |= (1L<<n);
	for (n = FIRST_HD_DEVICE, obj = MMC, mprefs->mirror.partitions = 0UL; n < NUM_DEVICES; n++, obj++)
		if (mirror[obj].ob_state & SELECTED)
			mprefs->mirror.partitions |= (1L<<n);

	/*
	 *	check frequencies
	 */
	if (mirror[TONCEAD].ob_state&SELECTED)
		mprefs->test.frequency = 1;
	else if (mirror[TONCEAWK].ob_state&SELECTED)
		mprefs->test.frequency = 7;
	else mprefs->test.frequency = 0;
	if (mirror[MONCEAD].ob_state&SELECTED)
		mprefs->mirror.frequency = 1;
	else if (mirror[MONCEAWK].ob_state&SELECTED)
		mprefs->mirror.frequency = 7;
	else mprefs->mirror.frequency = 0;

	copy_to_edgepath(mprefs->mirror_path,tmirror->te_ptext);
}

PRIVATE void set_dm_app_path()
{
short boot;

	boot = get_sys(_bootdev);
	sprintf(app_path,DM_PATH,'A'+boot);
}
