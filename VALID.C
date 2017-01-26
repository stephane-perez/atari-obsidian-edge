/*
 *	valid.c: perform file validation functions
 *
 *	version 2.10 (july/2002)		roger burrows
 *		a highly cleaned up and reorganised version of
 *		the DE v2.04 source; no new function
 *
 *	version 2.11 (august/2002)		roger burrows
 *		revised validation file structure
 *		. new starting magic (necessary precaution :-))
 *		. store method in file, allowing mixed crc/checksum files
 *
 *	version 2.12 (november/2002)	roger burrows
 *		fix subtle bug in validation code: if the name/date/time/size
 *		match, but the crc/checksum doesn't, we must keep on checking
 *		the validation file, in case we have two almost-identical files
 *
 *	version 2.54 (august/2003)		roger burrows
 *		. add DEMO support for do_valid()
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <dos.h>
#include <portab.h>
#include <string.h>
#include <unistd.h>
#include <vdi.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"
#include "xrsrc.h"

#define MODULE_NUMBER	15		/* used by error() macro */

#define MINBUFSIZE		4096L	/* for validation i/o buffer, somewhat arbitrary */

extern short numflops, myhandle, dev, win1;
extern STATUS status;
extern short workx, worky, workh, workw, ychar;
extern OBJECT *bottom, *text;
extern OPT opt;

/*
 *	private variables
 */
MLOCAL FILE *vlderr;
MLOCAL VAL *firstval, *curval;
MLOCAL short curdev, diskno, nerr, newfile;
MLOCAL GRECT rect1, rect2;
MLOCAL char *space;

MLOCAL char shortspace[] = "            ";
MLOCAL char longspace[]  = "                                ";

MLOCAL unsigned short crctab[] = {
	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};


/*
 *	function prototypes
 */
PRIVATE int alloc_val(void);
PRIVATE unsigned short calc_valid(char *path,DTA *dtap,int method);
PRIVATE int check_exists(char *pfname);
PRIVATE int check_valid(char *p,DTA *dtap,char *str,FILE *vf,int valcode);
PRIVATE int floop_hd(char *path,FILE *fout,int valcode);
PRIVATE void free_all_val(void);
PRIVATE int get_newdisk(void);
PRIVATE void load_dta_to_string(DTA *dtap,char *temp);
PRIVATE FILE *load_val(void);
PRIVATE int make_valid(char *p,DTA *dtap,char *str,FILE *vf,int valcode);
PRIVATE void output_string(char *str);
PRIVATE void update_crc(unsigned short val);
PRIVATE void update_vcrc(unsigned short val,unsigned short chk,int addfile,char *path,char *name);
PRIVATE void write_valid(DTA *dtap,unsigned short crcval,FILE *vf,int valcode);


/*
 *	perform file validation functions
 */
