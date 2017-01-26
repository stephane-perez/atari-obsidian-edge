/*
 *	main line processing for Diamond Edge
 *
 *
 *	version 2.10 (november/2002)		roger burrows
 *		a highly cleaned up and reorganised version of
 *		the DE v2.04 source; no major new function
 *
 *	version 2.11 (december 2002)		roger burrows
 *		updated for new preferences handling
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <assert.h>
#include <cookie.h>
#include <ctype.h>
#include <dos.h>
#include <portab.h>
#include <string.h>
#include <vdi.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "map.h"
#include "keycodes.h"
#include "proto.h"
#include "lib/driver.h"

#define MODULE_NUMBER	6		/* used by LOCATION macro */

#define RSC_NOT_FOUND	"[1][" RSC_FILE " missing or invalid.|Program terminated][ OK ]"
#define WIDTH_WARNING	"[1][Warning: screen is not wide|enough to display windows.|Program may not work!][Continue|Cancel]"


/*****Defined in Main*******************/

unsigned long _STACK = 32768L;		//rfb: was 16384L, probably no need to have changed ...

short pxyarray[10]; 				/* input point array */
short msg_buf[8];
short myhandle;						/* workstation handle */
short nplanes, Xw, Yh;				/* graphics device: # planes, width-1, height-1 */
short xchar, ychar;					/* character width, height */
short win1;							/* window handle */
short rez;							/* char height / 8 */
short windhidden = 1;
short fullx, fully, fullh, fullw;	/* exterior coords of win1 */
short workx, worky, workh, workw;	/* work area coordinates of win1 */
short bgcolor = WHITE;				/* background object colour */
short indcolor = WHITE;				/* indicator object colour */

/* system-related variables */
short fastram = FALSE;				/* TRUE => fast RAM present */
short cpu_type = 0;					/* from _CPU cookie */
//short machine_type = 0;				/* from _MCH cookie */
short MagiC = 0;					/* TRUE => MagiC present */

/* device-related variables */
short numflops;						/* number of floppy drives (1 or 2) */
long drivemap;						/* from Drvmap() (excluding devices from CDmap()) */
short dev;							/* the logical device (partition) we are currently processing */
MYBPB dpb;							/* BPB info associated with 'dev' */

/* preference-related variables */
char app_path[MAX_PATHNAME];		/* where PREFS_FILE is stored */
OPT opt;							/* preferences, loaded from PREFS_FILE */

/* dialog processing variables */
short edit_obj = 0;					/* both these are used in undel.c */
short edpos = 0;

/* program state variables */
STATUS status = {IDLE, -1, -1 };	/* used everywhere, keeps track of next action to perform */

/* for tracking recursive directory processing */
short dirlevel;
DIRLIST dirlist[MAX_DIR_DEPTH];

/*
 * special variable used by datalite processing
 *	'doing_opt'	is only set during optimisation routines (both full & compress free space).
 *				if set, load_mybpb() sets mybpb->cdata from numcl in the BPB, rather than
 *				doing the "standard" datalite calculation ...
 */
short doing_opt = 0;

//char *fat12;
//unsigned short *fat16;
short mouse_x, mouse_y, num_clicks, key, keystate;
DIRENTRY direntry;
FILEINFO finfo;
GRECT gmap;

short blockheight;					/* for cluster map */
									/* pointers to trees */
OBJECT *about, *menu, *text, *printstr, *dinfo, *bottom, *pcsel, *multi, *chart, *dmap, *prefs;
OBJECT *saveinfo, *restinfo, *restconf, *medic, *badpref, *mirror, *bar, *dup, *obar;
OBJECT *selscsi, *scsiedit, *newsize, *split, *rebone, *bone, *undel, *undname;


/*
 *	private variables
 */
MLOCAL short ap_id;								/* application id */
MLOCAL short byebye = 0;
MLOCAL short lastdev;
MLOCAL short initial_msg_type = MN_SELECTED;	/* see send_initial_message() */
MLOCAL int protect_error = 1;					/* protection scheme ... */


/*
 *	function prototypes
 */
PRIVATE void control(void);
PRIVATE void do_menu(void);
PRIVATE void do_multi(short type);
PRIVATE void do_redraw(short hand,GRECT *rec1);
PRIVATE int get_drive(void);
PRIVATE void handle_button(void);
PRIVATE void handle_keys(void);
PRIVATE void handle_mask(void);
PRIVATE void menu_keyshort(OBJECT *adr_menu,int evnt,int mode);
PRIVATE void menu_toggle(void);
PRIVATE short open_workstation(void);
PRIVATE void process(void);
PRIVATE void redraw(short h,GRECT rect);
PRIVATE void send_initial_message(void);
PRIVATE void set_app_path(char *app_name);
PRIVATE void set_opt(void);
PRIVATE void set_test(void);
PRIVATE void set_window(void);
PRIVATE void set_work(OBJECT *tree);
PRIVATE int stdkey(int key_code);
PRIVATE void update_menu(int dev);


/*
 *	macros
 */
							/* return pointer to text of TEDINFO object */
#define	gettext(tree,which)		((char *)(((TEDINFO *)tree[which].ob_spec)->te_ptext))


/**************************************************************************/
PUBLIC int main(void)
{
long cookieval;

	ap_id = appl_init();
	myhandle = open_workstation();

	set_app_path(PROGRAM_FILE);
	numflops = nflop();
	drivemap = Drvmap() & ~CDmap();			/* eliminate CD-ROM devices from map    */
											/* to prevent irritating error messages */
	if (drivemap & (1L<<FIRST_HD_DEVICE))	/* if drive C exists */
		dev = FIRST_HD_DEVICE;				/* use it as default */
	else dev = 0;							/* else use drive A  */
	lastdev = dev;

	if (getcookie(_FRB,NULL))
		fastram = TRUE;
//	if (getcookie(_MCH,&cookieval))
//		machine_type = cookieval >> 16;
	if (getcookie(_CPU,&cookieval))
		cpu_type = (short) cookieval;
	if (getcookie(MAGX_COOKIE,&cookieval))
		MagiC = TRUE;

	process();

	graf_mouse(ARROW,NULL);
	close_driver();
	v_clsvwk(myhandle);
	appl_exit();

	exit(0);

	return 0;			/* keep compiler happy */
}

