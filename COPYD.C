/*
 *	copyd.c: implements the copy-partition-with-defrag function
 *
 *	this function is similar to a file-by-file copy of a partition,
 *	but preserves the dates and times of folders as well as files
 *
 *
 *	version 2.10 (november/2002)		roger burrows
 *		a highly cleaned up and reorganised version of
 *		the DE v2.04 source; no new function
 *
 *	version 2.13 (february/2003)		roger burrows
 *		more cleanup, removal of pointless 'look for more free space
 *		when we run out' code
 *
 *	version 2.55 (february/2007)		roger burrows
 *		fix bug: in the output partition, the directory entry for
 *		empty files inadvertently contained the next cluster number
 *		to be assigned, rather thn zero.  this created a file collision
 *		in the output partition.
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <dos.h>
#include <osbind.h>
#include <portab.h>
#include <string.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"

#define MODULE_NUMBER	1		/* used by various error() macros */

#define OUT_OF_SPACE	-2		/* set by rwabs_write()/mydcreate()/copyfile() */

extern STATUS status;
extern MYBPB dpb;
extern OPT opt;
extern short dev;
extern short dirlevel;
extern DIRLIST dirlist[MAX_DIR_DEPTH];
extern OBJECT *pcsel, *text;


/*
 *	private variables
 */
MLOCAL unsigned short *fat16;	/* our own, no need to share it :-) */
MLOCAL long clusters_written;
MLOCAL RUN currun, *firstrun, *run;
MLOCAL short nextcluster;

/*
 *	function prototypes
 */
PRIVATE char *alloc_copybuf(long *bufsize,long filesize);
PRIVATE int backdir(char *path);
PRIVATE int calc_nextcluster(void);
PRIVATE void cleanup_dirs(char *curpath,char *nextpath,short override);
PRIVATE int cleanup_disk(void);
PRIVATE int copydir(char *path);
PRIVATE int copyfile(char *from,DTA *dtap,DIRLIST *curdir,char *buf,long bufsize);
PRIVATE void dir_entry(DIRLIST *dest,DTA *dtap,short cluster,short flag);
PRIVATE long estimate(char *path);
PRIVATE int form_run(void);
PRIVATE void free_all_runs(void);
PRIVATE void get_dirname(char *source,char *dirname,short level);
PRIVATE int mydcreate(DTA *dtap,DIRLIST *parent,DIRLIST *child);
PRIVATE void myfdelete(DIRLIST *dir);
PRIVATE int rwabs_write(char *buf,long count);
PRIVATE int setup_hd(void);
PRIVATE void update_hdfat(long count,RUN *r1);
PRIVATE void write_dir(DIRLIST *dir);


/****************************************************************/
PUBLIC void do_copydefrag(void)
{
int dev1, dev2, savedev;
int rc;
long totclus;
char temp[MAX_ALERT_SIZE];
char path[3];

	wind_update(BEG_UPDATE);
	savedev = dev;

	if ((rc=get_src_dest()) < 0) {
		wind_update(END_UPDATE);
		return;
	}
	dev1 = rc >> 8;
	dev2 = rc & 0xff;

	mouse_off();

	sprintf(temp,text[PROG_SRCDEST].ob_spec,dev1+'A',dev2+'A');
	start_bar(temp);
	dev = dev2;

	if (get_mybpb(&dpb,dev) < 0)
		xform_alert(1,GEN_BPB_ALERT);
	else if (zero_fats_and_root() < 0)
		xform_alert(1,GEN_OPFAIL_ALERT);
	else if (setup_hd() >= 0) {
		update_barmsg(text[PROG_SCANFOLDER].ob_spec);
		sprintf(path,"%c:",dev1+'A');
		totclus = estimate(path);
		update_barmsg(temp);
		init_bar(totclus);
		dirlevel = 0;
		clusters_written = 0;
		backdir(path);			//rfb: should we test rc?
		cleanup_dirs("","",1);
		cleanup_disk();			//rfb: should we test rc?
		free_all_runs();
		wrap_bar();
		ring_bell();
		ring_bell();
	}

	if (fat16) {
		free(fat16);
		fat16 = NULL;
	}
	dev = savedev;
	get_mybpb(&dpb,dev);
	mouse_on();
	wind_update(END_UPDATE);

	force_update(dev2);			//rfb: do we need a _mediach() for dev2 here ??
}

/*
 *	copy all directories below 'path'
 */