PUBLIC void do_valid(short type,short valcode)
{
int i, rc, saveaction, maxlines;
char fpath[MAX_PATHNAME];
static char title[MAX_DISPLAYSIZE];
FILE *fto;

#ifdef DEMO
	if (dev > FIRST_HD_DEVICE) {
		form_alert(1,DEMO_DRIVEC);
		return;
	}
#endif

	vsf_style(myhandle,8);
	vsf_color(myhandle,GREEN);

	maxlines = (workh - 4 * ychar) / ychar;
	if (bottom[0].ob_flags & FL3DMASK)
		maxlines--;

	rect2.g_x = workx;
	rect2.g_y = worky + ychar;
	rect2.g_w = workw;
	rect2.g_h = ychar * (maxlines - 1);

	rect1.g_x = workx;
	rect1.g_y = worky;
	rect1.g_w = workw;
	rect1.g_h = ychar * (maxlines - 1);

	saveaction = status.action;
	switch(type) {
	case MAKEVAL:
		change_mode(MAKEVAL);
		space = shortspace;
		if (dev < 2) {
			i = get_writeable_fname(fpath,opt.filepath,VALID_MASK,text[VALIDATE_FSEL_CR].ob_spec);
		} else {
			sprintf(fpath,"%s" VALID_FILE,opt.filepath,'A'+dev);
			i = check_exists(fpath);
		}
		if (i == 0)			/* file exists, don't mess with it! */
			break;

		if (opt.novice)
			if (xform_alert(1,VL_NOVICE_ALERT) == 2)
				break;
		if (i == 1) {		/* overwrite */
			if ((fto=fopen(fpath,"wb+")) == NULL) {
				error(-1L,OPEN,fpath);
				break;
			}
			fputl(VALIDATION_MAGIC,fto);
			sprintf(title,text[VALIDATE_TITLE1].ob_spec,dev+'A');
		} else {			/* append */
			if ((fto=fopen(fpath,"ab+")) == NULL) {
				error(-1L,OPEN,fpath);
				break;
			}
			if (fseek(fto,0L,SEEK_END) < 0) {
				fclose(fto);
				error(-1L,READ,fpath);
				break;
			}
			sprintf(title,text[VALIDATE_TITLE1A].ob_spec,dev+'A');
		}
		wind_set(win1,WF_NAME,ADDR(title));
		wind_set(win1,WF_INFO,ADDR(text[VALIDATE_INFO1].ob_spec));
		blank(0);
		wind_update(BEG_UPDATE);
		mouse_off();
		rc = 0;
		if (dev < 2) {
			diskno = 1;
			curdev = dev;
			while(get_newdisk()) {
				sprintf(fpath,"%c:",'A'+curdev);
				if (rc=floop_hd(fpath,fto,valcode))			//was floop_flop()
					break;
			}
		} else {
			sprintf(fpath,"%c:",'A'+dev);
			rc = floop_hd(fpath,fto,valcode);
		}
		fclose(fto);
		if (rc < 0)
			output_string(text[VALIDATE_INTRPT].ob_spec);
		else output_string(text[VALIDATE_COMPLET].ob_spec);
		delay(VALIDATION_END_DELAY);
		mouse_on();
		wind_update(END_UPDATE);
		break;

	case CHECKVAL:
		change_mode(CHECKVAL);
		space = longspace;
		if ((fto=load_val()) == NULL)				/* error loading file */
			break;
		sprintf(title,text[VALIDATE_TITLE2].ob_spec,dev+'A');
		wind_set(win1,WF_NAME,ADDR(title));
		wind_set(win1,WF_INFO,ADDR(text[VALIDATE_INFO2].ob_spec));
		blank(0);
		wind_update(BEG_UPDATE);
		mouse_off();
		rc = 0;
		if (dev < 2) {
			diskno = 1;
			while (get_newdisk()) {
				sprintf(fpath,"%c:",'A'+curdev);
				if (rc=floop_hd(fpath,fto,valcode))			//was floop_flop()
					break;
			}
		} else {
			sprintf(fpath,"%c:",'A'+dev);
			rc = floop_hd(fpath,fto,valcode);
		}
		fclose(fto);
		mouse_on();
		wind_update(END_UPDATE);
		free_all_val();
		if (vlderr)
			fclose(vlderr);
		if (nerr > 0)
			xform_alert(1,VL_ERROR_ALERT,'A'+dev);
		else if (rc < 0)
			xform_alert(1,VL_INTRPT_ALERT);
		else xform_alert(1,VL_OK_ALERT);
		break;
	}

	change_mode(saveaction);
	force_update(-1);
}

/*
 *	perform CRC calculation on a string of bytes
 */
PUBLIC unsigned short addcrc(unsigned short crc,unsigned char *p,long count)
{
long i;

	for (i = 0L; i < count; i++)
		crc = ((crc>>8)&0x00ff) ^ crctab[(crc^*p++)&0x00ff];

	return crc;
}

/*
 *	accumulate checksum
 */
PUBLIC unsigned short addchk(unsigned short chksum,unsigned short *p,long count)
{
long i;

	for (i = 0L; i < count; i++)
		chksum += *p++;

	return chksum;
}

