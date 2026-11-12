/*
 *	info.c
 *
 *	performs the following major functions:
 *		undelete: save/restore disk and scsi info
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 *
 *	Modified for Obsidian Edge (November 2026): demo version removed
 */

/*
 *	version 2.10 (december/2002)	roger burrows
 *
 *	version 2.54 (august/2003)		roger burrows
 *		. add DEMO support for save_disk_info(), restore_disk_info()
 *		  [also handles Unzero function]
 *
 *	version 2.57 (january/2017)		roger burrows
 *		prepare for public release
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <dos.h>
#include <portab.h>
#include <string.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"
#include "lib/driver.h"

#define MODULE_NUMBER	5		/* used by error() macro */

extern OPT opt;
extern short dev;
extern long drivemap;
extern OBJECT *restconf, *restinfo, *saveinfo, *selscsi, *text;

/* scan-related variable, was used for communication with part1.c */
MLOCAL int ndev = -1;			/* number of devices found (-ve means scan needed) */


/*
 *	function prototypes
 */
PRIVATE void do_scan(void);
PRIVATE long get_fsize(short fid);
PRIVATE long idlun(int startobj);
PRIVATE void read_and_edit_root(short id,short lun);
PRIVATE int restore_di(short fid,long fsize,char *name,short restdev,char *rootbuf);
PRIVATE void save_root_sectors(short id,short lun,char *path);
PRIVATE void setup_rescan(void);


/**************************************************************/
PUBLIC int save_info(short savedev,char *path)
{
long n, ntowrite;
short fboot, nsec;
char *bootbuf, *rootbuf = NULL;
char fbootf[MAX_PATHNAME];
int rc = 0;
MYBPB tpb;
BPB *tbpb;

	n = (savedev >= FIRST_HD_DEVICE) ? MAX_HD_LOGSECSIZE : MAX_FL_LOGSECSIZE;
	if ((bootbuf=malloc(n)) == NULL) {
		error(n,MEMALLOC,"");
		return -1;
	}

	_mediach(savedev);
	if (log_read(savedev,1,0,bootbuf,BOOT)) {
		free(bootbuf);
		xform_alert(1,GEN_OPFAIL_ALERT);
		return -1;
	}

	if ((tbpb=Getbpb(savedev)) == NULL) {
		free(bootbuf);
		xform_alert(1,GEN_BPB_ALERT);
		return -1;
	}
	if (load_mybpb(bootbuf,&tpb,tbpb) < 0) {
		free(bootbuf);
		xform_alert(1,GEN_BPB_ALERT);
		return -1;
	}

	sprintf(fbootf,"%s" MIRROR_FILE,path,'A'+savedev);
	if ((fboot=Fcreate(fbootf,0)) >= 0L) {
		nsec = 1 + 2 * tpb.spf + tpb.rds;
		if ((rootbuf=malloc(nsec*tpb.bps)) != NULL) {
			if (log_read(savedev,nsec,0,rootbuf,BOOT) == 0) {
				memmove(rootbuf+tpb.bps+tpb.bpf,rootbuf+tpb.bps+2*tpb.bpf,tpb.rdb);
													/* new format only saves the first FAT */
				ntowrite = tpb.bps + tpb.bpf + tpb.rdb;
				if (file_write(fboot,ntowrite,rootbuf,fbootf))
					rc = -1;
			} else rc = -1;
		} else {
			error(nsec*tpb.bps,MEMALLOC,"");
			rc = -1;
		}
		Fclose(fboot);
	} else {
		error(fboot,OPEN,fbootf);
		rc = -1;
	}

	free(bootbuf);
	if (rootbuf)
		free(rootbuf);

	return rc;
}

/**************************************************************************/
PUBLIC void save_disk_info(void)
{
short j, ret;
int done = FALSE;
char fbootf[MAX_PATHNAME];
char *savep = NULL;
OBJECT *obj;

	myform_center(saveinfo);
	obj_head(saveinfo);
	do {
		j = 0;
		ret = myform_do(saveinfo,&j,0) & 0x7fff;
		saveinfo[ret].ob_state &= ~SELECTED;
		redraw_object(saveinfo,ret);
		switch(ret) {
		case SICLEAR:
			for (j = 0; j < NUM_DEVICES; j++)
				objc_xdsel(saveinfo,SIA+j);
			break;
		case SISELALL:
			for (j = FIRST_HD_DEVICE; j < NUM_DEVICES; j++)
				if ((1L<<j) & drivemap)
					objc_xsel(saveinfo,SIA+j);
			break;
		case SICAN:
			refresh_screen(saveinfo);
			return;
			break;
		case SAVEDI:
			done = TRUE;
			break;
		}
	} while(!done);

	refresh_screen(saveinfo);
	if (get_path(fbootf,opt.filepath,MIRROR_MASK,text[DINFO_FSEL_SAVE].ob_spec)) {
		savep = fbootf;
		for (j = 0, obj = &saveinfo[SIA]; j < NUM_DEVICES; j++, obj++)
			if (obj->ob_state & SELECTED)
				save_info(j,savep);
	}
}

