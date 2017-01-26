/*
 *	install: install Diamond Edge
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

/*
 *	version 2.5	roger burrows	26/april/2003
 *		totally rewritten to implement new protection scheme,
 *		and numbered to correspond to Edge
 *
 *	version 2.57 roger burrows	18/January/2017
 *		disable protection scheme for public release
 */
#include <stdio.h>
#include <stdlib.h>

#include <aes.h>
#include <dos.h>
#include <portab.h>
#include <string.h>
#include <unistd.h>
#include <vdi.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "install.h"
#include "../lib/encrypt.h"

#define NOPROTECT		/* disable protection */

#ifdef NOPROTECT
#define NOPROTECT_SERNUM	"E-0000-25"
#define NOPROTECT_REGCODE	"DSFMDSCN"
#endif

/*
 *	typedefs
 */
typedef struct {
	short magic;
	long textlen;
	long datalen;
	long bsslen;
	long symlen;
	long reserved;
	long flags;
	short absflg;
} PROGRAM_HEADER;


/*
 *	manifest constants */
#define	_bootdev		0x446

#define PROGRAM_MAGIC	0x601a

								/* file extensions */
#define	ACTIVE_ACC		"ACC"		/* standard */
#define	ACTIVE_CPX		"CPX"
#define	ACTIVE_PRG		"PRG"
#define	INACTIVE_ACC	"ACX"		/* these are quasi-standard */
#define	INACTIVE_CPX	"CPZ"
#define	INACTIVE_PRG	"PRX"
#define	BACKUP_EXT		"BAK"


/*
 *	filenames and paths
 */
#define PROGRAM_NAME	"INSTALL.PRG"
#define RESOURCE_NAME	"INSTALL.RSC"
#define EDGE_NAME		"EDGE.PRG"
#define EDGERSC_NAME	"EDGE.RSC"
#define PREFS_FILE		"EDGE.INF"
#define MIRROR_FILE		"DMIRROR.PRG"
#define FILE_HELP		"FILE.HLP"
#define MEDIC_HELP		"MEDIC.HLP"
#define OPTIMIZE_HELP	"OPTIMIZE.HLP"
#define UNDELETE_HELP	"UNDELETE.HLP"
#define UTILITY_HELP	"UTILITY.HLP"
#define EDGE_PATH		"%c:\\EDGE"
#define HELP_PATH		EDGE_PATH "\\HELP"
#define AUTO_PATH		"%c:\\AUTO"


/*
 *	alerts
 */
#define NO_RSC_ALERT		"[3][Can't load " RESOURCE_NAME "][ Quit ]"


/*
 *	other defines
 */
#define GENERAL_ERROR	-1
#define REGINFO_ERROR	-2
#define MAXPATH			256		/* should be big enough for our purposes ... */
#define MAXNAME			128

/*
 *	the following are dependent on the RSC file
 */
#define	LOG_LINES		12		/* must agree with number of lines in LOG_DIALOG */
#define	LOG_LINE_LENGTH	48		/* must agree with actual length in LOG_DIALOG */



/*
 *	globals
 */
int cur_drive;
int boot_drive;						/* drive nnumber */
long drivemap;
int protect_error = FALSE;

short myhandle, rez;
short work_in[11];	/* Input to GSX parameter array */
short work_out[57];	/* Output from GSX parameter array */

short Xw, Yh;

OBJECT *install, *log_dialog, *final, *text_strings;
TEDINFO *username, *address1, *address2, *address3, *sernum, *regcode;

char source_dir[MAXPATH], auto_dir[MAXPATH];


/*
 *	function prototypes
 */
int check_install(void);
int check_string(FILE *fp,long seekpos,char *string);
int copy_file(char *src,char *dest,char *name,char *newname);
int do_install(int dev);
void get_curr_path(char *curr_path);
char *get_ext(char *fname);
void get_prg_path(char *app_path,char *app_name);
void issue_alert(int index);
unsigned long get_sys(char **address);
int install_file(char *src,char *dest,char *name,char *newname);
int isdir(char *dir);
int modify_string(FILE *fp,long seekpos,char *string);
void objc_head(OBJECT *tree);
int open_workstation(void);
void print_log_line(char *line);
void quit(void);
void redraw_object(OBJECT *tree,short idex);
int register_file(char *dest,char *pgm,char *rsc);