/****************************************************************/
PRIVATE int get_newdisk(void)
{
DTA dta;
char wildcard[10], *action = "";
int n;

	switch(status.action) {
		/*
		 * case MKFL: strcpy(line,util_msg[26]); break;
		 */
	case MAKEVAL:
		action = text[VALIDATE_CREATE].ob_spec;
		break;
	case CHECKVAL:
		action = text[VALIDATE_CHECK].ob_spec;
		break;
	}

	switch(numflops) {
	case 1:
		ring_bell();
		if (xform_alert(1,VL_INSERT1_ALERT,action,diskno) == 2)		/* wants to quit */
			return 0;
		curdev = 0;
		break;
	case 2:
		ring_bell();
		if ((diskno == 1) || (curdev == 1))
			n = 1;
		else n = 2;
		if ((n=xform_alert(n,VL_INSERT2_ALERT,action,diskno)) == 3)	/* wants to quit */
			return 0;
		curdev = n - 1;
		break;
	}

	_mediach(curdev);
	sprintf(wildcard,"%c:\\*.*",'A'+curdev);
	Fsetdta(&dta);
	if (Fsfirst(wildcard,FA_VOLUME) >= 0) {
		n = atoi(dta.name);
		if (diskno == n)
			diskno++;
		else diskno = n + 1;
	} else diskno = 1;

	return 1;
}


/****************************************************************************/

PRIVATE int floop_hd(char *path,FILE *fout,int valcode)
{
int ret;
DTA dta;
char pbuff[MAX_PATHNAME], pathsearch[MAX_PATHNAME], temp[MAX_DISPLAYSIZE];

	if (get_key() < 0)
		return -1;

	sprintf(pathsearch,"%s\\*.*",path);
	Fsetdta(&dta);

	/*
	 *	first we do any subdirectories
	 */
	ret = Fsfirst(pathsearch,FA_SUBDIR);
	while (ret >= 0) {
		if (dta.fattr & FA_SUBDIR) {
			if (strcmp(dta.name,".") && strcmp(dta.name,"..")) {
				sprintf(pbuff,"%s\\%s",path,dta.name);
				if (floop_hd(pbuff,fout,valcode) < 0)
					return -1;
				Fsetdta(&dta);
			}
		}
		ret = Fsnext();
	}

	/*
	 *	then we do any files
	 */
	ret = Fsfirst(pathsearch,FA_SYSTEM|FA_HIDDEN);
	while (ret >= 0) {
		load_dta_to_string(&dta,temp);
		switch(status.action) {
		case MAKEVAL:
			make_valid(path,&dta,temp,fout,valcode);
			break;
		case CHECKVAL:
			check_valid(path,&dta,temp,fout,valcode);
			break;
		}
		if (get_key() < 0)
			return -1;
		Fsetdta(&dta);
		ret = Fsnext();
	}

	return 0;
}

/*********************************************/

PRIVATE void load_dta_to_string(DTA *dtap,char *temp)
{
char nm[NODE_LENGTH+1];
char ext[EXT_LENGTH+1];
char *p, *d, dt_str[20];

	strcpy(ext,"   ");
	d = nm;
	p = dtap->name;
	while (*p != '\0') {
		if (*p == '.') {
			*d = '\0';
			d = ext;
			p++;
		} else
			*d++ = *p++;
	}
	*d = '\0';
	decode_date_time(dt_str,dtap->fdate,dtap->ftime,opt.datemode);

	sprintf(temp," %8s.%3s %10ld  %s   %s",
			nm,ext,dtap->fsize,dt_str,space);
}

/*
 *	create validation file
 */
PRIVATE int make_valid(char *p,DTA *dtap,char *str,FILE *vf,int valcode)
{
unsigned short crcval;

	output_string(str);
	crcval = calc_valid(p,dtap,valcode);
	update_crc(crcval);
	write_valid(dtap,crcval,vf,valcode);

	return 0;
}