PRIVATE short open_workstation(void)
{
short work_in[11];			/* Input to GSX parameter array */
short work_out[57];			/* Output from GSX parameter array */
short handle, dummy;
int i;

	work_in[0] = Getrez() + 2;
	for (i = 1; i < 10; work_in[i++] = 1)
		;
	work_in[10] = 2;
	handle = graf_handle(&xchar,&ychar,&dummy,&dummy);
	v_opnvwk(work_in,&handle,work_out);
	if (!handle)
		exit(0);

	rez = ychar / 8;

	Xw = work_out[0];
	Yh = work_out[1];

	vq_extnd(handle,1,work_out); 	/* determine number of planes */
	nplanes = work_out[4];

	return handle;
}

PRIVATE void set_app_path(char *app_name)
{
char *ptr;
int drive;

	strcpy(app_path,app_name);
	if (shel_find(app_path) == 0)
		app_path[0] = '\0';
	else if ((ptr=strrchr(app_path,'\\')) != NULL)
		ptr[1] = '\0';
	else {
		drive = Dgetdrv();
		sprintf(app_path,"%c:\\",'A'+drive);
		if (Dgetpath(app_path+2,drive+1) < 0)
			app_path[2] = '\0';
		else strcat(app_path,"\\");
	}
}

/*
 *	do mainline processing
 */
PRIVATE void process(void)
{
short dummy, j;

	if (load_rsrc() < 0) {
		form_alert(1,RSC_NOT_FOUND);
		return;
	}

	protect_error = protect();				/* for later tests */

	if (Xw < MAX_WINDOW_WIDTH-1) {
#ifdef _DEBUG
		if (form_alert(1,WIDTH_WARNING) != 1)
			return;
#else
		xform_alert(1,GEN_CHGRES_ALERT);
		return;
#endif
	}

	blockheight = rez * 2;					/* for drawing cluster map */

	if (IS_COLOUR_AES && (nplanes > 1)) {	/* use default if old AES or mono */
		objc_sysvar(0,BACKGRCOL,0,0,&bgcolor,&dummy);
		objc_sysvar(0,INDBUTCOL,0,0,&indcolor,&dummy);
	}

	if (strncmp(gettext(about,USERNAME),UNREGISTERED,sizeof(UNREGISTERED)-1) == 0) {
		xform_alert(1,REG_WARNING);
		return;
	}

	fix_rsrc();
	update_menu(dev);

	if (protect_error)						/* delayed check to make it harder to defeat ... */
		initial_msg_type = AP_TERM;			/* quit immediately ... */

	wind_get(0,WF_WORKXYWH,&fullx,&fully,&j,&fullh);
	fullw = 640;										//MAGNUM
	if (fullh > 480) {									//MAGNUM
		fullx = fullx + (j - fullw) / 2;
		fully = fully + (fullh - 480) / 2;				//MAGNUM
		fullh = 464;									//MAGNUM
	}
	init_forms();
	win1 = wind_create(PARTS,fullx,fully,fullw,fullh);
	if (win1 < 0) {
		xform_alert(1,GEN_NOHNDL_ALERT);
		cleanup_rsrc();
		return;
	}

	menu_bar(menu,TRUE);
	send_initial_message();

	set_window();			/* initialises workx etc */
	myform_center(bar);

	do_diskinfo();

	graf_mouse(ARROW,NULL);

	control();				/* call main routine */

	change_mode(IDLE);
	if (!windhidden)
		wind_close(win1);
	wind_delete(win1);
	menu_bar(menu,FALSE);
	cleanup_rsrc();
}

/*
 *	dynamically updates contents of appropriate menu entries with current drive letter
 *
 *	IMPORTANT: the menu entries *must* be set up with a '?' as the drive letter placeholder
 */
PRIVATE void update_menu(int dev)
{
static int first_time = TRUE;
static int doopt = -1, tdisk = -1, mapbad = -1;
char *p;

	/*
	 *	the first time through, we find & save the offsets to the drive letter position
	 */
	if (first_time) {
		p = strchr((char *)menu[DOOPT].ob_spec,'?');
		if (p)
			doopt = p - (char *)menu[DOOPT].ob_spec;
		p = strchr((char *)menu[TDISK].ob_spec,'?');
		if (p)
			tdisk = p - (char *)menu[TDISK].ob_spec;
		p = strchr((char *)menu[MAPBAD].ob_spec,'?');
		if (p)
			mapbad = p - (char *)menu[MAPBAD].ob_spec;
		first_time = FALSE;
	}

	if (doopt >= 0)
		*((char *)menu[DOOPT].ob_spec+doopt) = dev + 'A';
	if (tdisk >= 0)
		*((char *)menu[TDISK].ob_spec+tdisk) = dev + 'A';
	if (mapbad >= 0)
		*((char *)menu[MAPBAD].ob_spec+mapbad) = dev + 'A';
}

PRIVATE void send_initial_message(void)
{
short msg[8];

	msg[0] = initial_msg_type;		/* either MN_SELECTED or AP_TERM ... */
	msg[1] = ap_id;
	msg[2] = 0;
	msg[3] = FILEBAR;
	msg[4] = SHOW;
	msg[5] = msg[6] = msg[7] = 0;
	appl_write(ap_id,sizeof(msg),msg);
}