PRIVATE int backdir(char *path)
{
int fail = 0;

	fail = copydir(path);

	if (dirlevel != 0)
		write_dir(&dirlist[dirlevel--]);

	return fail;
}

PRIVATE int copydir(char *path)
{
short ret;
int rc;
long bufsize;
DTA dta;
DIRLIST *curdir, *ddir;
char *iobuf;
char pbuff[MAX_PATHNAME], pathsearch[MAX_PATHNAME], fromfile[MAX_PATHNAME];

	ddir = &dirlist[dirlevel];
	while(ddir) {
		curdir = ddir;
		ddir = ddir->xtradir;
	}
	sprintf(pathsearch,"%s\\*.*",path);
	Fsetdta(&dta);

	/*
	 *	first, copy any subdirectories
	 */
	ret = Fsfirst(pathsearch,FA_SUBDIR);
	while (ret >= 0) {
		if (dta.fattr & FA_SUBDIR) {
			if (strcmp(dta.name,".") && strcmp(dta.name,"..")) {
				sprintf(pbuff,"%s\\%s",path,dta.name);
				if ((rc=mydcreate(&dta,&dirlist[dirlevel],&dirlist[dirlevel+1])) < 0) {
					if (rc == OUT_OF_SPACE)
						error(-1L,DISKFULL,"");
					return -1;
				}
				if (backdir(pbuff) < 0)
					return -1;
				ddir = &dirlist[dirlevel];
				while (ddir) {
					curdir = ddir;
					ddir = ddir->xtradir;
				}
				Fsetdta(&dta);		/* because recursively called copies of copydir() have changed it */
			}
		}
		ret = Fsnext();
	}

	/*
	 *	then copy files
	 */
	ret = Fsfirst(pathsearch,FA_SYSTEM|FA_HIDDEN);	/* was FA_HIDDEN */
	while (ret >= 0) {
		sprintf(fromfile,"%s\\%s",path,dta.name);

		if ((curdir->offset == dpb.bpc) && (dirlevel != 0)) {
			if ((curdir->xtradir=calloc(1,sizeof(DIRLIST))) == NULL) {
				error(sizeof(DIRLIST),MEMALLOC,"");
				return -1;
			}
			fat16[curdir->firstcluster] = nextcluster;
			curdir = curdir->xtradir;
			if ((curdir->dirbuf=calloc(1,dpb.bpc)) == NULL) {
				error(dpb.bpc,MEMALLOC,"");
				return -1;
			}
			curdir->offset = 0;
			curdir->firstcluster = nextcluster;
			curdir->xtradir = NULL;
			fat16[nextcluster] = dpb.bitmask;
			calc_nextcluster();		/* don't need to test rc here */
		}
		dir_entry(curdir,&dta,nextcluster,0);
		rc = -1;
		if (iobuf=alloc_copybuf(&bufsize,dta.fsize)) {
			rc = copyfile(fromfile,&dta,curdir,iobuf,bufsize);
			free(iobuf);
		}
		if (rc < 0) {
			if (rc == OUT_OF_SPACE)
				error(-1L,DISKFULL,"");
			return -1;
		}
		ret = Fsnext();
	}

	return 0;
}

/*
 *	copy one file
 */
PRIVATE int copyfile(char *from,DTA *dtap,DIRLIST *curdir,char *buf,long bufsize)
{
short ffrom;
int hdret = 0;
long count, bytes_written, rc;
long fsize = dtap->fsize;
RUN *r1;

	if ((rc=Fopen(from,0)) < 0L) {
		error(rc,OPEN,from);
		myfdelete(curdir);
		return 0;
	}
	ffrom = (short)rc;

	if (!run) {
		Fclose(ffrom);
		myfdelete(curdir);
		return OUT_OF_SPACE;
	}

	memcpy(&currun,run,sizeof(RUN));		/* set up for rwabs_write() */
	r1 = run;								/* remember for update_hdfat() */

	for (bytes_written = 0; bytes_written < fsize; bytes_written += count) {
		/*
		 *	check if previous segment write used up last run ...
		 */
		if (!run) {
			Fclose(ffrom);
			myfdelete(curdir);
			return OUT_OF_SPACE;
		}
		count = min(bufsize,fsize-bytes_written);
		if (file_read(ffrom,count,buf,from)) {
			Fclose(ffrom);
			myfdelete(curdir);
			return 0;
		}
		if ((hdret=rwabs_write(buf,count)) < 0) {
			Fclose(ffrom);
			myfdelete(curdir);
			return hdret;
		}
	}

	Fclose(ffrom);
	update_hdfat(fsize,r1);

	return 0;
}