/*
 *	macros
 */
							/* return pointer to text of TEDINFO object */
#define	gettext(tree,which)		((char *)(((TEDINFO *)tree[which].ob_spec)->te_ptext))
							/* turn on the HIDETREE attribute bit of the specified object */
#define	hide_obj(tree,which)	(tree[which].ob_flags |= HIDETREE)
							/* turn off the HIDETREE attribute bit of the specified object */
#define	unhide_obj(tree,which)	(tree[which].ob_flags &= ~HIDETREE)


int main(int argc,char **argv)
{
int i, rc;
long mask;
short mret;
short dev = -1;
int rerror = 0;

	appl_init();
	myhandle = open_workstation();

	boot_drive = get_sys((char **)_bootdev) >> 16;
	cur_drive = getdsk();
	drivemap = chgdsk(cur_drive);

	get_prg_path(source_dir,PROGRAM_NAME);

	if (!rsrc_load(RESOURCE_NAME)) {
		form_alert(1,NO_RSC_ALERT);
		quit();
	}

	rsrc_gaddr(R_TREE,MAIN,&install);
	rsrc_gaddr(R_TREE,LOG,&log_dialog);
	rsrc_gaddr(R_TREE,FINAL,&final);
	rsrc_gaddr(R_TREE,TEXT_STRINGS,&text_strings);

	if (Xw < 639) {					//FIXME: is this necessary???
		issue_alert(WRONGREZ_ALERT);
		quit();
	}

	form_center(install,&mret,&mret,&mret,&mret);
	form_center(log_dialog,&mret,&mret,&mret,&mret);
	form_center(final,&mret,&mret,&mret,&mret);

	username = (TEDINFO *)install[USERNAME].ob_spec;
	address1 = (TEDINFO *)install[ADDRESS1].ob_spec;
	address2 = (TEDINFO *)install[ADDRESS2].ob_spec;
	address3 = (TEDINFO *)install[ADDRESS3].ob_spec;
	sernum = (TEDINFO *)install[SERNUM].ob_spec;
	regcode = (TEDINFO *)install[REGCODE].ob_spec;

	graf_mouse(ARROW,NULL); 

	*username->te_ptext = '\0'; 		/* Initialize the user's strings */
	*address1->te_ptext = '\0';
	*address2->te_ptext = '\0';
	*address3->te_ptext = '\0';

	/*
	 *	ensure current drive is not selectable iff it's a floppy
	 */
	if (cur_drive < 2)
		drivemap &= ~(1<<cur_drive);

	for (i = ADRIVE, mask = 1L; i < (ADRIVE+16); i++, mask <<= 1) {
		if (drivemap & mask)
			install[i].ob_state &= ~SELECTED;
		else install[i].ob_state |= DISABLED;
	}

	/*
	 *	select default drive, in the following preference order:
	 *		boot drive
	 *		lowest hard disk partition
	 *		lowest floppy
	 */
	if (drivemap & (1<<boot_drive))
		dev = boot_drive;
	else {
		for (i = 2; i < 16; i++)
			if (drivemap & (1<<i))
				break;
		if (i >= 16)
			for (i = 0; i < 2; i++)
				if (drivemap & (1<<i))
					break;
		dev = i;
	}
	install[ADRIVE+dev].ob_state |= SELECTED;

	while(TRUE) {
		*sernum->te_ptext = '\0';
		*regcode->te_ptext = '\0';
		objc_head(install);
		mret = form_do(install,USERNAME) & 0x7fff;
		install[mret].ob_state &= ~SELECTED;
		redraw_object(install,mret);
		if (mret == INSTCAN)
			break;

		dev = check_install();
		if (dev < 0) {
			if (dev == REGINFO_ERROR) {
				if (++rerror >= 3)
					break;
				issue_alert(REGINFO_ALERT);
			}
			continue;
		}

		rc = do_install(dev);
		if (rc < 0)
			print_log_line(text_strings[INSTALL_FAILED].ob_spec);
		else print_log_line(text_strings[INSTALL_SUCCESS].ob_spec);

		unhide_obj(log_dialog,LOG_BUTTON_BOX);	/* show this now we're done ... */
		redraw_object(log_dialog,LOG_BUTTON_BOX);
		form_do(log_dialog,0);

		if (rc == 0) {
			objc_head(final);
			form_do(final,0);
		}
		break;
	}

	quit();

	return 0;
}