/*
 *	check if file exists; if it does, prompts for overwrite/append/cancel
 *
 *	returns:
 *		0	it exists, and user doesn't want it altered
 *		1	it exists and can be overwritten, or it doesn't exist
 *		2	it exists, and user wants to append	
 */
PRIVATE int check_exists(char *pfname)
{
short k;
char temp2[MAX_FILENAME];

	if (access(pfname,0) == 0) {				/* it exists */
		stcgfn(temp2,pfname);					/* extract filename */
		if ((k=xform_alert(2,FI_EXISTS_ALERT,temp2)) == 3)	/* Overwrite/Append/Cancel: default is Append */
			k = 0;
	} else k = 1;

	return k;
}

/*
 *	check validation file
 */
PRIVATE int check_valid(char *p,DTA *dtap,char *str,FILE *vf,int valcode)
{
VAL *v, *vsave;
unsigned short crcval;

	output_string(str);

	for (v = firstval, vsave = NULL; v; v = v->next_val) {
		if ((dtap->fdate == v->vdate)
		 &&	(dtap->ftime == v->vtime)
		 && (dtap->fsize == v->vsize)
		 && (strcmp(dtap->name,v->vname) == 0)) {
			crcval = calc_valid(p,dtap,v->method);	/* must calc every time, because v->method may vary */
			if (crcval == v->crc) {
				update_vcrc(crcval,v->crc,0,p,dtap->name);
				return 0;
			}
			vsave = v;			/* crc mismatch, but keep on checking in case there are multiple  */
								/* files with the same date/time/size/name but different contents */
		}
	}

	if (vsave) {				/* we'll use the last calculated crcval ... */
		update_vcrc(crcval,vsave->crc,0,p,dtap->name);
		return 1;
	}

	/*
	 *	must be a new file
	 */
	switch(newfile) {
	case ASKFILE:
		if (xform_alert(1,VL_ASK_ALERT) == 2)
			return 0;
						/* else drop through to 'ADDFILE' action */
	case ADDFILE:
		crcval = calc_valid(p,dtap,valcode);
		write_valid(dtap,crcval,vf,valcode);
		update_vcrc(crcval,crcval,1,p,dtap->name);
		break;
	}

	return 0;
}

/*
 *	write a validation file entry
 */
PRIVATE void write_valid(DTA *dtap,unsigned short crcval,FILE *vf,int valcode)
{
	fputw(dtap->fdate,vf);
	fputw(dtap->ftime,vf);
	fputl(dtap->fsize,vf);
	fputw(crcval,vf);
	fputc(valcode,vf);
	fprintf(vf,"%s\n",dtap->name);
}

/*
 *	do crc/checksum calculation for one file
 */
PRIVATE unsigned short calc_valid(char *path,DTA *dtap,int valcode)
{
char *buf;
unsigned short crcval;
short fh, offset;
long div, bytes_out, count, rc;
long bufsize;
char pathname[MAX_PATHNAME];

	sprintf(pathname,"%s\\%s",path,dtap->name);
	if ((rc=Fopen(pathname,0)) < 0) {
		error(-1L,OPEN,pathname);
		return 0;
	}
	fh = (short)rc;

	for (div = 1; ; div++) {
		bufsize = (dtap->fsize/div + 1) & ~1L;	/* ensure even number for checksum calc */
		if (buf=malloc(bufsize))
			break;
		if (bufsize < MINBUFSIZE) {			//rfb: we should issue an error msg
			Fclose(fh);						//better still, only assign/free buffer in calling routine
			return 0;
		}
	}

	if ((valcode&0x0f) && (dtap->fsize > VALIDATION_START))
		offset = VALIDATION_START;
	else offset = 0;
	for (bytes_out = 0L, crcval = 0; bytes_out < dtap->fsize; bytes_out += count, offset = 0) {
		count = min(bufsize,dtap->fsize-bytes_out);
		if (file_read(fh,count,buf,pathname)) {
			Fclose(fh);
			free(buf);
			return 0;
		}
		if ((valcode>>4)&0x0f == CRC)
			crcval = addcrc(crcval,buf+offset,count-offset);
		else crcval = addchk(crcval,(unsigned short *)(buf+offset),(count-offset)/2L);
	}

	free(buf);
	Fclose(fh);

	return crcval;
}