/*
 *	restore disk info (root + FATs) from file
 *
 *	'unzero' sets the defaults for a restore:
 *		0	default is no disk info
 *		1	default is all disk info
 */
PUBLIC void restore_disk_info(int unzero)
{
short j, fid;
short restdev = -1;
char *rootbuf;
char fname[MAX_PATHNAME];
DTA dta;
OBJECT *obj;
int rc;

	if (unzero) {
		restinfo[MRESBOOT].ob_state |= SELECTED;
		restinfo[MRESROOT].ob_state |= SELECTED;
		restinfo[MRESFAT].ob_state |= SELECTED;
	} else {
		restinfo[MRESBOOT].ob_state &= ~SELECTED;
		restinfo[MRESROOT].ob_state &= ~SELECTED;
		restinfo[MRESFAT].ob_state &= ~SELECTED;
	}

	do {
		if (handle_dial(restinfo,0,0,0) != RESTDI)
			return;
		for (j = 0, obj = &restinfo[RESTA]; j < NUM_DEVICES; j++, obj++) {
			if (obj->ob_state & SELECTED) {
				restdev = j;
				break;				/* get options & verify drive */
			}
		}
		if (restdev < 0)
			xform_alert(1,GEN_PART_ALERT);
	} while(restdev < 0);

	if (get_existing_fname(fname,opt.filepath,MIRROR_MASK,text[DINFO_FSEL_REST].ob_spec) == 0)
		return;

	Fsetdta(&dta);
	Fsfirst(fname,0);								/* get file info */

	if ((fid=Fopen(fname,0)) < 0) {					/* open file */
		error(fid,OPEN,dta.name);
		return;
	}

	if ((rootbuf=malloc(2*dta.fsize)) == NULL) {	/* alloc buffer */
		error(2*dta.fsize,MEMALLOC,"");
		Fclose(fid);
		return;
	}

	rc = restore_di(fid,dta.fsize,dta.name,restdev,rootbuf);

	_mediach(restdev);
	if (rootbuf)
		free(rootbuf);
	Fclose(fid);
	if (dev == restdev)
		force_update(dev);
	if (rc < 0)
		xform_alert(1,GEN_OPFAIL_ALERT);
}

/*
 *	do the actual restore of the saved diskinfo
 *
 *	returns -1 if the caller should issue the general error message
 */
PRIVATE int restore_di(short fid,long fsize,char *name,short restdev,char *rootbuf)
{
MYBPB tpb;
unsigned short *f16;
long offset;
int j, used;

	if (file_read(fid,fsize,rootbuf,name))
		return -1;

	if (load_mybpb(rootbuf,&tpb,NULL) < 0) {	/* verify that it's a recognizable boot sector */
		xform_alert(1,GEN_BPB_ALERT);
		return 0;
	}

	f16 = (unsigned short *)(rootbuf + tpb.bps);
	for (j = 2, used = 0; j < tpb.maxc; j++)
		if (f16[j])
			used++;

	sprintf(restconf[RBPS].ob_spec,"%5d",tpb.bps);
	sprintf(restconf[RSPF].ob_spec,"%3d",tpb.spf);
	sprintf(restconf[RNDIR].ob_spec,"%4d",tpb.ndir);
	sprintf(restconf[RTSEC].ob_spec,"%9ld",tpb.tsec);
	sprintf(restconf[RCDATA].ob_spec,"%5d",tpb.cdata);
	sprintf(restconf[RUSED].ob_spec,"%5d",used);

	if (handle_dial(restconf,0,0,0) != RPROCEED)
		return 0;

	if (opt.warn)
		if (xform_alert(2,DI_WARN_ALERT) == 2)
			return 0;

	if (Getbpb(restdev) == NULL) {
		xform_alert(1,DI_BADBPB_ALERT,'A'+restdev);
		return 0;
	}

	if (restinfo[MRESBOOT].ob_state & SELECTED)
		if (log_write(restdev,1,0,rootbuf,BOOT))
			return -1;

	if (restinfo[MRESFAT].ob_state & SELECTED) {
		offset = tpb.bps;
		must_write(restdev,tpb.spf,1,rootbuf+offset,FAT);
		must_write(restdev,tpb.spf,1+tpb.spf,rootbuf+offset,FAT);
	}

	if (restinfo[MRESROOT].ob_state & SELECTED) {
		offset = tpb.bps + tpb.bpf;
		must_write(restdev,tpb.rds,1+2*tpb.spf,rootbuf+offset,DIR);
	}

	return 0;
}