/***********************************************************/
int open_workstation()
{
int x;
short handle;
short width, height, dummy;

	 work_in[0] = Getrez() + 2;
	 for (x = 1; x < 10; work_in[x++] = 1)
	 	;
	 work_in[10] = 2;
	 handle = graf_handle(&width,&height,&dummy,&dummy);
	 v_opnvwk(work_in,&handle,work_out);
	 if (!handle)
	 	_exit(0);

	 rez = height/8;

	 Xw = work_out[0];
	 Yh = work_out[1];

	 return handle;
}

/*
 *	returns	>=0	selected device
 *			<0	retry
 */
int check_install(void)
{
int i, dev;
unsigned long hexreg;

#ifdef NOPROTECT
	strcpy(address1->te_ptext," ");
	strcpy(address2->te_ptext," ");
	strcpy(address3->te_ptext," ");
	strcpy(sernum->te_ptext,NOPROTECT_SERNUM);
	strcpy(regcode->te_ptext,NOPROTECT_REGCODE);
#endif

	hexreg = external_to_hex(regcode->te_ptext);

	for (i = ADRIVE, dev = -1; i < (ADRIVE+16); i++) {
		if (install[i].ob_state & SELECTED) {
			dev = i - ADRIVE;
			break;
		}
	}

	if (encrypt(sernum->te_ptext,PREFS_FILE) != hexreg) {	/* no match ? */
		*(regcode->te_ptext + 6) = '\0';					/* force error in regcode ... */
		protect_error = TRUE;								/* double checked in do_install() */
	}

	if (!*username->te_ptext
	 ||	!*address1->te_ptext
	 ||	!*address2->te_ptext
	 ||	!*address3->te_ptext
	 ||	!*sernum->te_ptext
	 ||	!*regcode->te_ptext) {
		issue_alert(INCOMPLETE_ALERT);
		return GENERAL_ERROR;
	}

	if (strncmp(sernum->te_ptext,"E-",2) || strcmp(sernum->te_ptext+6,"-25") || (strlen(regcode->te_ptext) != 8))
		return REGINFO_ERROR;

	return dev;
}

int do_install(int dev)
{
char target_dir[MAXPATH];
char msg[200], *p;
int i;

	sprintf(target_dir,EDGE_PATH,'A'+dev);
	p = gettext(log_dialog,TARGET_DIRECTORY);	/* initialise dialog */
	strncpy(p,target_dir,strlen(p));
	for (i = 0; i < LOG_LINES; i++)
		*gettext(log_dialog,LOG_ENTRY+i) = '\0';
	hide_obj(log_dialog,LOG_BUTTON_BOX);		/* hidden until done ... */
	objc_head(log_dialog);

	if (install_file(source_dir,target_dir,EDGE_NAME,NULL) < 0)
		return GENERAL_ERROR;
	if (install_file(source_dir,target_dir,EDGERSC_NAME,NULL) < 0)
		return GENERAL_ERROR;

	sprintf(msg,text_strings[REGISTERING].ob_spec,EDGE_NAME);
	print_log_line(msg);
	if (register_file(target_dir,EDGE_NAME,EDGERSC_NAME) == 0)
		print_log_line(text_strings[LOG_OK].ob_spec);
	else {
		print_log_line(text_strings[LOG_FAILED].ob_spec);
		return GENERAL_ERROR;
	}

	sprintf(target_dir,HELP_PATH,'A'+dev);
	if (install_file(source_dir,target_dir,FILE_HELP,NULL) < 0)
		return GENERAL_ERROR;
	if (install_file(source_dir,target_dir,MEDIC_HELP,NULL) < 0)
		return GENERAL_ERROR;
	if (install_file(source_dir,target_dir,OPTIMIZE_HELP,NULL) < 0)
		return GENERAL_ERROR;
	if (install_file(source_dir,target_dir,UNDELETE_HELP,NULL) < 0)
		return GENERAL_ERROR;
	if (install_file(source_dir,target_dir,UTILITY_HELP,NULL) < 0)
		return GENERAL_ERROR;

	if (install[MIRYES].ob_state&SELECTED) {
		sprintf(target_dir,AUTO_PATH,'A'+boot_drive);
		if (install_file(source_dir,target_dir,MIRROR_FILE,NULL) < 0)
			return GENERAL_ERROR;
	}

	return 0;
}