PRIVATE void control(void)
{
short event, dum;

//	form_dial(FMD_FINISH,0,0,0,0,workx,worky,workw,workh);		//useless?
	while (!byebye) {
//		assert(mouse_on()==FALSE);								//rfb: verify that this is unnecessary !!!
		event = evnt_multi(MU_KEYBD | MU_MESAG | MU_BUTTON,
							NUM_CLICKS, LEFT_BUTTON, BUTTON_DOWN,
							0, 0, 0, 0, 0,
							0, 0, 0, 0, 0,
							msg_buf, 0, 0,
							&mouse_x, &mouse_y, &dum, &keystate, &key, &num_clicks);

		menu_keyshort(menu,event,0x03);		/* convert keyboard shortcuts */

		if (event & MU_KEYBD)
			handle_keys();

		if (event & MU_MESAG)
			handle_messages();

		if (event & MU_BUTTON)
			handle_button();
	}
}

/*
 *	this routine is borrowed from windform
 *
 *	it scans the menu, and if a matching keycode abbreviation is found,
 *	it sends a message indicating that the corresponding menu item has
 *	been selected.
 *
 *	options are set by 'mode':
 *	bit 0: allows detection of ALTERNATE abbreviation as well as CONTROL
 *	bit 1: prevents inversion of the menu title
 */
PRIVATE void menu_keyshort(OBJECT *adr_menu,int evnt,int mode)
{
int title, titles_box, option, options_box;
char test;
short m_buff[8];
OBJECT *adr_option;
char *adr_text;

	if (!(evnt & MU_KEYBD))
		return;

	if (keystate == K_CTRL)
		test = '^';
	else if ((mode & 1) && (keystate == K_ALT))
		test = 0x07;
	else return;

	titles_box = adr_menu[adr_menu[ROOT].ob_head].ob_head;
	title = adr_menu[titles_box].ob_head;

	options_box = adr_menu[adr_menu[ROOT].ob_tail].ob_head;

	if (protect_error)		/* a little extra protection ... */
		key = 0;

	while (title != titles_box) {
		if (!(adr_menu[title].ob_state & DISABLED)) {
			option = adr_menu[options_box].ob_head;		/* NEW */
			while (option != options_box) {
				adr_option = &adr_menu[option];
				if (!(adr_option->ob_state & DISABLED)) {
					adr_text = NULL;
					switch(adr_option->ob_type & 0xff) {
						case G_STRING:
							adr_text = adr_option->ob_spec;
							break;
						case G_BOXTEXT:
						case G_TEXT:
							adr_text = ((TEDINFO *)adr_option->ob_spec)->te_ptext;
					}
					if (adr_text) {
						adr_text = strchr(adr_text,test);
						if (adr_text && (stdkey(key) == toupper(adr_text[1]))) {
							m_buff[0] = MN_SELECTED;
							m_buff[1] = ap_id;
							m_buff[2] = 0;
							m_buff[3] = title;
							m_buff[4] = option;
							*((OBJECT **) &m_buff[5]) = adr_menu;
							m_buff[7] = options_box;
							if (!(mode & 2)) 	/* inverse menu title allowed */
								menu_tnormal(adr_menu,title,0);
							appl_write(ap_id,16,m_buff);
							key = 0;
							return;		/* match found */
						}
					}
				}
				option = adr_option->ob_next;
			}
		}
		title = adr_menu[title].ob_next;
		options_box = adr_menu[options_box].ob_next;
	}

	/* else return with no match */
}	

/*
 *	convert keycode to character
 */
PRIVATE int stdkey(int key_code)
{
static KEYTAB *kt = NULL;

	if (!kt)
		kt = (KEYTAB *)Keytbl((void *)-1,(void *)-1,(void *)-1);

	return toupper(kt->shift[key_code>>8]);
}

/*
 *	handle keyboard
 *
 *	note: keys that are (ctl- & alt-) menu shortcut keys are not
 *	handled here, since they have already been converted to messages
 *	via menu_shortcut()
 */
PRIVATE void handle_keys(void)
{
char path[MAX_PATHNAME];
short newdev;
int rc;

	//rfb: what good did this do?
//	wind_get(win1,WF_TOP,&acthand,&dum,&dum,&dum);
//	if (acthand != win1)
//		if ((key != ALT_W) && (key != CNTRL_Q))
//			return;

	switch(key) {
	/*
	 *	the following keyboard shortcuts are otherwise selected via buttons
	 */
	case ALT_I:
		do_diskinfo();
		break;

	case ALT_M:
		do_diskmap();
		break;

	case ALT_A:
		do_chart(-1);
		break;

	case ALT_P:
		do_infoprint();
		break;

	/*
	 *	these are here because there is (at the moment) no function key
	 *	mapping in menu_keyshort() ... 
	 */
	case F1_keycode:
		sprintf(path,"%s%s",app_path,FILE_HELP);
		view_file(path,1);									//MAGNUM
		break;

	case F2_keycode:
		sprintf(path,"%s%s",app_path,MEDIC_HELP);
		view_file(path,1);									//MAGNUM
		break;

	case F3_keycode:
		sprintf(path,"%s%s",app_path,OPTIMIZE_HELP);
		view_file(path,1);									//MAGNUM
		break;

	case F4_keycode:
		sprintf(path,"%s%s",app_path,UNDELETE_HELP);
		view_file(path,1);									//MAGNUM
		break;

	case F5_keycode:
		sprintf(path,"%s%s",app_path,UTILITY_HELP);
		view_file(path,1);									//MAGNUM
		break;

	case CR:
	case ENTR:
		if ((status.action == UNDELETE) && (edit_obj == 0)) {
			objc_xsel(undel,UNDEL);
			undelete_files();
			objc_xdsel(undel,UNDEL);
			break;
		}
		/* drop thru if not UNDELETE, or edit_obj is non-zero */

	default:
		if ((status.action == UNDELETE) && edit_obj) {
			handle_mask();
			break;
		}
		newdev = get_drive();
		if (newdev < 0)
			break;

		/*
		 *	here we have a valid drive letter, so we switch drives
		 */
		lastdev = dev;
		dev = newdev;

		do {
			new_icon();
			update_menu(dev);
			switch(status.action) {
			case ALLINFO:
			case DISKINFO:
				rc = do_diskinfo();
				break;
			case DISKMAP:
				rc = do_diskmap();
				break;
			case UNDELETE:
				rc = init_undel();
				break;
			default:
				rc = 0;
			}
			if (dev == lastdev)		/* prevent infinite loop */
				break;
			if (rc < 0)
				dev = lastdev;
		} while(rc < 0);
		break;
	}

}