/*
 *	load validation file into memory
 */
PRIVATE FILE *load_val()
{
FILE *fto;
char fpath[MAX_PATHNAME];
int i = 1;

	firstval = curval = NULL;
	nerr = 0;
	vlderr = NULL;

	sprintf(fpath,"%s" VALID_FILE,opt.filepath,'A'+dev);

	if (access(fpath,0)) {
		i = 0;
		i = get_existing_fname(fpath,opt.filepath,VALID_MASK,text[VALIDATE_FSEL_LD].ob_spec);
	}
	if (!i)
		return NULL;

	if ((fto=fopen(fpath,"ab+")) == NULL) {
		error(-1L,OPEN,fpath);
		return NULL;
	}

	i = fgetl(fto);
	if (i != VALIDATION_MAGIC) {
		xform_alert(1,VL_BADVAL_ALERT);
		fclose(fto);
		return NULL;
	}

	newfile = xform_alert(0,VL_ACTION_ALERT);
	while(!feof(fto)) {
		if (alloc_val() != 0) {
			free_all_val();
			fclose(fto);
			return NULL;
		}
		curval->vdate = fgetw(fto);
		curval->vtime = fgetw(fto);
		curval->vsize = fgetl(fto);
		curval->crc = fgetw(fto);
		curval->method = fgetc(fto);
		fgets(curval->vname,FNSIZE+1,fto);
		curval->vname[strlen(curval->vname)-1] = '\0';	/* remove newline */
	}

	fseek(fto,0L,SEEK_END);		//rfb: check rc? ... but why do this anyway, we must be at the end?

	return fto;
}

/*************************************************************************/
PRIVATE void output_string(char *str)
{
	blit(&rect1,&rect2);
	v_gtext(myhandle,workx,worky+ychar-1,str);
}

/********************************************************************/
PRIVATE void update_crc(unsigned short val)
{
char temp[MAX_DISPLAYSIZE];

	sprintf(temp,"   0x%04X ",val);
	v_gtext(myhandle,workx+360,worky+ychar-1,temp);			//MAGNUM
}

/********************************************************************/
PRIVATE void update_vcrc(unsigned short val,unsigned short chk,int addfile,char *path,char *name)
{
char temp[MAX_PATHNAME];

	if (val == chk) {
		sprintf(temp,"   0x%04X     %-4.4s",
				val,text[addfile?VALIDATE_ADD:MISC_OK].ob_spec);
		v_gtext(myhandle,workx+360,worky+ychar-1,temp);		//MAGNUM
		return;
	}

	if (nerr++ == 0) {
		sprintf(temp,"%s" VALIDERR_FILE,opt.filepath,'A'+dev);
		vlderr = fopen(temp,"w+");
	}

	sprintf(temp,"   0x%04X %-4.4s= 0x%04X  %-6.6s",
			val,text[VALIDATE_REF].ob_spec,chk,text[MISC_ERROR].ob_spec);
	v_gtext(myhandle,workx+360,worky+ychar,temp);			//MAGNUM
	if (vlderr)
		fprintf(vlderr,"%s\\%s\n%s\n",path,name,temp);
	else xform_alert(1,VL_BADFILE_ALERT);
}

/*
 *	allocate a VAL structure
 */
PRIVATE int alloc_val()
{
VAL *temp;

	if ((temp=calloc(1,sizeof(VAL))) == NULL) {
		error(-1L,MEMALLOC,"");
		return 1;
	}

	if (firstval)
		curval->next_val = temp;
	else firstval = temp;

	curval = temp;
	return 0;
}

/*
 *	free all allocated VAL structures
 */
PRIVATE void free_all_val()
{
VAL *n, *m;

	for (n = firstval; n; n = m) {
		m = n->next_val;
		free(n);
	}
	firstval = curval = NULL;
}