/*
 * install_file:
 *	copy an individual file to a directory, optionally renaming it,
 *	and renaming any existing file to a backup 
 *
 *	in order to handle the standard way of temporarily deactivating
 *	files by renaming them, the backup algorithm is as follows:
 *		. if a corresponding deactivated file exists, delete any
 *		  .BAK file, rename the deactivated file to .BAK
 *		. if a corresponding activated file exists, delete any
 *		  .BAK file, rename the activated file to .BAK
 *	the details vary by file extension:
 *		activated	deactivated
 *			ACC			ACX
 *			CPX			CPZ
 *			PRG			PRX
 *			other		n/a
 *
 *	returns 0 if ok, else -ve error code
 */
int install_file(char *src,char *dest,char *name,char *newname)
{
char target_path[MAXPATH], backup_path[MAXPATH], inactive_path[MAXPATH];
char inactive[MAXNAME];
char *install, *inactive_ext, *install_ext;
char msg[200];
int rc;

	install = newname?newname:name;			/* point to name to install */

	if (!isdir(dest)) {						/* directory doesn't exist */
		sprintf(msg,text_strings[CREATING_DIR].ob_spec,dest);
		print_log_line(msg);
		if (mkdir(dest,0) == 0)
			print_log_line(text_strings[LOG_OK].ob_spec);
		else print_log_line(text_strings[LOG_FAILED].ob_spec);
	}

	strmfp(target_path,dest,install);			/* get full name of file to install */
	strmfe(backup_path,target_path,BACKUP_EXT);	/*  & name of putative backup       */

	install_ext = get_ext(install);				/* the following sets inactive_ext -> extension of inactive version of current module */
	if (strcmp(install_ext,ACTIVE_ACC) == 0)
		inactive_ext = INACTIVE_ACC;
	else if (strcmp(install_ext,ACTIVE_CPX) == 0)
		inactive_ext = INACTIVE_CPX;
	else if (strcmp(install_ext,ACTIVE_PRG) == 0)
		inactive_ext = INACTIVE_PRG;
	else inactive_ext = NULL;

	if (inactive_ext) {								/* if there is an inactive version: */
		strmfe(inactive_path,target_path,inactive_ext);	/* create full pathname                */
		if (access(inactive_path,0) == 0) {				/* if inactive version exists:         */
			remove(backup_path);							/* remove any backup that exists */
			strmfe(inactive,install,inactive_ext);
			sprintf(msg,text_strings[RENAMING].ob_spec,inactive,BACKUP_EXT);
			print_log_line(msg);
			if (rename(inactive_path,backup_path) == 0)		/* & rename inactive to BAK      */
				print_log_line(text_strings[LOG_OK].ob_spec);
			else print_log_line(text_strings[LOG_FAILED].ob_spec);
		}
	}

	if (access(target_path,0) == 0) {				/* if active version exists: */
		remove(backup_path);							/* remove any backup that exists */
		sprintf(msg,text_strings[RENAMING].ob_spec,install,BACKUP_EXT);
		print_log_line(msg);
		if (rename(target_path,backup_path) == 0)		/* & rename target to BAK        */
			print_log_line(text_strings[LOG_OK].ob_spec);
		else print_log_line(text_strings[LOG_FAILED].ob_spec);
	}

	if (newname)
		sprintf(msg,text_strings[COPYING_TO].ob_spec,name,newname);
	else sprintf(msg,text_strings[COPYING].ob_spec,name);
	print_log_line(msg);
	if ((rc=copy_file(src,dest,name,newname)) == 0)
		print_log_line(text_strings[LOG_OK].ob_spec);
	else print_log_line(text_strings[LOG_FAILED].ob_spec);

	return rc;
}

/*
 *	check if a supplied name is a directory.
 *	the following are directories by default:
 *		\
 *		x:\
 */
int isdir(char *dir)
{
struct FILEINFO info;

	if (strcmp(dir,"\\") == 0)
		return TRUE;
	if (strlen(dir) == 3)
		if (strcmp(dir+1,":\\") == 0)
			return TRUE;
	if (dfind(&info,dir,0x10) == 0)
		return TRUE;

	return FALSE;
}