/*
 *	allocate i/o buffer for copyfile
 */
PRIVATE char *alloc_copybuf(long *bufsize,long filesize)
{
char *iobuf;

	for (*bufsize = max(filesize,dpb.bpc); *bufsize >= dpb.bpc; *bufsize /= 2) {
		*bufsize &= ~(dpb.bpc-1L);		/* ensure multiple of bpc for rwabs_write() */
		if (iobuf=malloc(*bufsize))
			return iobuf;
	}

	error(dpb.bpc,MEMALLOC,"");
	return NULL;
}

/*
 *	write 'count' bytes, rounded up to a whole number of logical clusters
 *
 *	returns:	0	OK
 *				-1	write error (msg has been issued)
 *				-2	out of runs (disk full)
 */
PRIVATE int rwabs_write(char *buf,long count)
{
short clusleft;
int cluswrite;
long offset = 0L;

	clusleft = (count+dpb.bpc-1) >> dpb.cluster_shift;	/* allow for partial end cluster */

	while(TRUE) {
		cluswrite = min(clusleft,currun.num);		/* clusters to write */

		if (log_write(dev,cluswrite*dpb.spc,logsector(nextcluster),buf+offset,FILEERR))
			return -1;

		clusters_written += cluswrite;
		update_bar(clusters_written);

		clusleft -= cluswrite;
		if (clusleft == 0)
			break;

		/*
		 *	here when currun doesn't have enough clusters to satisfy this write ...
		 */
		run = run->nextrun;
		if (run) {
			nextcluster = run->start;
			memcpy(&currun,run,sizeof(RUN));
		} else return OUT_OF_SPACE;
		offset += (long) cluswrite * dpb.bpc;
	}

	currun.num -= cluswrite;
	currun.start += cluswrite;
	if (currun.num) {
		nextcluster = currun.start;
	} else {
		run = run->nextrun;
		if (run) {
			nextcluster = run->start;
			memcpy(&currun,run,sizeof(RUN));
		}						/* else next write will be out of space ... */
	}

	return 0;
}

/*
 *	update in-memory copy of FAT
 */
PRIVATE void update_hdfat(long count,RUN *r1)
{
int numclus, j;

	numclus = (count+dpb.bpc-1) >> dpb.cluster_shift;
	if (numclus == 0)					/* avoid some work, including setting  */
		return;							/* irrelevant fat16[] entry to bitmask */

	nextcluster = r1->start;
	for (j = 0; j < numclus-1; j++) {	/* everything except last cluster */
		if (r1->num == 1) {				/* last cluster of this run, so we need to get next run to know the next cluster */
			r1 = r1->nextrun;
			if (!r1)					/* the disk is full ... should not happen, since these */
				break;					/* clusters have already been written ...              */
			fat16[nextcluster] = r1->start;
			nextcluster = r1->start;
		} else {
			fat16[nextcluster] = nextcluster + 1;
			nextcluster++;
			r1->num--;
			r1->start++;
		}
	}
	fat16[nextcluster] = dpb.bitmask;	/* last cluster */
	calc_nextcluster();
}

/*
 *	prepare destination hard disk partition for being written to
 *	on exit, fat16 may be allocated
 */
PRIVATE int setup_hd(void)
{
char *rootbuf;

	_mediach(dev);

	if (!dpb.isfat16) {
		xform_alert(1,CP_COMPAT_ALERT);
		return -1;
	}

	if ((fat16=malloc(dpb.bpf)) == NULL) {
		error(dpb.bpf,MEMALLOC,"");
		return -1;
	}
	if (log_read(dev,dpb.spf,1,fat16,FAT))
		return -1;

	if ((rootbuf=malloc(dpb.rdb)) == NULL) {
		error(dpb.rdb,MEMALLOC,"");
		return -1;
	}
	if (log_read(dev,dpb.rds,1+2*dpb.spf,rootbuf,DIR)) {
		free(rootbuf);
		return -1;
	}
	memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));
	dirlist[0].dirbuf = rootbuf;
	dirlist[0].offset = 0;
	dirlevel = 0;
	firstrun = NULL;
	swab(fat16,fat16,dpb.bpf);
	if (form_run() < 0) {	/* only if insufficient memory, VERY unlikely */
		free_all_runs();
		free(rootbuf);
		return -1;
	}

	return 0;
}