/*
 *	this code handles keys that aren't requests for drive switching
 *	or for file undeletion: we either do a find/findnext, or update
 *	the file mask
 */
PRIVATE void handle_mask()
{
short c, obj;
int i, len;
TEDINFO *ted;

	c = key & 0xff;
	if ((key == CR) || (key == ENTR)) {
		if (undel[FINDNEXT].ob_state & DISABLED)
			obj = UFIND;
		else obj = FINDNEXT;
		objc_edit(undel,edit_obj,0,&edpos,EDEND);
		objc_xsel(undel,obj);
		udo_find((obj==UFIND)?0:1);
		objc_xdsel(undel,obj);
		objc_edit(undel,edit_obj,0,&edpos,EDINIT);
	} else if ((key == DOT1) || (key == DOT2) || (key == SPACE)) {
		ted = (TEDINFO *) (undel+edit_obj)->ob_spec;
		len = strlen(ted->te_ptext);
		for (i = len; i < 7; i++)											//MAGNUM
			objc_edit(undel,edit_obj,SPACE,&edpos,EDCHAR);
	} else if ((key == QMARK) || (key == ESC) || (key == BS) || (key == DEL) || (key == RT_ARROW) || (key == LF_ARROW)) {
		objc_edit(undel,edit_obj,key,&edpos,EDCHAR);
	} else if (isalnum(c) || (c == '_')) {
		objc_edit(undel,edit_obj,key,&edpos,EDCHAR);
	}
}

/*
 *	check key code / state for valid drive & return drive number
 *
 *	returns -1 if drive invalid
 */
PRIVATE int get_drive()
{
int drive;

	if (keystate&(K_CTRL|K_ALT))
		return -1;

	drive = stdkey(key) - 'A';
	if ((drive < 0) || (drive >= NUM_DEVICES))
		return -1;

	if (!((1L<<drive) & drivemap))
		return -1;

	return drive;
}

/**************************************************************************/

PUBLIC void handle_messages(void)
{
	switch(msg_buf[0]) {
	case MN_SELECTED:
		do_menu();
		break;
	case WM_REDRAW:
		do_redraw(msg_buf[3],(GRECT *)&msg_buf[4]);
		break;
	case WM_CLOSED:
		wind_close(win1);
		windhidden = 1;
		menu_toggle();
		break;
	case WM_TOPPED:
		if (msg_buf[3] == win1)
			wind_set(win1,WF_TOP,fullx,fully,fullw,fullh);
		break;
	case WM_MOVED:
		if (msg_buf[3] == win1) {
			fullx = msg_buf[4] & ~(16-1);		/* align x to 16-bit boundary */
			fully = msg_buf[5];
			fullw = msg_buf[6];
			fullh = msg_buf[7];
			wind_set(win1,WF_CXYWH,fullx,fully,fullw,fullh);
			set_window();
		}
		break;
	case AP_TERM:
		byebye = TRUE;
		break;
	}
}

/***************************/
PRIVATE void menu_toggle()
{
	if (windhidden) {
		menu[PREF].ob_state |= DISABLED;
		menu[SHOW].ob_state &= ~DISABLED;
		menu[MEDICBAR].ob_state |= DISABLED;
		menu[OPTBAR].ob_state |= DISABLED;
		menu[UNDELBAR].ob_state |= DISABLED;
		menu[UTILBAR].ob_state |= DISABLED;
		menu[HELPBAR].ob_state |= DISABLED;
	} else {
		menu[PREF].ob_state &= ~DISABLED;
		menu[SHOW].ob_state |= DISABLED;
		menu[MEDICBAR].ob_state &= ~DISABLED;
		menu[OPTBAR].ob_state &= ~DISABLED;
		menu[UNDELBAR].ob_state &= ~DISABLED;
		menu[UTILBAR].ob_state &= ~DISABLED;
		menu[HELPBAR].ob_state &= ~DISABLED;
	}
	menu_bar(menu,TRUE);		//rfb: do we need this??
}