/*
 * copy_file:
 *	copy an individual file to a directory, optionally renaming it
 *	note: we retain the source file's date/time stamp
 */
int copy_file(char *src,char *dest,char *name,char *newname)
{
char target[MAXPATH], source[MAXPATH], *buf;
FILE *in, *out;
int fsize, rc = GENERAL_ERROR;
long ftime;

	strmfp(target,dest,newname?newname:name);	/* get full name of dest file */
	strmfp(source,src,name);					/*  and source */

	if (((in=fopen(source,"rb")) == NULL)
	 || ((out=fopen(target,"wb")) == NULL))
		return rc;
	ftime = getft(fileno(in));		/* save file date/time */

	fsize = filelength(fileno(in));
	if (buf=malloc(fsize)) {
		if (fread(buf,fsize,1,in) == 1)
			if (fwrite(buf,fsize,1,out) == 1)
				rc = 0;
		free(buf);
	}

	/*
	 * note that we close & reopen the output file before we attempt to
	 * set the date/time.  this is to bypass a bug in TOS 1.0 with
	 * Fdatime() which I _suspect_ will also show up in chgft().
	 */
	if (fclose(in) || fclose(out))
		rc = GENERAL_ERROR;

	if (rc)
		return rc;

	rc = GENERAL_ERROR;
	if (out=fopen(target,"rb")) {
		rc = chgft(fileno(out),ftime);
		fclose(out);				/* don't care about _this_ rc */
	}

	return rc;
}

int register_file(char *dest,char *pgm,char *rsc)
{
char target_path[MAXPATH];
FILE *fp;
PROGRAM_HEADER header;
long hexreg, hexname;
int c, rc;
long n, seekpos;

	/*
	 *	first, store registration code & encrypted name in program
	 */
	strmfp(target_path,dest,pgm);			/* get full name of file to register */
	hexreg = external_to_hex(regcode->te_ptext);
	hexname = encrypt(username->te_ptext,PREFS_FILE);

	fp = fopen(target_path,"rb+");
	if (!fp)
		return GENERAL_ERROR;

	rc = -1;
	if (fread(&header,sizeof(header),1,fp) == 1)
		if (fseek(fp,header.textlen,SEEK_CUR) == 0)
			rc = 0;
	if (rc < 0) {
		fclose(fp);
		return GENERAL_ERROR;
	}

	for (n = 0, seekpos = -1L; (n < header.datalen) && !feof(fp); n += 4) {
		seekpos = ftell(fp);
		if (fgetl(fp) == PROTECT_MAGIC1)
			if (fgetl(fp) == 0UL)
				if (fgetl(fp) == 0UL)
					if (fgetl(fp) == PROTECT_MAGIC2) {
						seekpos = ftell(fp) - 12;
						break;
					}
	}

	rc = -1;
	if (seekpos >= 0)
		if (fseek(fp,seekpos,SEEK_SET) == 0)
			if (fputl(hexreg,fp) == hexreg)
				if (fputl(hexname,fp) == hexname)
					rc = 0;
	fclose(fp);
	if (rc < 0)
		return GENERAL_ERROR;

	/*
	 *	next, store user name and serial number in resource
	 */
	strmfp(target_path,dest,rsc);			/* get full name of file to register */

	fp = fopen(target_path,"rb+");
	if (!fp)
		return GENERAL_ERROR;

	for (rc = 0; (rc != 0x03) && !feof(fp); ) {
		c = fgetc(fp);
		if (c == 'U') {
			seekpos = ftell(fp);
			if (check_string(fp,seekpos-1,"Unregistered") == 0) {
				if (modify_string(fp,seekpos-1,username->te_ptext) == 0)
					rc |= 0x01;
				else break;
			}
			if (fseek(fp,seekpos,SEEK_SET) < 0)
				break;
		} else if (c == '?') {
			seekpos = ftell(fp);
			if (check_string(fp,seekpos-1,"??????????") == 0) {
				if (modify_string(fp,seekpos-1,sernum->te_ptext) == 0)
					rc |= 0x02;
				else break;
			}
			if (fseek(fp,seekpos,SEEK_SET) < 0)
				break;
		}
	}
	fclose(fp);

	if (rc != 0x03)
		return GENERAL_ERROR;

	return 0;
}