/*
 *	create zero or more runs, each run describing a block of consecutive free
 *	clusters in fat16[]
 *
 *	NOTE: this routine is called once only, at the beginning of the copy, after
 *	calling zero_fats_and_root().  at this time, the only possible types of
 *	cluster entry are free and bad; if we have N bad cluster entries we will
 *	in general have N+1 runs.  in most cases, we will only have one run.
 *
 *	modifies:	run, firstrun, nextcluster
 *
 *	returns:	0	all runs created OK
 *				-1	insufficient memory (some runs may have been created)
 */
PRIVATE int form_run(void)
{
short j;
RUN *temp;

	for (j = 2; j < dpb.maxc; j++) {
		if (fat16[j])
			continue;
		if ((temp=calloc(1,sizeof(RUN))) == NULL) {
			error(sizeof(RUN),MEMALLOC,"");
			return -1;
		}
		if (firstrun)
			run->nextrun = temp;
		else firstrun = temp;
		run = temp;
		run->start = j;
		run->num = 1;
		while ((!(fat16[++j])) && (j < dpb.maxc))
			run->num++;
	}
	run = firstrun;
	nextcluster = run->start;

	return 0;
}

/****************************************************************************/
/*
 *	free memory used by all existing runs
 */
PRIVATE void free_all_runs(void)
{
RUN *temp;

	for (run = firstrun; run; run = temp) {
		temp = run->nextrun;
		free(run);
	}
	firstrun = NULL;
}

/*
 *	determine 'nextcluster'
 */
PRIVATE int calc_nextcluster(void)
{
	nextcluster = 0;	/* preload for negative rc */

	if (!run)
		return -1;

	run->num--;
	run->start++;

	if (!run->num) {
		run = run->nextrun;
		if (!run)
			return -1;
	}

	nextcluster = run->start;

	return 0;
}

/*
 *	create directory entry in dest->dirbuf from DTA, cluster number, flag
 */
PRIVATE void dir_entry(DIRLIST *dest,DTA *dtap,short cluster,short flag)
{
register char *p, *d;
DIRENTRY dir;

	memset(&dir,' ',NODE_LENGTH+EXT_LENGTH);
	memset(dir.reserved,0x00,sizeof(dir.reserved));
	switch(flag) {
	case 0:
		d = dir.name;
		p = dtap->name;
		while (*p != '\0') {
			if (*p == '.') {
				d = dir.ext;
				p++;
			} else *d++ = *p++;
		}
		break;
	case 1:
		dir.name[0] = '.';
		break;
	case 2:
		dir.name[0] = '.';
		dir.name[1] = '.';
		break;
	}
	dir.attr = (dtap->fattr) & ~FA_ARCHIVE;
	putword(&dir.time,dtap->ftime);
	putword(&dir.date,dtap->fdate);
	putlong(&dir.size,dtap->fsize);
	if (!(dtap->fattr&FA_SUBDIR) && (dtap->fsize == 0L))	/* zero-length files have no cluster */
		cluster = 0;
	putword(&dir.start,cluster);
	memcpy(dest->dirbuf+dest->offset,&dir,sizeof(DIRENTRY));
	dest->offset += sizeof(DIRENTRY);
}

/*
 *	create an entry for a subdirectory (extending the directory if necessary)
 *	plus the subdirectory itself, and the . and .. entries
 */
PRIVATE int mydcreate(DTA *dtap,DIRLIST *parent,DIRLIST *child)
{
DIRLIST *tempd;

	child->offset = 0;
	tempd = parent;
	while(tempd->xtradir)
		tempd = tempd->xtradir;
	if ((tempd->offset == dpb.bpc) && (dirlevel != 0)) {
		if (!run)				/* no clusters available to extend directory ... */
			return OUT_OF_SPACE;
		if ((tempd->xtradir=calloc(1,sizeof(DIRLIST))) == NULL) {
			error(sizeof(DIRLIST),MEMALLOC,"");
			return -1;
		}
		fat16[tempd->firstcluster] = nextcluster;
		tempd = tempd->xtradir;
		if ((tempd->dirbuf=calloc(1,dpb.bpc)) == NULL) {
			error(dpb.bpc,MEMALLOC,"");
			return -1;
		}
		tempd->offset = 0;
		tempd->firstcluster = nextcluster;
		tempd->xtradir = NULL;
		calc_nextcluster();
	}

	if (!run)					/* no clusters available to create subdirectory ... */
		return OUT_OF_SPACE;

	dir_entry(tempd,dtap,nextcluster,0);
	fat16[nextcluster] = dpb.bitmask;
	if ((child->dirbuf=calloc(1,dpb.bpc)) == NULL) {
		error(dpb.bpc,MEMALLOC,"");
		myfdelete(tempd);		/* undo effects of dir_entry() */
		return -1;
	}
	dir_entry(child,dtap,nextcluster,1);
	dir_entry(child,dtap,parent->firstcluster,2);
	child->firstcluster = nextcluster;
	child->xtradir = NULL;
	calc_nextcluster();
	dirlevel++;

	return 0;
}