/**************************************************************************/
PRIVATE void do_menu(void)
{
char path[MAX_PATHNAME];
short acthand, dum;
short title, item;

	wind_get(win1,WF_TOP,&acthand,&dum,&dum,&dum);
	if (acthand != win1) {
		if ((msg_buf[3] == FILEBAR) && (msg_buf[4] == SHOW))
			;
		else if ((msg_buf[3] == FILEBAR) && (msg_buf[4] == BYEBYE))
			;
		else if (msg_buf[3] == DESKBAR)
			;
		else return;
	}

	title = msg_buf[3];		/* remember because msg_buf[] may get overwritten */
	item = msg_buf[4];

	byebye |= protect_error;	/* obscure, in case previous check was zapped */

	switch(title) {
	case DESKBAR:
		handle_dial(about,0,0,0);
		break;
	case FILEBAR:
		switch(item) {
		case BYEBYE:
			byebye = TRUE;
			break;
		case SHOW:
			if (!windhidden)
				break;
			windhidden = 0;
			menu_toggle();
			wind_open(win1,fullx,fully,fullw,fullh);
			form_dial(FMD_FINISH,0,0,0,0,workx,worky,workw,workh);
			break;
		case PREF:
			do_prefs(&opt);
			break;
		}
		break;
	case OPTBAR:
		switch(item) {
		case OFULL:
			menu[OFULL].ob_state |= CHECKED;
			menu[OCOMP].ob_state &= ~CHECKED;
			break;
		case OCOMP:
			menu[OCOMP].ob_state |= CHECKED;
			menu[OFULL].ob_state &= ~CHECKED;
			break;
		case OREAD:
			menu[OREAD].ob_state |= CHECKED;
			menu[OWRITE].ob_state &= ~CHECKED;
			break;
		case OWRITE:
			menu[OWRITE].ob_state |= CHECKED;
			menu[OREAD].ob_state &= ~CHECKED;
			break;
		case ORMDEL:
			menu[ORMDEL].ob_state ^= CHECKED;
			break;
		case OAUTOFIX:
			if (menu[OAUTOFIX].ob_state & CHECKED) {
				menu[OAUTOFIX].ob_state &= ~CHECKED;
				menu[MAUTOFIX].ob_state &= ~CHECKED;
			} else {
				menu[OAUTOFIX].ob_state |= CHECKED;
				menu[MAUTOFIX].ob_state |= CHECKED;
			}
			break;
		case OIGNORE:
			if (menu[OIGNORE].ob_state & CHECKED) {
				menu[OIGNORE].ob_state &= ~CHECKED;
				menu[MIGNORE].ob_state &= ~CHECKED;
			} else {
				menu[OIGNORE].ob_state |= CHECKED;
				menu[MIGNORE].ob_state |= CHECKED;
			}
			break;
		case OSAVLOST:
			if (menu[OSAVLOST].ob_state & CHECKED) {
				menu[OSAVLOST].ob_state &= ~CHECKED;
				menu[MSAVLOST].ob_state &= ~CHECKED;
			} else {
				menu[OSAVLOST].ob_state |= CHECKED;
				menu[MSAVLOST].ob_state |= CHECKED;
			}
			break;
		case OVISUAL:
			if (menu[OVISUAL].ob_state & CHECKED) {
				menu[OVISUAL].ob_state &= ~CHECKED;
				menu[MVISUAL].ob_state &= ~CHECKED;
			} else {
				menu[OVISUAL].ob_state |= CHECKED;
				menu[MVISUAL].ob_state |= CHECKED;
			}
			break;
		case DOOPT:
			set_opt();
			if (opt.warn)
				if (xform_alert(2,OP_RECOVER_ALERT) == 2)
					break;
			if (opt.novice)
				if (xform_alert(2,OP_WARN_ALERT) == 2)
					break;
			if (menu[OFULL].ob_state & CHECKED)
				do_opt();
			else do_opt_compress(1);							//MAGNUM
			break;
		case DOOPTM:		/* multiple optimisations */
			set_opt();
			do_multi(OPTM);
			break;
		}
		break;
	case UNDELBAR:
		switch(item) {
		case UDATEUS:
			menu[UDATEUS].ob_state |= CHECKED;
			menu[UDATEUK].ob_state &= ~CHECKED;
			if (opt.datemode != US) {
				opt.datemode = US;
				if (status.action == UNDELETE) {
					load_ulines(-1);
					redraw_object(undel,UDROP);
				}
			}
			break;
		case UDATEUK:
			menu[UDATEUK].ob_state |= CHECKED;
			menu[UDATEUS].ob_state &= ~CHECKED;
			if (opt.datemode != UK) {
				opt.datemode = UK;
				if (status.action == UNDELETE) {
					load_ulines(-1);
					redraw_object(undel,UDROP);
				}
			}
			break;
		case UCONFIG:
			config_mirror();
			break;
		case DOUNDEL:
			init_undel();
			break;
		case USAVE:
			save_disk_info();
			break;
		case USSCSI:
			save_scsi_info();
			break;
		case URESTORE:
			restore_disk_info(0);				//MAGNUM
			break;
		case URSCSI:
			restore_scsi_info();
			break;
		}
		break;
	case MEDICBAR:
		switch(item) {
		case MPART:
			do_part();
			break;
		case CCRC:
			do_valid(MAKEVAL,(CRC<<4)|opt.noheader);
			break;
		case CCSUM:
			do_valid(MAKEVAL,(CHECKSUM<<4)|opt.noheader);
			break;
		case VALIDATE:
			do_valid(CHECKVAL,0);
			break;
		case VALIDM:			/* multiple validations */
			do_multi(VALM);
			break;
		case TDISK:
			set_test();
			do_tdisk(0);				//MAGNUM
			force_update(-1);
			break;
		case TDISKM:			/* multiple tests */
			set_test();
			do_multi(TESTM);
			break;
		case IGNOREPH:
			menu[IGNOREPH].ob_state ^= CHECKED;
			opt.noheader = (menu[IGNOREPH].ob_state&CHECKED) ? 1 : 0;
			break;
		case MAPBAD:
			if (bad_prefs())
				break;
			do_bad(menu[MAUTOFIX].ob_state & CHECKED);
			break;
		case MAPBADM:			/* multiple mark bad */
			if (bad_prefs())
				break;
			do_multi(MBADM);
			break;
		case MAUTOFIX:
			if (menu[MAUTOFIX].ob_state & CHECKED) {
				menu[OAUTOFIX].ob_state &= ~CHECKED;
				menu[MAUTOFIX].ob_state &= ~CHECKED;
			} else {
				menu[OAUTOFIX].ob_state |= CHECKED;
				menu[MAUTOFIX].ob_state |= CHECKED;
			}
			break;
		case MIGNORE:
			if (menu[MIGNORE].ob_state & CHECKED) {
				menu[OIGNORE].ob_state &= ~CHECKED;
				menu[MIGNORE].ob_state &= ~CHECKED;
			} else {
				menu[OIGNORE].ob_state |= CHECKED;
				menu[MIGNORE].ob_state |= CHECKED;
			}
			break;
		case MSAVLOST:
			if (menu[MSAVLOST].ob_state & CHECKED) {
				menu[OSAVLOST].ob_state &= ~CHECKED;
				menu[MSAVLOST].ob_state &= ~CHECKED;
			} else {
				menu[OSAVLOST].ob_state |= CHECKED;
				menu[MSAVLOST].ob_state |= CHECKED;
			}
			break;
		case MVISUAL:
			if (menu[MVISUAL].ob_state & CHECKED) {
				menu[OVISUAL].ob_state &= ~CHECKED;
				menu[MVISUAL].ob_state &= ~CHECKED;
			} else {
				menu[OVISUAL].ob_state |= CHECKED;
				menu[MVISUAL].ob_state |= CHECKED;
			}
			break;
		}
		break;
	case UTILBAR:
		switch(item) {
		case FMEDIAC:
			_mediach(dev);
			force_update(dev);
			break;
		case COPYIMG:
			do_copyimage();
			break;
		case COPYDFRG:
			do_copydefrag();
			break;
		case UVIEW:
			view_file(NULL,1);									//MAGNUM
			break;
		case UPRINT:
			print_file();
			break;
		case UBROWSE:
			browse();
			break;
		case EZERO:
			if (menu[ZERO].ob_state & DISABLED) {
				menu[ZERO].ob_state &= ~DISABLED;
				menu[WIPE].ob_state &= ~DISABLED;
				menu[UNZERO].ob_state &= ~DISABLED;
				strcpy(menu[EZERO].ob_spec,text[ZERO_DISABLE].ob_spec);
				opt.disablezero = FALSE;
			} else {
				menu[ZERO].ob_state |= DISABLED;
				menu[WIPE].ob_state |= DISABLED;
				menu[UNZERO].ob_state |= DISABLED;
				strcpy(menu[EZERO].ob_spec,text[ZERO_ENABLE].ob_spec);
				opt.disablezero = TRUE;
			}
			break;
		case ZERO:
			if (opt.warn || opt.novice)
				if (xform_alert(2,ZR_WARN_ALERT,'A'+dev) == 2)
					break;
			do_zero();
			break;
		case UNZERO:
			do_unzero();
			break;
		case WIPE:
			if (opt.warn || opt.novice)
				if (xform_alert(2,WP_WARN_ALERT,'A'+dev,'A'+dev) == 2)
					break;
			do_wipe();
			break;
		}
		break;
	case HELPBAR:
		switch(item) {
		case HFILE:
			sprintf(path,"%s%s",app_path,FILE_HELP);
			view_file(path,1);									//MAGNUM
			break;
		case HOPT:
			sprintf(path,"%s%s",app_path,OPTIMIZE_HELP);
			view_file(path,1);									//MAGNUM
			break;
		case UNDELHLP:
			sprintf(path,"%s%s",app_path,UNDELETE_HELP);
			view_file(path,1);									//MAGNUM
			break;
		case HMEDIC:
			sprintf(path,"%s%s",app_path,MEDIC_HELP);
			view_file(path,1);									//MAGNUM
			break;
		case HUTIL:
			sprintf(path,"%s%s",app_path,UTILITY_HELP);
			view_file(path,1);									//MAGNUM
			break;
		}
		break;
	}
	menu_tnormal(menu,title,TRUE);
	menu_tnormal(menu,item,TRUE);
}