/*************************************************************************/
PUBLIC void save_scsi_info(void)
{
char fbootf[MAX_PATHNAME];
short j, id, lun, exitobj;
int n, rc;
OBJECT *obj;

	strcpy(selscsi[SELSTOP].ob_spec,text[SCSIINFO_SAVE].ob_spec);
	strcpy(selscsi[SELSGO].ob_spec,text[SCSIINFO_SAVEBUT].ob_spec);
	for (j = 0, obj = &selscsi[SNAME1]; j < MAX_DEVS; j++, obj++) {
		obj->ob_flags &= ~RBUTTON;
		obj->ob_state &= ~SELECTED;
	}
	draw_dial(selscsi,0);

	while(TRUE) {
		if (ndev < 0)			/* need device scan */
			do_scan();

		j = 0;
		exitobj = myform_do(selscsi,&j,0) & 0x7fff;
		selscsi[exitobj].ob_state &= ~SELECTED;
		redraw_object(selscsi,exitobj);
		switch(exitobj) {
		case SRESCAN:
			setup_rescan();
			break;

		case SELSGO:
			for (j = 0, n = 0, obj = &selscsi[SNAME1]; j < ndev; j++, obj++)
				if (obj->ob_state & SELECTED)
					n++;
			if (n == 0) {
				xform_alert(1,GEN_DRIVE_ALERT);
				continue;				/* retry */
			}
			if (get_path(fbootf,opt.filepath,PART_MASK,text[SCSIINFO_FSEL_PT].ob_spec)) {
				for (j = 0, obj = &selscsi[SNAME1]; j < ndev; j++, obj++) {
					if (obj->ob_state & SELECTED) {
						if ((rc=idlun(j)) >= 0L) {
							id = rc >> 16;
							lun = rc & 0xffff;
							save_root_sectors(id,lun,fbootf);
						}
					}
				}
				refresh_screen(selscsi);	/* this causes the system to issue a redraw message */
				return;
			}
			break;

		case SCANCEL:
			refresh_screen(selscsi);		/* as above */
			return;
		}
	}
}

/*
 *	reset SELSCSI dialog and force bus/device rescan
 */
PRIVATE void setup_rescan(void)
{
int n;
OBJECT *obj;

	for (n = 0, obj = &selscsi[SNAME1]; n < ndev; n++, obj++) {
		strcpy(obj->ob_spec,"");
		obj->ob_state &= ~SELECTED;				/* deselect */
		redraw_object(selscsi,SNAME1+n);
	}
	rescan_buses();		/* do bus rescan */
	ndev = -1;			/* & force device rescan */
}

/*
 *	scan the selscsi device display for a selected object, starting at offset 'startobj'
 *
 *	returns -1 if none found, else the id || lun of the first
 *	selected object found
 */
PRIVATE long idlun(int startobj)
{
int i, id, lun;
OBJECT *obj;

	for (i = startobj, obj = &selscsi[SNAME1+startobj]; i <= ndev; i++, obj++) {
		if (obj->ob_state & SELECTED) {
			id = atoi(obj->ob_spec);
			lun = atoi((char *)obj->ob_spec+4);
			return (id<<16) | lun;
		}
	}

	return -1;
}

/*
 *	read in root sector and extended root sectors (if any)
 *	and save to file
 */
PRIVATE void save_root_sectors(short id,short lun,char *path)
{
long numread;
char *iobuf;

	if (!(iobuf=malloc(ROOTSEC_BUFSIZE))) {
		error(ROOTSEC_BUFSIZE,MEMALLOC,"");
		return;
	}

	if ((numread=load_root_sectors(id,lun,iobuf)) > 0L)
		write_partinfo_file(path,id,lun,iobuf,numread);
	else scsi_op_failed(id,lun);

	free(iobuf);
}

/*************************************************************************/
PUBLIC void restore_scsi_info(void)
{
	strcpy(selscsi[SELSTOP].ob_spec,text[SCSIINFO_RESTORE].ob_spec);
	strcpy(selscsi[SELSGO].ob_spec,text[SCSIINFO_RESTBUT].ob_spec);
	handle_scsiinfo_dialog(read_and_edit_root);
}

/*
 *	handle the 'write' scsiinfo dialog, calling an appropriate function
 *
 *	this routine is used by restore_scsi_info() and do_part()
 */