PRIVATE void myfdelete(DIRLIST *dir)
{
	if (dir) {
		dir->offset -= sizeof(DIRENTRY);
		memset(dir->dirbuf+dir->offset,0,sizeof(DIRENTRY));
//		fat16[nextcluster] = 0;				//BUG i think - why did we do it???
	}
}

/*
 *	write FATs + root to destination partition
 */
PRIVATE int cleanup_disk(void)
{
int fail = 0;

	swab((char *)fat16,(char *)fat16,dpb.bpf);
	if (log_write(dev,dpb.spf,1,fat16,FAT))
		fail = -1;

	if (log_write(dev,dpb.spf,1+dpb.spf,fat16,FAT))
		fail = -1;

	if (log_write(dev,dpb.rds,1+2*dpb.spf,dirlist[0].dirbuf,DIR))
		fail = -1;

	_mediach(dev);

	return fail;
}

/********************************************/
PRIVATE void write_dir(DIRLIST *dir)
{
unsigned short sector;
short j = 0;
DIRLIST *temp;

	do {
		sector = logsector(dir->firstcluster);
		if (log_write(dev,dpb.spc,sector,dir->dirbuf,DIR))
			return;
		free(dir->dirbuf);
		temp = (DIRLIST *)dir->xtradir;
		if (j > 0)
			free(dir);
		else memset(dir,0,sizeof(DIRLIST));
		dir = temp;
		j++;
	} while (dir != NULL);
}

/*********************************/
PRIVATE void get_dirname(char *source,char *dirname,short level)
{
register short i, j;
short len, dirsfound = 0;

	len = strlen(source);
	i = 3;
	while (i < len) {
		j = 0;
		while ((*(source + i) != '\\') && (i < len)) {
			*(dirname + j) = *(source + i);
			j++;
			i++;
		}
		if (i < len) {
			*(dirname + j) = '\0';
			i++;
			dirsfound++;
			if (dirsfound == level)
				return;
		}
	}
	*dirname = '\0';
}

/******************************************/
PRIVATE void cleanup_dirs(char *curpath,char *nextpath,short override)
{
short j, k;
char next[MAX_FILENAME], cur[MAX_FILENAME];

	k = dirlevel + 1;
	for (j = 1; j < k; j++) {
		if (override) {
			write_dir(&dirlist[j]);
		} else {
			get_dirname(curpath,cur,j);
			get_dirname(nextpath,next,j);
			if (strcmp(cur,next) != 0)
				write_dir(&dirlist[j]);
		}
	}
	if (override)
		dirlevel = 0;
}


/****************************************************************/
PRIVATE long estimate(char *path)
{
int ret;
long clusters = 0L;
DTA dta;
char pbuff[MAX_PATHNAME], pathsearch[MAX_PATHNAME];

	sprintf(pathsearch,"%s\\*.*",path);
	Fsetdta(&dta);
	ret = Fsfirst(pathsearch,FA_SUBDIR);
	while (ret >= 0) {
		if (dta.fattr & FA_SUBDIR) {
			if (strcmp(dta.name,".") && strcmp(dta.name,"..")) {
				sprintf(pbuff,"%s\\%s",path,dta.name);
				clusters += estimate(pbuff);
				Fsetdta(&dta);
			}
		}
		ret = Fsnext();
	}
	ret = Fsfirst(pathsearch,FA_SYSTEM|FA_HIDDEN);	/* was FA_HIDDEN */
	while (ret >= 0) {
		clusters += (dta.fsize+dpb.bpc-1) >> dpb.cluster_shift;
		ret = Fsnext();
	}

	return clusters;
}