PRIVATE void handle_button(void)
{
short obj, obj1, savedev;
short acthand, exitobj;
short buttondown, dum, i, xx, x, y;
short mstate, kstate;
int rc;

	obj = obj1 = -1;
	obj = objc_find(bottom,0,MAX_DEPTH,mouse_x,mouse_y);
	obj1 = objc_find(undel,0,MAX_DEPTH,mouse_x,mouse_y);

	wind_get(win1,WF_TOP,&acthand,&dum,&dum,&dum);
	if (acthand != win1) {
		ring_bell();
		return;
	}
	graf_mkstate(&xx,&y,&buttondown,&i);
	if (obj > 0) {
		if (bottom[obj].ob_state & SELECTED)
			return;
		if ((bottom[obj].ob_flags & SELECTABLE) && buttondown) {
			if (!mywatchbox(bottom,obj,bottom[obj].ob_state^SELECTED,bottom[obj].ob_state))		/* He gave up...  */
				return;
			buttondown = 0;
		}
	}

	switch(obj) {
	case BDINFO:
		do_diskinfo();
		break;

	case BFRAGMAP:
		do_diskmap();
		break;

	case BALLINFO:
		do_chart(-1);
		break;

	case BPRINT:
		do_infoprint();
		break;

	case BDOOPT:
		set_opt();
		if (opt.warn)
			if (xform_alert(2,OP_RECOVER_ALERT) == 2)
				break;
		if (opt.novice)
			if (xform_alert(2,OP_WARN_ALERT) == 2)
				break;
		if (menu[OFULL].ob_state & CHECKED)
			do_opt();
		else do_opt_compress(1);					//MAGNUM
		break;

	case BTESTD:
		set_test();
		do_tdisk(0);								//MAGNUM
		force_update(-1);
		break;

	case BUNDEL:
		init_undel();
		break;

	case BACTIVEF:
	case BACTIVEH:
		pcsel[0].ob_x = mouse_x;
		pcsel[0].ob_y = mouse_y - pcsel[0].ob_height;
		strcpy(pcsel[PCNAME].ob_spec,text[PCSEL_ACTIVE].ob_spec);
		exitobj = do_dial(pcsel,0,0,1);
		if ((exitobj == 0) || ((exitobj - PCA) == dev)) {
			objc_xdsel(bottom,obj);
			break;
		}
		savedev = dev;
		dev = exitobj - PCA;

		do {
			new_icon();
			update_menu(dev);
			switch(status.action) {
			case ALLINFO:
			case DISKINFO:
				rc = do_diskinfo();
				break;
			case DISKMAP:
				rc = do_diskmap();
				break;
			case UNDELETE:
				rc = init_undel();
				break;
			default:
				rc = 0;
			}
			if (dev == savedev)			/* precaution against infinite loop */
				break;
			if (rc < 0)
				dev = savedev;
		} while (rc < 0);
		break;
	}

	if (status.action == UNDELETE) {
		switch(obj1) {
		case USMASK:
			if (edit_obj == 0) {
				edit_obj = USMASK;
				objc_edit(undel,edit_obj,0,&edpos,EDINIT);
			}
			break;
		case UDSIM:
			objc_xsel(undel,UDSIM);
			objc_xdsel(undel,UDMIR);
			opt.usemirror = FALSE;
			init_uroot(dev,1);				//MAGNUM
			break;
		case UDMIR:
			objc_xdsel(undel,UDSIM);
			objc_xsel(undel,UDMIR);
			opt.usemirror = TRUE;
			init_uroot(dev,1);				//MAGNUM
			break;
		case UFIND:
			objc_xsel(undel,obj1);
			udo_find(0);					//MAGNUM
			objc_xdsel(undel,obj1);
			break;
		case FINDNEXT:
			objc_xsel(undel,obj1);
			udo_find(1);					//MAGNUM
			objc_xdsel(undel,obj1);
			break;
		case UNDEL:
			objc_xsel(undel,obj1);
			undelete_files();
			objc_xdsel(undel,obj1);
			break;
		case UUP:
			objc_xsel(undel,obj1);
			udo_up1();
			objc_xdsel(undel,obj1);
			break;
		case UDOWN:
			objc_xsel(undel,obj1);
			udo_down1();
			objc_xdsel(undel,obj1);
			break;
		case UBARBACK:
			objc_offset(undel,UBARTOP,&x,&y);
			if (mouse_y < y)
				udo_up_page();
			else udo_down_page();
			break;
		case UBARTOP:
			graf_mkstate(&x,&y,&mstate,&kstate);
			if (mstate & 0x01) {
				kstate = graf_slidebox(undel,UBARBACK,UBARTOP,1);
				udo_random_page(kstate);
			}
			break;
		case UPARENT:
			objc_xsel(undel,obj1);
			udo_parent();
			objc_xdsel(undel,obj1);
			break;
		default:
			if ((obj1 >= ULINE0) && (obj1 <= ULINE15))
				udo_line(obj1,num_clicks,keystate);
			else if (edit_obj) {
				objc_edit(undel,edit_obj,0,&edpos,EDEND);
				edit_obj = 0;
			}
			break;
		}
	}
}