PUBLIC void handle_scsiinfo_dialog(void edit_root(short,short))
{
long rc;
short exitobj, j;
int id, lun;
OBJECT *obj;

	for (j = 0, obj = &selscsi[SNAME1]; j < MAX_DEVS; j++, obj++) {
		obj->ob_flags |= RBUTTON;
		obj->ob_state &= ~SELECTED;
	}
	draw_dial(selscsi,0);

	while(TRUE) {
		if (ndev < 0)			/* need device scan */
			do_scan();

		j = 0;
		exitobj = myform_do(selscsi,&j,0) & 0x7fff;
		selscsi[exitobj].ob_state &= ~SELECTED;
		redraw_object(selscsi,exitobj);
		switch(exitobj) {
		case SRESCAN:
			setup_rescan();
			break;

		case SELSGO:
			if ((rc=idlun(0)) >= 0L) {	/* first (if present) is only selected object */
				refresh_screen(selscsi);	/* clean up the selscsi dialog ... */
				id = rc >> 16;
				lun = rc & 0xffff;
				edit_root(id,lun);
				return;
			}
			xform_alert(1,GEN_DRIVE_ALERT);
			break;

		case SCANCEL:
			refresh_screen(selscsi);	/* clean up the selscsi dialog ... */
			return;
		}
	}
}

/*
 *	scan for all hard disk devices & populate SELSCSI dialog
 *
 *	at this time we don't look for non-zero LUNs ...
 */
PRIVATE void do_scan(void)
{
char identifier[25];
int n, bus, id, lun = 0;
OBJECT *obj;

	for (bus = 0, ndev = 0, obj = &selscsi[SNAME1]; bus < NUM_BUSES; bus++) {
		if (!bus_exists(bus))
			continue;
		for (n = 0; (n < IDS_PER_BUS) && (ndev < MAX_DEVS); n++) {
			id = bus*IDS_PER_BUS + n;
			sprintf(obj->ob_spec,"%3d %2d   %s",id,lun,text[PART_INQUIRING].ob_spec);
			redraw_object(selscsi,SNAME1+ndev);
			if (device_exists(id,lun,identifier) == HD_DEVICE) {
				sprintf(obj->ob_spec,"%3d %2d   %s",id,lun,identifier);
				redraw_object(selscsi,SNAME1+ndev);
				ndev++;
				obj++;
			}
		}
	}

	if (ndev < MAX_DEVS) {
		strcpy(selscsi[SNAME1+ndev].ob_spec,"");
		redraw_object(selscsi,SNAME1+ndev);
		for (n = 0, obj = &selscsi[SNAME1]; n < ndev; n++, obj++)
			obj->ob_flags |= SELECTABLE;
		for ( ; n < MAX_DEVS; n++, obj++)
			obj->ob_flags &= ~SELECTABLE;
	}
}


/*
 *	read in the root sector(s) from a file, and edit them
 */
PRIVATE void read_and_edit_root(short id,short lun)
{
char *iobuf;
long bufsize, fsize;

	bufsize = ROOTSEC_BUFSIZE;
	if ((iobuf=calloc(1,bufsize)) == NULL) {
		error(bufsize,MEMALLOC,"");
		return;
	}
	fsize = read_info_file(iobuf,bufsize);
	if (fsize >= 0L)
		do_scsi_edit(id,lun,iobuf,fsize);

	free(iobuf);
}


/*
 *	read info file into buffer (maxsize specifies size of buffer)
 *
 *	returns size of file, or negative if error
 */
PUBLIC long read_info_file(char *buf,long maxsize)
{
char fullname[MAX_PATHNAME], fname[MAX_FILENAME];
long rc, filesize;
short fid;

	if (get_existing_fname(fullname,opt.filepath,PART_MASK,text[SCSIINFO_FSEL_FL].ob_spec) == 0)
		return -1L;
	stcgfn(fname,fullname);			/* get name *without* path for display */

	rc = Fopen(fullname,0);
	if (rc < 0) {					/* precaution, get_existing_fname() should have ensured it exists */
		error(rc,OPEN,fname);
		return -2L;
	}
	fid = (short)rc;
	filesize = get_fsize(fid);
	if (filesize > maxsize)
		rc = -1L;					/* slightly misleading, but saves a new message ... */
	else if (filesize < 0L)
		rc = filesize;
	else rc = file_read(fid,filesize,buf,fname);
	Fclose(fid);

	if (rc < 0L) {
		error(rc,READ,fname);
		return -2L;
	}

	return filesize;
}

PRIVATE long get_fsize(short fid)
{
long rc, size;

	size = Fseek(0L,fid,SEEK_END);
	rc = Fseek(0L,fid,SEEK_SET);

	return (rc==0)?size:rc;
}