int check_string(FILE *fp,long seekpos,char *string)
{
char temp[100];
int n;

	if (fseek(fp,seekpos,SEEK_SET) < 0)
		return GENERAL_ERROR;

	n = strlen(string);
	if (fread(temp,n,1,fp) != 1)
		return GENERAL_ERROR;

	if (memcmp(string,temp,n) != 0)
		return GENERAL_ERROR;

	return 0;
}

int modify_string(FILE *fp,long seekpos,char *string)
{
int n;

	if (fseek(fp,seekpos,SEEK_SET) < 0)
		return GENERAL_ERROR;

	n = strlen(string) + 1;			/* includes the terminating null */
	if (fwrite(string,n,1,fp) != 1)
		return GENERAL_ERROR;

	return 0;
}

/*
 * adds text (up to the end of string or the first newline) to the current
 * line in the log dialog
 *	. increments line ptr if a newline is found
 *	. scrolls text if log dialog gets full
 */
void print_log_line(char *line)
{
char *p, *q;
int i, startobj;
static int count = -1;

	if (count < 0) {
		startobj = ROOT;
		count = 0;
	} else if (count < LOG_LINES)
		startobj = LOG_ENTRY + count;
	else startobj = LOG_BOX;

	if (count >= LOG_LINES) {		/* scroll when dialog is full */
		for (i = 1; i < LOG_LINES; i++)
			strcpy(gettext(log_dialog,LOG_ENTRY+i-1),gettext(log_dialog,LOG_ENTRY+i));
		*gettext(log_dialog,LOG_ENTRY+LOG_LINES-1) = '\0';
		count = LOG_LINES-1;
	}

	for (q = gettext(log_dialog,LOG_ENTRY+count); *q; q++)
		;

	for (p = line, i = 0; *p && (i < LOG_LINE_LENGTH-1); i++) {
		if (*p == '\n') {
			count++;
			break;
		}
		*q++ = *p++;
	}
	*q = '\0';

	objc_head(log_dialog);
}

/*--------------------------------------------------------------*
 *			miscellaneous support routines						*
 *--------------------------------------------------------------*/
/*
 *	returns contents of system word
 */
unsigned long get_sys(char **address)
{
unsigned long contents;
char *stack_save;

	stack_save = Super(0);			/* into supervisor state */
	contents = (unsigned long) *address;
	Super(stack_save);				/* back to user-land */

    return contents;
}

/*
 *	return pointer to extension portion of file spec
 */
char *get_ext(char *fname)
{
int n;
char *p;

	n = strlen(fname);
	for (p = fname+n-1; p >= fname; p--) {
		if (*p == '.')
			return p+1;			/* extension follows period */
		if (*p == '\\')
			return NULL;		/* stop at directory */
	}

	return NULL;
}

void get_prg_path(char *app_path,char *app_name)
{
char *ptr;

	strcpy(app_path,app_name);
	if (shel_find(app_path) == 0)
		app_path[0] = '\0';
	else if ((ptr=strrchr(app_path,'\\')) != NULL)
		*(ptr+1) = '\0';
	else get_curr_path(app_path);
}

void get_curr_path(char *curr_path)
{
int drive;
char s[257];

	drive = Dgetdrv() + 1;
	if (Dgetpath(s,drive) < 0)
		s[0] = '\0';
	sprintf(curr_path,"%c:%s\\",'A'+drive-1,s);
}

void issue_alert(int index)
{
char **alert;

	rsrc_gaddr(R_FRSTR,index,&alert);
	form_alert(1,*alert);
}

void objc_head(OBJECT *tree)
{
	objc_draw(tree,0,MAX_DEPTH,tree->ob_x-3,tree->ob_y-3*rez,
                    tree->ob_width+6,tree->ob_height+6*rez);
}

void redraw_object(OBJECT *tree,short idex)
{
short xoff, yoff;

	objc_offset(tree,idex,&xoff,&yoff);
	objc_draw(tree,0,MAX_DEPTH,xoff,yoff,tree[idex].ob_width,tree[idex].ob_height);
}  

void quit()
{
	v_clsvwk(myhandle);
	appl_exit();
	exit(0);
}