/**************************************************************************/

PRIVATE void do_redraw(short hand,GRECT *rec1)
{
GRECT rec2;

	wind_update(BEG_UPDATE);
	mouse_off();
	wind_get(hand,WF_FIRSTXYWH,&rec2.g_x,&rec2.g_y,&rec2.g_w,&rec2.g_h);

	while (rec2.g_w && rec2.g_h) {
		if (rc_intersect(rec1,&rec2))
			redraw(hand,rec2);
		wind_get(hand,WF_NEXTXYWH,&rec2.g_x,&rec2.g_y,&rec2.g_w,&rec2.g_h);
	}
	mouse_on();
	wind_update(END_UPDATE);
}

/***************************************************************************/
PRIVATE void redraw(short h,GRECT rect)
{
	if (h == win1) {
		switch(status.action) {
		case ALLINFO:
			objc_draw(chart,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			objc_draw(bottom,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			break;
		case DISKINFO:
			objc_draw(dinfo,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			objc_draw(bottom,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			break;
		case DISKMAP:
			objc_draw(dmap,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			restore_map(rect);
			objc_draw(bottom,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			break;
		case UNDELETE:
			if (edit_obj)
				objc_edit(undel,edit_obj,0,&edpos,EDEND);
			objc_draw(undel,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			objc_draw(bottom,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			if (edit_obj)
				objc_edit(undel,edit_obj,0,&edpos,EDINIT);
			break;
		case ESCSI:
			objc_draw(scsiedit,0,MAX_DEPTH,rect.g_x,rect.g_y,rect.g_w,rect.g_h);
			break;
		}
	}
}

/***************************************************************************/
PRIVATE void set_opt()
{
	if (menu[OWRITE].ob_state & CHECKED)
		opt.priority = WRITE;
	else opt.priority = READ;

	if (menu[ORMDEL].ob_state & CHECKED)
		opt.rmdel = TRUE;
	else opt.rmdel = FALSE;

	if (menu[OAUTOFIX].ob_state & CHECKED)
		opt.autofix = TRUE;
	else opt.autofix = FALSE;

	if (menu[OIGNORE].ob_state & CHECKED)
		opt.ignore = TRUE;
	else opt.ignore = FALSE;

	if (menu[OSAVLOST].ob_state & CHECKED)
		opt.savelost = TRUE;
	else opt.savelost = FALSE;

	if (menu[OVISUAL].ob_state & CHECKED)
		opt.visual = TRUE;
	else opt.visual = FALSE;
}

/*********************************************************************/
PRIVATE void set_test()
{
	if (menu[MAUTOFIX].ob_state & CHECKED)
		opt.autofix = TRUE;
	else opt.autofix = FALSE;

	if (menu[MIGNORE].ob_state & CHECKED)
		opt.ignore = TRUE;
	else opt.ignore = FALSE;

	if (menu[MSAVLOST].ob_state & CHECKED)
		opt.savelost = TRUE;
	else opt.savelost = FALSE;

	if (menu[MVISUAL].ob_state & CHECKED)
		opt.visual = TRUE;
	else opt.visual = FALSE;
}

/*********************************************************************/
PRIVATE void do_multi(short type)
{
short j, k, savedev, lastdev, ret;
int rc;
char *titlep, *buttonp, *alertp;

	savedev = lastdev = dev;

	myform_center(multi);
	for (j = 0; j < NUM_DEVICES; j++) {
		if ((1L<<j) & drivemap & opt.multi)
			multi[AMULTI+j].ob_state |= SELECTED;
		else multi[AMULTI+j].ob_state &= ~SELECTED;
	}

	switch(type) {
	case OPTM:
		titlep = text[MULTI_OPT_TITLE].ob_spec;
		buttonp = text[MULTI_OPT_BUTTON].ob_spec;
		alertp = text[MULTI_OPT_ALERT].ob_spec;
		break;
	case TESTM:
		titlep = text[MULTI_TST_TITLE].ob_spec;
		buttonp = text[MULTI_TST_BUTTON].ob_spec;
		alertp = text[MULTI_TST_ALERT].ob_spec;
		break;
	case MBADM:
		titlep = text[MULTI_MAP_TITLE].ob_spec;
		buttonp = text[MULTI_MAP_BUTTON].ob_spec;
		alertp = text[MULTI_MAP_ALERT].ob_spec;
		break;
	case VALM:
		titlep = text[MULTI_VAL_TITLE].ob_spec;
		buttonp = text[MULTI_VAL_BUTTON].ob_spec;
		/*
		 * because the rc is always set to zero for VALM (see below),
		 * we will never currently issue this version of the alert
		 */
		alertp = text[MULTI_VAL_ALERT].ob_spec;
		break;
	}
	strcpy(multi[MULTOP].ob_spec,titlep);
	strcpy(multi[DOMULT].ob_spec,buttonp);

	do {
		obj_head(multi);
		while(TRUE) {
			j = 0;
			ret = myform_do(multi,&j,0) & 0x7fff;
			multi[ret].ob_state &= ~SELECTED;
			redraw_object(multi,ret);
			if (ret == MCLEAR) {
				for (j = 0; j < NUM_DEVICES; j++) {
					if (multi[AMULTI+j].ob_state&SELECTED) {
						multi[AMULTI+j].ob_state &= ~SELECTED;
						redraw_object(multi,AMULTI+j);
					}
				}
			} else if (ret == MSELALL) {
				for (j = FIRST_HD_DEVICE; j < NUM_DEVICES; j++) {
					if (((1L<<j) & drivemap)) {
						multi[AMULTI+j].ob_state |= SELECTED;
						redraw_object(multi,AMULTI+j);
					}
				}
			} else break;
		}
		refresh_now(multi,0);
		if (ret != DOMULT)
			return;

		k = 0;
		for (j = 0; j < NUM_DEVICES; j++)
			if (multi[AMULTI+j].ob_state & SELECTED)
				k++;
		if (!k)
			xform_alert(1,GEN_PART_ALERT);
	} while (k == 0);

	if (type == OPTM) {
		if (opt.warn)
			if (xform_alert(2,OP_RECOVER_ALERT) == 2)
				return;
		if (opt.novice)
			if (xform_alert(2,OP_WARN_ALERT) == 2)
				return;
	}

	for (j = 0; j < NUM_DEVICES; j++) {
		if (!(multi[AMULTI+j].ob_state&SELECTED))
			continue;

		dev = j;
		new_icon();
		lastdev = dev;

		switch(type) {
		case OPTM:
			set_opt();
			if (menu[OFULL].ob_state & CHECKED)
				rc = do_opt();
			else rc = do_opt_compress(0);
			break;
		case TESTM:
			set_test();
			rc = do_tdisk(1);							//MAGNUM
			break;
		case MBADM:
			rc = do_bad(menu[MAUTOFIX].ob_state & CHECKED);
			break;
		case VALM:
			do_valid(CHECKVAL,0);
			rc = 0;
			break;
		}

		if (rc < 0)
			if (xform_alert(1,ML_CONT_ALERT,alertp) == 2)
				break;
	}

	dev = savedev;
	new_icon();

	if (get_mybpb(&dpb,dev) < 0)
		xform_alert(1,GEN_BPB_ALERT);

	force_update(-1);

	for (j = FIRST_HD_DEVICE, opt.multi = 0UL; j < NUM_DEVICES; j++)
		if (multi[AMULTI+j].ob_state & SELECTED)
			opt.multi |= 1 << j;
}

/*******************************************************************/
PRIVATE void set_window(void)
{
	wind_calc(WC_WORK,PARTS,fullx,fully,fullw,fullh,&workx,&worky,&workw,&workh);

	bottom->ob_x = workx;
	bottom->ob_y = worky + workh - 4 * ychar;		//MAGNUM: the 4*ychar is the height of the BOTTOM dialog
	bottom->ob_width = workw;

	set_work(dinfo);
	set_work(undel);
	set_work(chart);

	obar->ob_x = workx;
	obar->ob_y = worky;

	set_work(dmap);

	objc_offset(dmap,MAPAREA,&gmap.g_x,&gmap.g_y);

	set_work(medic);

	scsiedit->ob_x = workx;
	scsiedit->ob_y = worky;
	scsiedit->ob_width = workw;
	scsiedit->ob_height = workh;
}

/************************************************************************************/
PRIVATE void set_work(OBJECT *tree)
{
int extra;

	tree->ob_x = workx;
	tree->ob_y = worky;
	tree->ob_width = workw;

	/*
	 *	the following was changed to avoid screen residues in the display window under MagiC ...
	 */
	if (!MagiC && (bottom[0].ob_flags&FL3DMASK))
		extra = 3;
	else extra = 1;

	tree->ob_height = workh - 4 * ychar - extra;
}
