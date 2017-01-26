/*
 *	medic1.c: implements;
 *				the test/repair disk function
 *				the 'map bad' function
 *
 *	version 2.10 (december/2002)		roger burrows
 *		a highly cleaned up and reorganised version of the DE v2.04
 *		source; no new function
 *
 *	version 2.50 (april/2003)			roger burrows
 *		we now display bad sectors in the FAT/root area, in do_wr()
 *
 *	version 2.53 (july/2003)			roger burrows
 *		1. do_tdisk() now returns the maximum number (i.e. high-water mark) of
 *		   directory clusters (excluding the root) that it had to store while
 *		   making its pass over the whole partition.  this is used by the
 *		   optimisation routines in opt2.c to ensure that enough memory is
 *		   available before they start to modify the partition ...
 *		2. fix multi_pause() to handle times > 65 secs (precautionary,
 *		   currently max time is 9 secs anyway)
 *		3. for multiple map bad, control-C now aborts back to main window
 *		4. code cleanup:
 *			. eliminate mapx/mapy (no longer used by dmap.c) and gmap (no
 *			  longer used by medic1.c)
 *			. remove unnecessary setting of opt.visual in do_tdisk()
 *
 *	version 2.54 (august/2003)			roger burrows
 *		. add DEMO support for Medic & Map bad
 *		. fix test for directory in test_loop()/bad_loop(): it was
 *		  (attr==FA_SUBDIR), which failed when any other bit was set;
 *		  it's now (attr&FA_SUBDIR != 0)
 *
 *	version 2.55 (february/2007)		roger burrows
 *		. add cluster number to file collision message
 *		. removed commented-out code (neatness)
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <assert.h>
#include <dos.h>
#include <oserr.h>
#include <portab.h>
#include <stdarg.h>
#include <string.h>
#include <vdi.h>

#include <misc.h>				/* for compare_memory() */

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"

#define MODULE_NUMBER	7		/* used by error() macro */

struct bad_list {
	unsigned short badclus;
	struct bad_list *next_bad;
};
typedef struct bad_list BAD;

extern short usermode, win1;
extern short dev, nplanes, xchar;
extern short myhandle;
extern MYBPB dpb;
extern short dirlevel;
extern DIRLIST dirlist[MAX_DIR_DEPTH];
extern STATUS status;
extern OPT opt;
extern OBJECT *badpref, *medic, *menu, *text;


/*
 *	standard filename character validation table (shared with fileio.c, undel.c)
 */
char ascii[256] = {
			   /* 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
	/* 0- 15 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 16- 31 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 32- 47 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
	/* 48- 64 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
	/* 64- 79 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 80- 95 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
	/* 96-111 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 112-127 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
	/* 128-143 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 144-159 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 160-175 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 176-191 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 192-207 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 208-223 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 224-239 */ 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 240-255 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*
 *	private variables
 */
MLOCAL DIRENTRY *curfile;	/* this use to be shared with opt2.c, but I'm 99.9% */
							/* sure that it was just due to the name being the same */

MLOCAL char ascii2[256] = {	/* filename character validation table (used iff opt.ignore == TRUE) */
			   /* 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
	/* 0- 15 */   0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 16- 31 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 32- 47 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
	/* 48- 64 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
	/* 64- 79 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 80- 95 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
	/* 96-111 */  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 112-127 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 128-143 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 144-159 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 160-175 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 176-191 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 192-207 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 208-223 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 224-239 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 241-255 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };


#define BADDIR		0		/* error counts */
#define READERR 	1
#define BADFNAME	2
#define BADENTRY	3
#define BADSIZE 	4
#define BADFAT		5
#define COLLISION	6
#define LOSTCLUSTER	7
#define MAXERRTYPE	LOSTCLUSTER
#define NERRTYPE	(MAXERRTYPE+1)
MLOCAL short errcnt[NERRTYPE];
 
MLOCAL char medic_fname[MAX_PATHNAME];
MLOCAL FILE *efile = NULL;		/* associated with 'medic_fname' */

MLOCAL short nbadclus;

MLOCAL struct {
#define RR	   0
#define RWR	   1
#define WR	   2
	int type;					/* type of mapping */
	int passes;					/* number of passes */
} mapbad;

MLOCAL void (*do_block)(short,short);	/* do_visblock/do_novisblock */

MLOCAL BAD *firstbad = NULL;	/* start of chain of bad clusters */
MLOCAL BAD *curbad = NULL;		/* current entry */

MLOCAL BAD *firstfatbad = NULL;	/* start of chain of bad sectors in FAT/root area */
MLOCAL BAD *curfatbad = NULL;

MLOCAL short *tfat;

MLOCAL char *fat12;
MLOCAL unsigned short *fat16;

MLOCAL int fix;

/*
 *	the following variables track the max# of directory clusters stored.
 *	this info is used by the optimisation routines to check that there is
 *	sufficient memory before they start to modify the partition
 *
 *	variables are initialised in test_disk(), incremented in down_dirlist(),
 *	decremented in up_dirlist()
 */
MLOCAL int cur_clusters, max_clusters;


/*
 *	function prototypes
 */
PRIVATE int alloc_bad(void);
PRIVATE int alloc_fatbad(void);
PRIVATE int bad_loop(short dstart,char *enddir,short mustwrite);
PRIVATE int check_bad(DIRENTRY *file,short nclus,long size);
PRIVATE int check_fat(DIRENTRY *file,short *badflag,char *pathin);
PRIVATE void check_garbage(char *pathin);
PRIVATE int compare_fats(char *fat1,char *fat2);
PRIVATE int discard_lost_clusters(unsigned short *f16,unsigned short *tfat);
PRIVATE void display_bad(BAD *curbad);
PRIVATE void do_novisblock(short start,short flag);
PRIVATE int do_rr(int passes,char *buf,long bufsize);
PRIVATE int do_rwr(int passes,char *buf,long bufsize);
PRIVATE void do_visblock(short start,short flag);
PRIVATE int do_wr(int passes,char *buf,long bufsize);
PRIVATE int down_dirlist(int ncl);
PRIVATE int fix_collision(DIRENTRY *file,short start,short fixstart,short prev);
PRIVATE void free_all_bad(void);
PRIVATE void free_all_fatbad(void);
PRIVATE void free_pointers(void);
PRIVATE void get_lostclus_name(char *tname,char *endpoint);
PRIVATE int is_bad_cluster(short cluster);
PRIVATE int load_directory(short *dirstart,int *ncl,short *mustwrite,char *pathin);
PRIVATE int load_directory_bad(short *dirstart,int *ncl,short *mustwrite);
PRIVATE char *load_root(short *mwrite,char *pathin);
PRIVATE int mark_bad_clusters(void);
PRIVATE int multi_pause(int numsec);
PRIVATE int open_errfile(void);
PRIVATE int process_bad(void);
PRIVATE void redraw_special(OBJECT *tree,short idex,short width);
PRIVATE int rr_bysector(char *ptr1,char *ptr2,short nread,short clus);
PRIVATE int rwr_bysector(char *ptr1,char *ptr2,short nread,short clus);
PRIVATE int test_disk(int called,int *problems);
PRIVATE int test_loop(short dstart,char *enddir,short twrite,char *pathin);
PRIVATE void up_dirlist(void);
PRIVATE void update_activity(int obj,int mode);
PRIVATE int val_bad(DIRENTRY *dirstart,char *end,short *needtowrite);
PRIVATE int val_direntries(DIRENTRY *dirstart,char *end,short *needtowrite,char *pathin);
PRIVATE int wait_bad(void);
PRIVATE int wr_bysector(char *ptr1,char *ptr2,short nread,short clus);
PRIVATE int wr_fat(char *ptr1,char *ptr2,short nread,unsigned short secloc);
PRIVATE void write_errfile(char *fmt, ...);



/*
 *	perform test disk
 *
 *	returns -ve for failure, else max# dir clusters stored
 */
PUBLIC int do_tdisk(int called)
{
int problems = 0;
int rc;

	toggle_bottom(BTESTD);
	fix = opt.autofix;

	rc = test_disk(called,&problems);
	if (rc == 0)				/* problems found, fix was 0, user wants to fix */
		rc = test_disk(called,&problems);

	if (rc > 0)					/* ok */
		rc = max_clusters;

	if (efile) {
		fclose(efile);
		efile = NULL;
	}

	free_pointers();
	cleanup_diskmap();			/* because we use draw_block() */

	switch(status.action) {
	case DISKINFO:
		if (fix && (problems > 0))
			status.infodev = -1;
		break;
	case DISKMAP:
		if (fix && (problems > 0))
			status.mapdev = -1;
		break;
	case UNDELETE:
		break;
	default:
		if (fix && (problems > 0))
			mark_chart(dev);
		break;
	}

	return rc;
}

/*
 *	do the actual disk test
 *
 *	returns	1	OK, done (max# clusters stored is in max_clusters)
 *			0	retry (test completed with fix = 0, user wants to fix)
 *			-1	fail
 */
PRIVATE int test_disk(int called,int *problems)
{
short start_fld;
short fail = 0;
short faterr, obj;
int n, rc;
char *fat1, *fat2, *end;
char drv[4];
TEDINFO *blocks;
static char title[MAX_DISPLAYSIZE];

	blocks = (TEDINFO *) medic[MBLOCKS].ob_spec;

	nbadclus = 0;
	faterr = 0;
	fail = 0;
	end = NULL;
	medic[MRAREA].ob_flags |= HIDETREE;
	medic[MSAREA].ob_flags &= ~HIDETREE;	/* display scanning panel */
	medic[MBAREA].ob_flags |= HIDETREE;
	medic[MBADT1].ob_flags |= HIDETREE;
	medic[MBADT2].ob_flags |= HIDETREE;
	medic[MBADPASS].ob_flags |= HIDETREE;
	medic[VERROR].ob_state &= ~DISABLED;
	medic[REPCAN].ob_state &= ~DISABLED;

	strcpy(medic[VBOOTS].ob_spec,"");
	strcpy(medic[VFAT12S].ob_spec,"");
	strcpy(medic[VDIRS].ob_spec,"");
	strcpy(medic[VDIRES].ob_spec,"");
	strcpy(medic[VFATS].ob_spec,"");
	if (fix)
		strcpy(medic[FIXNOW].ob_spec,text[MEDIC_CONTINUE].ob_spec);
	else strcpy(medic[FIXNOW].ob_spec,text[MEDIC_FIXNOW].ob_spec);

	strcpy(medic[MBAD].ob_spec,text[MISC_NA].ob_spec);

	sprintf(title,text[MEDIC_TITLE].ob_spec,dev+'A');
	wind_set(win1,WF_NAME,ADDR(title));
	wind_set(win1,WF_INFO,ADDR(text[MEDIC_INFO].ob_spec));
	sprintf(drv,"%c:\\",'A'+dev);

	vsf_interior(myhandle,FIS_PATTERN);
	vsf_style(myhandle,8);
	vsf_color(myhandle,GREEN);
	if (nplanes == 1)
		vsf_color(myhandle,BLACK);

	strcpy(blocks->te_ptext,"0.0");
	redraw_object(medic,0);

	if (get_mybpb(&dpb,dev) < 0) {
		strcpy(medic[VBOOTS].ob_spec,text[MISC_ERROR].ob_spec);
		redraw_object(medic,VBOOTS);
		xform_alert(1,MD_BADBOOT_ALERT);
		return 1;
	}
	strcpy(medic[VBOOTS].ob_spec,text[MISC_OK].ob_spec);
	redraw_object(medic,VBOOTS);

	format_cpb(blocks->te_ptext);	/* display blocks/cluster as m.n */
	redraw_object(medic,MBLOCKS);

	if (dpb.isfat16) {										/* fat16 */
		if ((fat1=(char *)fat16=malloc(dpb.bpf * 2L)) == NULL) {
			error(dpb.bpf*2,MEMALLOC,"");
			return 1;
		}
		fat2 = fat1 + dpb.bpf;
		tfat = (unsigned short *)fat2;
		rc = compare_fats(fat1,fat2);
		if (rc < 0)
			return -1;
		if (rc > 0)
			faterr = 1;

		swab(fat16,fat16,dpb.bpf);
		swab(tfat,tfat,dpb.bpf);
	} else {												/* fat12 */
		if ((fat12=fat1=malloc(dpb.bpf * 4L)) == NULL) {
			error(dpb.bpf*4,MEMALLOC,"");
			return 1;
		}
		fat2 = fat1 + 2 * dpb.bpf;
		rc = compare_fats(fat1,fat2);
		if (rc < 0)
			return -1;
		if (rc > 0)
			faterr = 1;

		if ((fat16=malloc(dpb.bpf * 4L)) == NULL) {
			error(dpb.bpf*4,MEMALLOC,"");
			return 1;
		}
		fat12_to_fat16(fat12,fat16);
		tfat = fat16 + dpb.bpf;
		fast_copy((char *)tfat,fat16,dpb.bpf * 2);
	}

	dirlevel = 0;
	memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));
	for (n = 0; n < NERRTYPE; n++)
		errcnt[n] = 0;

	wind_update(BEG_UPDATE);
	mouse_off();
	set_clip();

	if (opt.visual) {
		draw_block(0,CLUSTER_OK);
		draw_block(1,CLUSTER_OK);
		do_block = &do_visblock;
	} else do_block = &do_novisblock;

	cur_clusters = max_clusters = 0L;		/* track memory usage in terms of clusters */

	if (test_loop(0,end,0,drv) < 0) {
		mouse_on();
		xform_alert(1,GEN_OPFAIL_ALERT);
		wind_update(END_UPDATE);
		return -1;
	}
	check_garbage(drv);
	if (fix) {
		if (dpb.isfat16) {				/* fat16 */
			swab(fat16,fat16,dpb.bpf);
			fat12 = (char *)fat16;
			fat16 = NULL;
		} else {						/* fat12 */
			fat16_to_fat12(fat16,fat12);
		}
		must_write(dev,dpb.spf,1,fat12,FAT);
		must_write(dev,dpb.spf,1+dpb.spf,fat12,FAT);
	}
	mouse_on();
	wind_update(END_UPDATE);
	medic[MBAREA].ob_flags |= HIDETREE;
	medic[MSAREA].ob_flags |= HIDETREE;
	medic[MRAREA].ob_flags &= ~HIDETREE;	/* display logical disk report */
	if (efile) {
		fclose(efile);
		efile = NULL;
	}

	for (n = 0, *problems = 0; n < NERRTYPE; n++) {
		sprintf(medic[ER1+n].ob_spec,"%5d",errcnt[n]);
		*problems += errcnt[n];
	}
	sprintf(medic[BADCLUS].ob_spec,"%5d",nbadclus);
	if (faterr) {
		strcpy(medic[FAT12].ob_spec,text[MISC_ERROR].ob_spec);
		*problems++;
	} else sprintf(medic[FAT12].ob_spec,"%5s",text[MISC_OK].ob_spec);

	if (!*problems) {
		strcpy(medic[REPACT].ob_spec,text[MEDIC_NOPROBLEMS].ob_spec);
		strcpy(medic[FIXNOW].ob_spec,text[MEDIC_CONTINUE].ob_spec);
		medic[VERROR].ob_state |= DISABLED;
		if (!called)
			medic[REPCAN].ob_state |= DISABLED;
	} else if (fix)
		strcpy(medic[REPACT].ob_spec,text[MEDIC_DEFIX_DONE].ob_spec);
	else strcpy(medic[REPACT].ob_spec,text[MEDIC_NEED_DEFIX].ob_spec);

	redraw_object(medic,MRAREA);

	if (*problems || !called)
		ring_bell();
	else if (opt.mpause != PAUSE_FOREVER) {
		if (multi_pause(opt.mpause) == REPCAN)
			return -1;
		return 1;
	}

	while(TRUE) {
		start_fld = 0;
		obj = myform_do(medic,&start_fld,0) & 0x7fff;
		medic[obj].ob_state &= ~SELECTED;
		redraw_object(medic,obj);
		if (obj != VERROR)
			break;
		save_screen();
		view_file(medic_fname,0);
		restore_screen();
		sprintf(title,text[MEDIC_TITLE].ob_spec,dev+'A');
		wind_set(win1,WF_NAME,ADDR(title));
		wind_set(win1,WF_INFO,ADDR(text[MEDIC_INFO].ob_spec));
	}
	if (*problems && !fix) {
		if (obj == FIXNOW) {
#ifdef DEMO
			form_alert(1,DEMO_UNAVAIL);
#else
			fix = 1;
			free_pointers();
			return 0;			/* try again ... */
#endif
		}
		return -1;
	}
	if (obj == REPCAN)
		return -1;

	return 1;
}

/*
 *	compare 2 copies of FAT
 */
PRIVATE int compare_fats(char *fat1,char *fat2)
{
int saveans;

	if (log_read(dev,dpb.spf,1,fat1,FAT))
		return -1;

	if (log_read(dev,dpb.spf,1+dpb.spf,fat2,FAT))
		return -1;

	if (compare_memory(fat1,fat2,dpb.bpf) == 0) {		/* all ok */
		strcpy(medic[VFAT12S].ob_spec,text[MISC_OK].ob_spec);
		redraw_object(medic,VFAT12S);
		return 0;
	}

	strcpy(medic[VFAT12S].ob_spec,text[MISC_ERROR].ob_spec);
	redraw_object(medic,VFAT12S);
	write_errfile("%c:\\  %s",'A'+dev,text[MEDLOG_FATDIFF].ob_spec);

	if ((saveans=xform_alert(1,MD_FATERR_ALERT)) == 3)
		return -1;

	if (fix) {
		if (xform_alert(1,MD_AUTOFIX_ALERT) == 1) {
			fix = 0;
			strcpy(medic[FIXNOW].ob_spec,text[MEDIC_FIXNOW].ob_spec);
		}
	}

	write_errfile("%s%d\n",text[MEDLOG_USEFAT].ob_spec,saveans);

	switch(saveans) {
	case 1:
		fast_copy(fat2,fat1,dpb.bpf);
		break;
	case 2:
		fast_copy(fat1,fat2,dpb.bpf);
	}

	return 1;
}

/*
 *	go 'down' the dirlist, i.e. allocate buffer for next lower directory
 *	note that 'dirlevel' has already been incremented
 *
 *	if allocation fails, issue error msg, backup to higher directory, return -1
 *	else return 0
 */
PRIVATE int down_dirlist(int ncl)
{
	if ((dirlist[dirlevel].dirbuf=calloc(ncl,dpb.bpc)) == NULL) {
		error(ncl*dpb.bpc,MEMALLOC,"");
		dirlevel--;
		if (dirlevel > 0)
			up_dirlist();
		return -1;
	}

	dirlist[dirlevel].size = ncl;
	cur_clusters += ncl;
	if (cur_clusters > max_clusters)
		max_clusters = cur_clusters;

	return 0;
}

/*
 *	go 'up' the dirlist, i.e. free directory buffer and return to higher directory
 */
PRIVATE void up_dirlist(void)
{
	free(dirlist[dirlevel].dirbuf);
	cur_clusters -= dirlist[dirlevel].size;
	memset(&dirlist[dirlevel],0x00,sizeof(DIRLIST));
	dirlevel--;
}

/*****************************************************************************/
/***************************************************************************/
PRIVATE int test_loop(short dstart,char *enddir,short twrite,char *pathin)
{
short st, sc;
short dirstart, mustwrite, start;
DIRENTRY *savecfile;
int i, ncl, rc;
long offset;
short errflag = 0;
short needtowrite = 0;
char *endpoint;
char *end;
char *temp;
char dname[MAX_FILENAME];
char pathout[MAX_ERRFILEMSGSIZE];

	mustwrite = twrite;
	if (dstart == 0) {
		if ((enddir=load_root(&mustwrite,pathin)) == NULL)
			return -1;
	}
	if (get_key() < 0)
		return -1;

	curfile = (DIRENTRY *)dirlist[dirlevel].dirbuf;
	if (dirlevel == 0) {
		if (curfile->attr == FA_VOLUME)
			curfile++;
	} else curfile += 2;

	for ( ; curfile->attr & FA_SUBDIR; curfile++) {
		if (curfile->name[0] == DELETION_MARK)
			continue;
		errflag = 0;
		dirlevel++;
		st = dirstart = get_word((char *)&curfile->start);
		ncl = 1;

		while (fat16[st] != dpb.bitmask) {
			sc = st;
			st = fat16[st];
			if (!valid_cluster(st)) {
				/* diskerr(1); */
				errcnt[BADDIR]++;
				get_name(curfile,dname);
				write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADDIR_VC].ob_spec);
				if (!fix)
					errflag = 1;
				fat16[sc] = dpb.bitmask;
				tfat[sc] = dpb.bitmask;
				break;
			}
			ncl++;
		}

		if (down_dirlist(ncl) < 0)
			return -1;

		while((rc=load_directory(&dirstart,&ncl,&mustwrite,pathin)) > 0)
			;

		if (rc < 0)
			continue;

		savecfile = (DIRENTRY *)dirlist[dirlevel].dirbuf;
		get_name(savecfile,dname);
		start = get_word((char *)&savecfile->start);
		if ((strcmp(dname,".") != 0) || (start != dirstart)) {
			errcnt[BADDIR]++;
			get_name(curfile,dname);
			write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADDIR_DT].ob_spec);
			if (fix)
				mustwrite = 1;
			fat16[dirstart] = 0;
			curfile->name[0] = DELETION_MARK;
			up_dirlist();
			continue;
		}
		savecfile++;
		get_name(savecfile,dname);
		start = get_word((char *)&savecfile->start);
		if ((strcmp(dname,"..") != 0) || (start != dstart)) {
			errcnt[BADDIR]++;
			get_name(curfile,dname);
			write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADDIR_DT].ob_spec);
			if (fix)
				mustwrite = 1;
			fat16[dirstart] = 0;
			curfile->name[0] = DELETION_MARK;
			up_dirlist();
			continue;
		}

		endpoint = sort_dir(dirlist[dirlevel].dirbuf,ncl,0);	/* don't remove deleted entries */
		end = dirlist[dirlevel].dirbuf + dpb.bpc * ncl;

		for (temp = endpoint; temp < end; temp += sizeof(DIRENTRY)) {
			for (i = 0; i < sizeof(DIRENTRY); i++) {
				if (*(temp + i)) {
					/* diskerr(3); */
					errcnt[BADENTRY]++;
					get_name(curfile,dname);
					write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADENTRY].ob_spec);
					memset(temp,0x00,sizeof(DIRENTRY));
					if (fix)
						needtowrite = 1;
					else errflag = 1;
					break;
				}
			}
		}
		if (errflag) {
			do_block(dirstart,CLUSTER_ERR);
			update_activity(VFATS,INACTIVE_MODE);
			update_activity(VDIRS,ERROR_MODE);
		} else {
			do_block(dirstart,CLUSTER_OK);
			update_activity(VFATS,INACTIVE_MODE);
			update_activity(VDIRS,ACTIVE_MODE);
		}

		get_name(curfile,dname);
		sprintf(pathout,"%s%s\\",pathin,dname);

		if (val_direntries((DIRENTRY *)dirlist[dirlevel].dirbuf,endpoint,&needtowrite,pathout) < 0) {
			do_block(dirstart,CLUSTER_ERR);
			update_activity(VDIRS,INACTIVE_MODE);
			update_activity(VDIRES,ERROR_MODE);
		} else {
			do_block(dirstart,CLUSTER_OK);
			update_activity(VDIRS,INACTIVE_MODE);
			update_activity(VDIRES,ACTIVE_MODE);
		}

		savecfile = curfile;

		if (test_loop(dirstart,endpoint,needtowrite,pathout) < 0) {
			up_dirlist();
			return -1;
		}
		curfile = savecfile;
	}

	errflag = 0;
	while ((void *) curfile < (void *) enddir) {
		if (curfile->name[0] != DELETION_MARK) {
			rc = check_fat(curfile,&errflag,pathin);
			if (rc < 0) {
				curfile->name[0] = DELETION_MARK;
				mustwrite = 1;
			} else if (rc == 1) {
				mustwrite = 1;
			}
		}
		curfile++;
	}
	if (errflag) {
		update_activity(VDIRES,INACTIVE_MODE);
		update_activity(VFATS,ERROR_MODE);
	} else {
		update_activity(VDIRES,INACTIVE_MODE);
		update_activity(VFATS,ACTIVE_MODE);
	}

	if (mustwrite) {
		if (dirlevel > 0) {
			st = dstart;
			offset = 0;

			do {
				must_write(dev,dpb.spc,logsector(st),dirlist[dirlevel].dirbuf+offset,DIR);
				st = fat16[st];
				offset += (long) dpb.bpc;
			} while (st != dpb.bitmask);
		} else {
			must_write(dev,dpb.rds,1+2*dpb.spf,dirlist[0].dirbuf,DIR);
		}
	}
	up_dirlist();

	return 0;
}

/*
 *	load directory into memory, handling unreadable sectors
 *
 *	if fix==1, and any directory sector is unreadable, we drop the
 *	corresponding cluster, fixing up the FAT chain and the parent
 *	directory entry as required
 *
 *	the meaning of the return code depends on the value of 'fix':
 *		1	fix==0	can't happen
 *			fix==1	first cluster in the chain is unreadable; parent
 *					directory entry has been updated to point to the next
 *		0	fix==0	no read errors
 *			fix==1	the directory has been loaded; at least one readable
 *					directory cluster exists
 *		-1	fix==0	a read error was detected
 *			fix==1	nothing loaded: all of the directory clusters were bad
 */
PRIVATE int load_directory(short *dirstart,int *ncl,short *mustwrite,char *pathin)
{
char dname[MAX_FILENAME];
short st, prev;
long offset;

	offset = 0L;
	st = prev = *dirstart;	/* we only initialise prev to prevent erroneous 'warning 94' messages */
	do {
		if (log_read(dev,dpb.spc,logsector(st),dirlist[dirlevel].dirbuf+offset,DIR)) {
			/* diskerr(2); */
			errcnt[READERR]++;
			get_name(curfile,dname);
			write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_READERR].ob_spec);
			if (fix) {
				if (*ncl == 1) {
					*mustwrite = 1;
					fat16[st] = dpb.badmask;
					tfat[st] = 1;
					curfile->name[0] = DELETION_MARK;
					up_dirlist();
					return -1;
				} else {
					*ncl--;
					if (st == *dirstart) {
						*mustwrite = 1;
						*dirstart = fat16[st];
						fat16[st] = dpb.badmask;
						tfat[st] = 1;
						putword(&curfile->start,*dirstart);
						assert(offset==0L);		/* temporary paranoia */
						return 1;
					} else {
						tfat[st] = 1;
						fat16[prev] = fat16[st];
						st = fat16[st];
						fat16[st] = dpb.badmask;
						continue;
					}
				}
			} else {
				do_block(*dirstart,CLUSTER_ERR);
				update_activity(VFATS,INACTIVE_MODE);
				update_activity(VDIRS,ERROR_MODE);
				up_dirlist();
				return -1;
			}
		}
		prev = st;
		st = fat16[st];
		offset += dpb.bpc;
	} while (st != dpb.bitmask);

	return 0;
}

/*
 *	update activity indicator for medic
 *
 *	does minimum amount of redraw for speed
 */
PRIVATE void update_activity(int obj,int mode)
{
char *display;
int redraw_len;

	display = medic[obj].ob_spec;

	switch(*display) {		/* i.e. previous mode */
	case ACTIVE_CHAR:
	case INACTIVE_CHAR:
		redraw_len = xchar;
		break;
	default:
		redraw_len = 5 * xchar;
	}

	switch(mode) {
	case ACTIVE_MODE:
		*display++ = ACTIVE_CHAR;
		*display = '\0';
		break;
	case INACTIVE_MODE:
		*display++ = INACTIVE_CHAR;
		*display = '\0';
		break;
	default:
		strncpy(display,text[MISC_ERROR].ob_spec,5);
		redraw_len = 5 * xchar;
	}

	redraw_special(medic,obj,redraw_len);
}

/**************************************************************************/
PRIVATE void redraw_special(OBJECT *tree,short idex,short width)
{
short xoff, yoff;

	objc_offset(tree,idex,&xoff,&yoff);
	objc_draw(tree,0,MAX_DEPTH,xoff,yoff,width,tree[idex].ob_height);
}

/**********************************************************************/
PRIVATE void do_visblock(short start,short flag)
{
short st;

	st = start;
	do {
		draw_block(st,flag);
		tfat[st] = 1;
		st = fat16[st];
	} while (st != dpb.bitmask);
}

/**********************************************************************/
PRIVATE void do_novisblock(short start,short flag)
{
short st;

	st = start;
	do {
		tfat[st] = 1;
		st = fat16[st];
	} while (st != dpb.bitmask);
}

/*
 *	validate directory entries
 *
 *	checks for the following problems:
 *		. garbage in attribute, reserved area, filename, cluster start, filesize
 *		. duplicate filenames
 *	accumulates the following counts:
 *		. BADFNAME, BADENTRY
 *	logs the following to the error file:
 *		. garbage in filename
 *		. duplicate filenames
 *		. garbage in more than one of the above areas
 *	performs the following fixes on the in-memory data:
 *		. changes garbage characters in filename to 'X'
 *		. if 'fix' is set, allows user to change a duplicate filename; if she doesn't
 *		  or 'fix' wasn't set, marks file as deleted
 *		. marks files with garbage in more than one area as deleted
 *
 *	if an error is logged and 'fix' is set, the 'needtowrite' flag will be set.
 *
 *	returns	-1	if 'fix' is clear and an error is logged (but see code comment)
 *			0	otherwise
 */
PRIVATE int val_direntries(DIRENTRY *dirstart,char *end,short *needtowrite,char *pathin)
{
DIRENTRY *dir, *sdir;
char *temp;
int ngarbage, namerr, duperr;
short start, i;
int badflag = 0;
long fsize, n;
char dname[MAX_FILENAME];
char *valid;

	for (dir = dirstart; (void *) dir < (void *) end; dir++) {
		if (dir->name[0] == DELETION_MARK)
			continue;

		ngarbage = namerr = duperr = 0;

		if (dir->attr & BADATTR_MASK)
			ngarbage++;

		for (i = 0, temp = dir->reserved; i < sizeof(dir->reserved); i++, temp++) {
			if (*temp) {
				ngarbage++;
				break;
			}
		}

		valid = opt.ignore?ascii2:ascii;
		for (i = 0, temp = dir->name; i < NAME_LENGTH; i++, temp++) {
			if (!valid[*temp]) {
				/* diskerr(5); */
				if (*temp == 0)
					ngarbage++;
				namerr = 1;
				*temp = REPLACEMENT_CHAR;
			}
		}
		if (namerr)
			ngarbage++;

		start = get_word((char *)&dir->start);
		fsize = get_long((char *)&dir->size);
		if (start || fsize) {
			if (!valid_cluster(start)) {
				/* diskerr(4); */
				ngarbage += 2;
			}
			if ((fsize < 0L) || (fsize > dpb.tbyte)) {
				/* diskerr(44); */
				ngarbage++;
			}
		}

		/*
		 *	check for duplicate filenames
		 */
		if (dir->attr != FA_VOLUME) {
			for (sdir = dirstart; (void *) sdir < (void *) dir; sdir++) {
				if (sdir->attr == FA_VOLUME)
					continue;
				if (memcmp(sdir->name,dir->name,NAME_LENGTH) == 0) {
					duperr = 1;
					if (fix) {
						get_name(dir,dname);
						if (get_newname(dname,pathin,dir->name)) {
							sdir = dirstart - 1;	/* restart at the beginning */
							continue;
						}
					}
					dir->name[0] = DELETION_MARK;
					break;
				}
			}
		}

		/*
		 *	if error(s) detected, log & accumulate
		 */
		if (namerr) {
			n = opt.ignore?BADENTRY:BADFNAME;
			errcnt[n]++;
			get_name(dir,dname);
			n = opt.ignore?MEDLOG_BADENTRY:MEDLOG_BADNAME;
			write_errfile("%s%s %s\n",pathin,dname,text[n].ob_spec);
			if (fix)
				*needtowrite = 1;
			else badflag = -1;
		}

		if (duperr) {
			/* diskerr(16); */
			errcnt[BADFNAME]++;
			get_name(dir,dname);
			write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_DUPNAME].ob_spec);
			if (fix)
				*needtowrite = 1;
			else badflag = -1;
		}

		if (ngarbage > 1) {
			/* diskerr(17); */
			if (!namerr) {
				errcnt[BADENTRY]++;
				get_name(dir,dname);
				write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADENTRY].ob_spec);
			}
			dir->name[0] = DELETION_MARK;
			if (fix)
				*needtowrite = 1;
			else badflag = -1;		/* added by rfb */
		}
	}

	return badflag;
}

/*************************************************************************/

PRIVATE int check_fat(DIRENTRY *file,short *badflag,char *pathin)
{
short st, prev;
short start, nclus;
short needtowrite = 0;
int i, ncollision = 0;
long size;
char dname[MAX_FILENAME];

	size = get_long((char *)&file->size);
	nclus = (size+dpb.bpc-1) >> dpb.cluster_shift;
	if (size < 0) {						//rfb: shouldn't we flag this ???
		nclus = 0;
	}

	st = start = get_word((char *)&file->start);
	/*
	 *	NOTE: we know that 'st' must be a valid cluster number, because if it
	 *	wasn't, val_direntries() would have marked this entry to be deleted,
	 *	and check_fat() wouldn't have been called for it ...
	 */
	if (st == 0) {
		if (size != 0L) {
			/* diskerr(6); */
			errcnt[BADSIZE]++;
			get_name(file,dname);
			write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADSIZE].ob_spec);
			if (fix)
				return -1;
		}
		return 0;
	}

	if (size == 0L)
		nclus = 1;

	if (tfat[st] == 1) {
		/* diskerr(7); */
		errcnt[COLLISION]++;
		get_name(file,dname);
		write_errfile("%s%s %s %d\n",pathin,dname,text[MEDLOG_COLLISION].ob_spec,st);
		if (fix) {
			if (fix_collision(file,start,st,start) < 0)
				return -1;
			st = start = get_word((char *)&file->start);
			needtowrite = 1;
		} else {
			*badflag = 1;
			return 0;
		}
	}

	for (i = 0; i <= dpb.cdata; ) {		/* ensure FAT loop doesn't cause program loop */
		prev = st;
		st = fat16[st];
		i++;

		if (st == dpb.bitmask)
			break;

		if (!valid_cluster(st)) {
			/* diskerr(10); */
			errcnt[BADFAT]++;
			get_name(file,dname);
			write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADFAT_VC].ob_spec);
			fat16[prev] = dpb.bitmask;
			if (fix) {
				putlong(&file->size,((long)i)<<dpb.cluster_shift);
				needtowrite = 1;
			} else *badflag = 1;
			i = nclus;	/* force match to avoid BADSIZE error as well */
			break;
		}

		if (tfat[st] == 1) {
			/* diskerr(8); */
			if (ncollision == 0) {
				errcnt[COLLISION]++;
				get_name(file,dname);
				write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_COLLISION].ob_spec);
				ncollision++;
			}
			if (fix) {
				if (fix_collision(file,start,st,prev) < 0)
					return -1;
				st = fat16[prev];
			} else *badflag = 1;
		}
	}

	if (i > dpb.cdata) {				/* this is a FAT loop ... */
		/* diskerr(11); */
		errcnt[BADFAT]++;
		get_name(file,dname);
		write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADFAT_LP].ob_spec);
		if (fix)
			return -1;
		else return 0;
	}

	if (i != nclus) {
		/* diskerr(12); */
		errcnt[BADSIZE]++;
		get_name(file,dname);
		write_errfile("%s%s %s\n",pathin,dname,text[MEDLOG_BADSIZE].ob_spec);
		if (fix) {
			putlong(&file->size,((long)i)<<dpb.cluster_shift);
			needtowrite = 1;
		} else *badflag = 1;
	}

	if (*badflag)
		do_block(start,CLUSTER_ERR);
	else do_block(start,CLUSTER_OK);

	return needtowrite;
}

/*
 *	copy clusters for a collision file to available clusters, building new FAT chain
 *		start		the first cluster of the file
 *		fixstart	the first collision cluster
 *		prev		the last non-collision cluster
 */
PRIVATE int fix_collision(DIRENTRY *file,short start,short fixstart,short prev)
{
int st, i;
unsigned short lastused, nextfree;
char *buf;

	/*
	 *	find start of contiguous free sectors at end of partition
	 */
	for (i = 2, lastused = dpb.maxc; i < dpb.maxc; i++)
		if (fat16[i])
			lastused = i;
	nextfree = lastused + 1;
	if (nextfree >= dpb.maxc)			/* there *is* no space at the end ... */
		return -1;

	if ((buf=malloc(dpb.bpc)) == NULL) {
		error(dpb.bpc,MEMALLOC,"");
		return -1;
	}

	/*
	 *	update directory entry or FAT entry as appropriate
	 *
	 *	BUG: this assumes that all of the clusters of the collision can be copied
	 *	successfully.  If there is an error reading or writing, the last cluster of
	 *	the reassigned file will not contain data from the collision.  Since this
	 *	is fairly unlikely, and the files may be fubar anyway, we won't sweat it ...
	 */
	if (start == fixstart)
		putword(&file->start,nextfree);
	else fat16[prev] = nextfree;

	/*
	 *	copy clusters from chain starting at 'fixstart' to free space
	 */
	for (st = fixstart; ; nextfree++) {
		if (log_read(dev,dpb.spc,logsector(st),buf,FILEERR))
			break;
		if (log_write(dev,dpb.spc,logsector(nextfree),buf,FILEERR))
			break;
		st = fat16[st];
		if (st == dpb.bitmask)
			break;
		if (!valid_cluster(st))
			break;
		if (nextfree >= dpb.maxc-1)
			break;
		fat16[nextfree] = nextfree + 1;
	}
	fat16[nextfree] = dpb.bitmask;

	free(buf);
	return 0;
}

/***************************************************************************/
PRIVATE char *load_root(short *mwrite,char *pathin)
{
short i;
char *endpoint;
char *end;
char *temp;
short needtowrite = 0;
char dname[MAX_FILENAME];

	if ((dirlist[0].dirbuf=calloc(1,dpb.rdb)) == NULL) {
		error(dpb.rdb,MEMALLOC,"");
		return NULL;
	}

	if (log_read(dev,dpb.rds,1+2*dpb.spf,dirlist[0].dirbuf,DIR)) {
		free(dirlist[0].dirbuf);				/* added by rfb */
		return NULL;
	}

	endpoint = sort_root(dirlist[0].dirbuf,0);	/* don't remove deleted entries */
	end = dirlist[0].dirbuf + dpb.rdb;
	for (temp = endpoint; temp < end; temp += sizeof(DIRENTRY)) {
		for (i = 0; i < sizeof(DIRENTRY); i++) {
			if (*(temp + i)) {
				/* diskerr(13); */
				errcnt[BADENTRY]++;
				get_name((DIRENTRY *)temp,dname);
				/*
				 * rfb: quick fix here because we're called from bad_loop() which has no pathin (yet)
				 */
				write_errfile("%s%s %s\n",pathin?pathin:"*unknown*",dname,text[MEDLOG_BADENTRY].ob_spec);
				if (fix) {
					needtowrite = 1;
					memset(temp,0x00,sizeof(DIRENTRY));
				}
				break;
			}
		}
	}
	if (val_direntries((DIRENTRY *)dirlist[0].dirbuf,endpoint,&needtowrite,pathin) < 0)
		update_activity(VDIRES,ERROR_MODE);
	else update_activity(VDIRES,ACTIVE_MODE);

	if (needtowrite)
		*mwrite = 1;

	return endpoint;
}

/*********************************************************************/
PRIVATE void check_garbage(char *pathin)
{
short start, curc, st;
int i, num, total;
char *endpoint, *end;
char tname[MAX_FILENAME];
DIRENTRY *d;

	for (i = 2; i < dpb.maxc; i++) {
		if ((tfat[i] != 0) && (tfat[i] != 1)) {
			if (tfat[i] != dpb.badmask) {
				/* diskerr(14); */
				errcnt[LOSTCLUSTER]++;
			} else nbadclus++;
		}
	}
	if (!errcnt[LOSTCLUSTER])
		return;

	write_errfile("%s  %d %s\n",pathin,errcnt[LOSTCLUSTER],text[MEDLOG_LOSTCLUS].ob_spec);
	for (i = 2, num = 0; i < dpb.maxc; i++) {
		if ((tfat[i] != 0) && (tfat[i] != 1)) {
			if (tfat[i] != dpb.badmask) {
				write_errfile(" %5d%c",i,(++num%10)?' ':'\n');
				if (opt.visual)
					draw_block(i,CLUSTER_ERR);
			}
		}
	}
	if (num%10)
		write_errfile("\n");

	if (!fix)
		return;

	if (!opt.savelost) {
		discard_lost_clusters(fat16,tfat);
		return;
	}
	
	if ((dirlist[0].dirbuf=calloc(1,dpb.rdb)) == NULL) { 	/* root dir buffer */
		error(dpb.rdb,MEMALLOC,"");
		return;
	}
	if (log_read(dev,dpb.rds,1+2*dpb.spf,dirlist[0].dirbuf,DIR))
		return;

	endpoint = sort_root(dirlist[0].dirbuf,1);	/* in case all root entries are used but some are deleted,  */
												/* remove deleted entries to make room for LOSTCLUS file(s) */
	end = dirlist[0].dirbuf + dpb.rdb;

	for (total = 0; endpoint < end; endpoint += sizeof(DIRENTRY), total += num) {
		for (i = 2, num = 0; i < dpb.maxc; i++) {
			if ((tfat[i] != 0) && (tfat[i] != 1) && (tfat[i] != dpb.badmask)) {
				num = 1;
				tfat[i] = 1;
				start = curc = i;
				st = fat16[curc];
				while(st != dpb.bitmask) {
					if (!valid_cluster(st) || (tfat[st] == 1) || (tfat[st] == dpb.badmask)) {
						fat16[curc] = dpb.bitmask;
						break;
					}
					num++;
					curc = st;
					tfat[curc] = 1;
					st = fat16[st];
				}
				break;
			}
		}
		if (!num)
			break;

		get_lostclus_name(tname,endpoint);
		d = (DIRENTRY *)endpoint;
		strcpy(endpoint,tname);
		putword(&d->time,Tgettime());
		putword(&d->date,Tgetdate());
		putword(&d->start,start);
		putlong(&d->size,((long)num)<<dpb.cluster_shift);
	}

	if (endpoint >= end) {					/* root dir is full */
		if (xform_alert(1,MD_ROOTFUL_ALERT) == 1)
			discard_lost_clusters(fat16,tfat);
	}

	if (total > 0) {						/* must have created at least one LOSTCLUS file */
		must_write(dev,dpb.rds,1+2*dpb.spf,dirlist[0].dirbuf,DIR);	/* so rewrite root */
	}

	free(dirlist[0].dirbuf);
	memset(&dirlist[0],0x00,sizeof(DIRLIST));
}

PRIVATE int discard_lost_clusters(unsigned short *f16,unsigned short *tfat)
{
int count, i;

	for (i = 2, count = 0; i < dpb.maxc; i++, tfat++, f16++) {
		if ((*tfat != 0) && (*tfat != 1) && (*tfat != dpb.badmask)) {
			*f16 = 0;
			count++;
		}
	}
	return count;		/* not currently used, but who knows? */
}

PRIVATE void get_lostclus_name(char *tname,char *endpoint)
{
int lcnum;
char *curpoint;

	for (lcnum = 0; ; lcnum++) {
		sprintf(tname,LOSTCLUS_FILE,lcnum);
		for (curpoint = dirlist[0].dirbuf; curpoint < endpoint; curpoint += sizeof(DIRENTRY)) {
			if (memcmp(curpoint,tname,NAME_LENGTH) == 0)
				break;
		}
		if (curpoint >= endpoint)
			return;
	}
}

/************************************************
 *												*	
 *	routines to handle the map bad function		*
 *												*	
 ************************************************/
PUBLIC int do_bad(int autofix)
{
int i;
TEDINFO *blocks;
int fail = 0;
static char title[MAX_DISPLAYSIZE];

	fix = autofix;
	blocks = (TEDINFO *) medic[MBLOCKS].ob_spec;

	strcpy(medic[VBOOTS].ob_spec,text[MISC_NA].ob_spec);
	strcpy(medic[VFAT12S].ob_spec,text[MISC_NA].ob_spec);
	strcpy(medic[VDIRS].ob_spec,text[MISC_NA].ob_spec);
	strcpy(medic[VDIRES].ob_spec,text[MISC_NA].ob_spec);
	strcpy(medic[VFATS].ob_spec,text[MISC_NA].ob_spec);

	medic[MBADT1].ob_flags &= ~HIDETREE;
	medic[MBADT2].ob_flags &= ~HIDETREE;
	medic[MBPASSES].ob_flags &= ~HIDETREE;
	medic[MRAREA].ob_flags |= HIDETREE;
	medic[MBAREA].ob_flags |= HIDETREE;
	medic[MSAREA].ob_flags &= ~HIDETREE;		/* display scanning panel */
	strcpy(medic[MBAD].ob_spec,".");
	for (i = BADS1; i < BADS1+MAPBADLIST_ROWS; i++)
		strcpy(medic[i].ob_spec,"");

	switch(mapbad.type) {
	case RR:
		strcpy(medic[MBADT1].ob_spec,text[MAPBAD_RR].ob_spec);
		strcpy(medic[MBADT2].ob_spec,text[MAPBAD_NONDES].ob_spec);
		break;
	case RWR:
		strcpy(medic[MBADT1].ob_spec,text[MAPBAD_RWR].ob_spec);
		strcpy(medic[MBADT2].ob_spec,text[MAPBAD_NONDES].ob_spec);
		break;
	case WR:
		strcpy(medic[MBADT1].ob_spec,text[MAPBAD_WR].ob_spec);
		strcpy(medic[MBADT2].ob_spec,text[MAPBAD_DESTRUCT].ob_spec);
		break;
	}
	sprintf(title,text[MAPBAD_TITLE].ob_spec,dev+'A');
	wind_set(win1,WF_NAME,ADDR(title));
	wind_set(win1,WF_INFO,ADDR(text[MAPBAD_INFO].ob_spec));
	strcpy(blocks->te_ptext,"0.0");
	strcpy(medic[NPASS].ob_spec," 1");
	sprintf(medic[TOTPASS].ob_spec,"%2d",mapbad.passes);
	strcpy(medic[MBAD].ob_spec,"ù");
	redraw_object(medic,0);

	if (fix) {
		strcpy(medic[BADFIXN].ob_spec,text[MEDIC_CONTINUE].ob_spec);
		medic[BADCAN].ob_state |= DISABLED;
	} else {
		strcpy(medic[BADFIXN].ob_spec,text[MEDIC_FIXNOW].ob_spec);
		medic[BADCAN].ob_state &= ~DISABLED;
	}

	vsf_interior(myhandle,FIS_PATTERN);
	vsf_style(myhandle,8);
	vsf_color(myhandle,GREEN);
	if (nplanes == 1)
		vsf_color(myhandle,BLACK);

	ce_off();

	if (get_mybpb(&dpb,dev) < 0) {
		strcpy(medic[VBOOTS].ob_spec,text[MISC_ERROR].ob_spec);
		redraw_object(medic,VBOOTS);
		xform_alert(1,MD_BADBOOT_ALERT);
	} else {
		wind_update(BEG_UPDATE);
		mouse_off();
		format_cpb(blocks->te_ptext);
		redraw_object(medic,MBLOCKS);
		fail = process_bad();
		mouse_on();
		wind_update(END_UPDATE);
	}

	ce_on();

	cleanup_diskmap();			/* because we use draw_block() */
	free_all_bad();
	free_all_fatbad();
	if (!dpb.isfat16)
		fat16 = NULL;
	free_pointers();
	force_update(-1);

	return fail;
}

/*
 *	note we enter here with the mouse hidden!
 */
PRIVATE int process_bad(void)
{
long bufsize;
int rc = 0;
char *iobuf;

	bufsize = MAPBADSIZE;
	if ((iobuf=malloc(2*bufsize)) == NULL) {
		error(2*bufsize,MEMALLOC,"");
		return 0;
	}
	firstbad = firstfatbad = NULL;
	set_clip();
	switch(mapbad.type) {
	case RR:
		rc = do_rr(mapbad.passes,iobuf,bufsize);
		break;
	case RWR:
		rc = do_rwr(mapbad.passes,iobuf,bufsize);
		break;
	case WR:
		rc = do_wr(mapbad.passes,iobuf,bufsize);
		break;
	}
	free(iobuf);
	if (rc < 0)
		return rc;

	/*
	 *	since mark_bad_clusters() calls bad_loop() which calls down_dirlist()
	 *	& up_dirlist(), both of which update cur_clusters/max_clusters, we
	 *	initialise them here.  however it's really for neatness only, since
	 *	we don't actually use the results in this logic path.
	 */
	cur_clusters = max_clusters = 0L;

	rc = mark_bad_clusters();
	if (rc > 0)				/* problems found, fix was 0, user wants to fix */
		rc = mark_bad_clusters();

	return rc;
}

/*
 *	routine to mark bad clusters
 *
 *	if the mapbad.type is not WR, and fix is set, and there are bad clusters,
 *	then we also check if any bad clusters are allocated to a file and if so,
 *	call bad_loop() to fix them up
 *
 *	returns	1	retry (test completed with fix = 0, user wants to fix)
 *			0	OK, done
 *			-1	fail
 */
PRIVATE int mark_bad_clusters(void)
{
int i, n;
int obj, inuse;
BAD *b1;

	/*
	 *	we treat WR mode specially: since we've clobbered any data,
	 *	all we need to do is mark bad clusters and write empty FATs & root
	 */
	if (mapbad.type == WR) {
		/*
		 *	if there's anything in the 'firstfatbad' chain, we're in
		 *	deep trouble (the partition cannot be built).  since WR
		 *	mode is the only mode that can put an entry in the chain,
		 *	we only need to test for it here
		 */
		if (!firstfatbad) {
			free_pointers();
			if (dpb.isfat16) {						/* fat16 */
				if ((fat16=calloc(1,dpb.bpf)) == NULL) {
					error(dpb.bpf,MEMALLOC,"");
					return -1;
				}
			} else {								/* fat12 */
				if ((fat12=calloc(3,dpb.bpf)) == NULL) {
					error(dpb.bpf*3,MEMALLOC,"");
					return -1;
				}
				fat16 = (unsigned short *)(fat12 + dpb.bpf);
			}
			/*
			 *	for WR mode, just mark all bad clusters as bad
			 */
			for (b1 = firstbad; b1; b1 = b1->next_bad)
				fat16[b1->badclus] = dpb.badmask;

			if (dpb.isfat16) {						/* fat16 */
				swab(fat16,fat16,dpb.bpf);
				fat12 = (char *)fat16;
			} else {								/* fat12 */
				fat16_to_fat12(fat16,fat12);
			}
			fat16 = NULL;
			must_write(dev,dpb.spf,1,fat12,FAT);
			if (dpb.nfat == 2)
				must_write(dev,dpb.spf,1+dpb.spf,fat12,FAT);

			/*
			 *	since (in theory at least) the root directory could be larger
			 *	than a FAT, we can't write it all at once from the FAT buffer,
			 *	so we'll write it one sector at a time ...
			 */
			memset(fat12,0x00,dpb.bps);
			for (i = 0, n = 1+dpb.nfat*dpb.spf; i < dpb.rds; i++, n++)
				must_write(dev,1,n,fat12,ROOTDIR);

			status.infodev = -1;
			status.mapdev = -1;
			mark_chart(dev);
			fix = 1;								/* so we display correct msgs */
		}
	} else if (fix && firstbad) {
		free_pointers();
		if (dpb.isfat16) {						/* fat16 */
			if ((fat16=malloc(dpb.bpf)) == NULL) {
				error(dpb.bpf,MEMALLOC,"");
				return -1;
			}
			if (log_read(dev,dpb.spf,1,fat16,FAT))
				return -1;
			swab((char *)fat16,(char *)fat16,dpb.bpf);
		} else {								/* fat12 */
			if ((fat12 = malloc(dpb.bpf * 6L)) == NULL) {
				error(dpb.bpf*6,MEMALLOC,"");
				return -1;
			}
			fat16 = (unsigned short *)(fat12 + dpb.bpf * 2L);
			if (log_read(dev,dpb.spf,1,fat12,FAT))
				return -1;
			fat12_to_fat16(fat12,fat16);
		}

		dirlevel = 0;
		memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));

		/*
		 *	check FAT for each bad cluster
		 *		if bad cluster is unassigned, mark it bad
		 *		if it's already marked bad, leave it alone
		 *		otherwise count in-use bad clusters
		 */
		for (b1 = firstbad, inuse = 0; b1; b1 = b1->next_bad) {
			if (fat16[b1->badclus] == 0)
				fat16[b1->badclus] = dpb.badmask;
			else if (fat16[b1->badclus] != dpb.badmask)
				inuse++;
		}
		if (inuse)
			bad_loop(0,NULL,0);
		if (dpb.isfat16) {						/* fat16 */
			swab(fat16,fat16,dpb.bpf);
			fat12 = (char *)fat16;
		} else {								/* fat12 */
			fat16_to_fat12(fat16,fat12);
		}
		fat16 = NULL;
		must_write(dev,dpb.spf,1,fat12,FAT);
		must_write(dev,dpb.spf,1+dpb.spf,fat12,FAT);
	}

	medic[MRAREA].ob_flags |= HIDETREE;
	medic[MSAREA].ob_flags |= HIDETREE;
	medic[MBAREA].ob_flags &= ~HIDETREE;		/* display physical disk report */

	/*
	 *	first we display any error sectors in the FAT/root area
	 */
	obj = 0;
	if (firstfatbad) {
		display_bad(firstfatbad);
		strcpy(medic[BADHEAD1].ob_spec,text[MEDIC_BADS_HEAD1].ob_spec);
		strcpy(medic[BADHEAD2].ob_spec,text[MEDIC_BADS_HEAD2].ob_spec);
		strcpy(medic[BADMSG].ob_spec,text[MEDIC_BADS_ERROR].ob_spec);
		strcpy(medic[BADFIXN].ob_spec,text[MEDIC_CONTINUE].ob_spec);
		obj = wait_bad();
	}
	if (obj == BADCAN)
		return 0;

	/*
	 *	then we display any clusters in error
	 */
	strcpy(medic[BADHEAD1].ob_spec,text[MEDIC_BC_HEAD1].ob_spec);
	strcpy(medic[BADHEAD2].ob_spec,text[MEDIC_BC_HEAD2].ob_spec);
	if (firstbad) {
		display_bad(firstbad);
		if (!firstfatbad) {
			if (fix) {
				strcpy(medic[BADMSG].ob_spec,text[MEDIC_BCFIX_DONE].ob_spec);
				strcpy(medic[BADFIXN].ob_spec,text[MEDIC_CONTINUE].ob_spec);
			} else {
				strcpy(medic[BADMSG].ob_spec,text[MEDIC_NEED_BCFIX].ob_spec);
				strcpy(medic[BADFIXN].ob_spec,text[MEDIC_FIXNOW].ob_spec);
			}
		}
	} else {
		if (!firstfatbad) {
			strcpy(medic[BADMSG].ob_spec,text[MEDIC_NOPROBLEMS].ob_spec);
			strcpy(medic[BADFIXN].ob_spec,text[MEDIC_CONTINUE].ob_spec);
			medic[BADCAN].ob_state |= DISABLED;
		}
	}
	obj = wait_bad();

	if (firstbad && !firstfatbad && !fix) {
		if (obj == BADFIXN) {
#ifdef DEMO
			form_alert(1,DEMO_UNAVAIL);
#else
			fix = 1;
			return 1;
#endif
		}
	}

	return 0;
}

/*
 *	copy bad sectors/clusters to display panel
 */
PRIVATE void display_bad(BAD *curbad)
{
int i, n;
char str[32], *p;

	for (i = 0; i < MAPBADLIST_ROWS; i++) {
		str[0] = '\0';
		for (n = 0, p = str; (n < MAPBADLIST_COLS) && curbad; curbad = curbad->next_bad, n++) {
			if (n)
				*p++ = ' ';
			p += sprintf(p,"%5d",curbad->badclus);
		}
		strcpy(medic[BADS1+i].ob_spec,str);
	}
}

/*
 *	wait for user input after displaying bad clusters/sectors
 */
PRIVATE int wait_bad(void)
{
short n;
int obj;

	redraw_object(medic,MBAREA);
	mouse_on();
	ring_bell();
	ring_bell();
	n = 0;
	obj = myform_do(medic,&n,0) & 0x7fff;
	medic[obj].ob_state &= ~SELECTED;
	mouse_off();

	return obj;
}

PRIVATE void free_pointers(void)
{
	if (fat16) {
		free(fat16);
		fat16 = NULL;
	}
	if (fat12) {
		free(fat12);
		fat12 = NULL;
	}
}

/*****************************************************************************/
/*
 *	get preferences for 'map bad' action
 */
PUBLIC int bad_prefs()
{
TEDINFO *nbad;

	myform_center(badpref);
	nbad = (TEDINFO *) badpref[MBPASSES].ob_spec;

	do {
		sprintf(nbad->te_ptext,"%2d",1);
		if (handle_dial(badpref,MBPASSES,0,0) != MBOK)
			return -1;
		if (badpref[MBRR].ob_state & SELECTED)
			mapbad.type = RR;
		else if (badpref[MBRWR].ob_state & SELECTED)
			mapbad.type = RWR;
		else mapbad.type = WR;
		mapbad.passes = atoi(nbad->te_ptext);
		if (!mapbad.passes)
			xform_alert(1,MD_PASSES_ALERT);
	} while(!mapbad.passes);

	sprintf(medic[NPASS].ob_spec,"%2d",1);
	sprintf(medic[TOTPASS].ob_spec,"%2d",mapbad.passes);

	return 0;
}

/*****************************************************************************/
PRIVATE int bad_loop(short dstart,char *enddir,short mustwrite)
{
short st, prev;
short dirstart;
int numclus, ncl, rc;
DIRENTRY *savecfile;
short i;
long offset, size;
short needtowrite = 0;
char *endpoint;
char *end;
char *temp;

	i = 0;
	if (dstart == 0) {
		if ((enddir=load_root(&i,NULL)) == NULL)	/* rfb: we should get a path somehow ... */
			return -1;
	}
	if (i)
		mustwrite = 1;

	curfile = (DIRENTRY *)dirlist[dirlevel].dirbuf;

	if (dirlevel == 0) {
		if (curfile->attr == FA_VOLUME)
			curfile++;
	} else curfile += 2;

	/*
	 *	process subdirectories (they have been sorted in-memory to start of dir)
	 */
	for ( ; curfile->attr & FA_SUBDIR; curfile++) {
		if (curfile->name[0] == DELETION_MARK)
			continue;
		/*
		 *	check if the subdirectory's starting cluster is bad.  if so, mark it
		 *	bad in the FAT, update the subdir entry to point to the next cluster
		 *	in the FAT chain, and check again.
		 *	if the entire chain is bad, mark the subdirectory to be deleted.
		 */
		for (st = dirstart = get_word((char *)&curfile->start); is_bad_cluster(st); st = dirstart) {
			if (fat16[st] == dpb.bitmask) {
				fat16[st] = dpb.badmask;
				curfile->name[0] = DELETION_MARK;
				mustwrite = 1;
				break;
			}
			dirstart = fat16[st];
			fat16[st] = dpb.badmask;
			putword(&curfile->start,dirstart);
			mustwrite = 1;
		}
		if (curfile->name[0] == DELETION_MARK)	/* must have been set above */
			continue;

		/*
		 *	count the clusters remaining in this subdirectory
		 */
		for (ncl = 1; fat16[st] != dpb.bitmask; ncl++) {
			prev = st;
			st = fat16[st];
			if (!valid_cluster(st)) {
				fat16[prev] = dpb.bitmask;
				break;
			}
		}
		dirlevel++;
		if (down_dirlist(ncl) < 0)
			return -1;

		while((rc=load_directory_bad(&dirstart,&ncl,&mustwrite)) > 0)
			;

		if (rc < 0)
			continue;

		endpoint = sort_dir(dirlist[dirlevel].dirbuf,ncl,0);	/* don't remove deleted entries */
		end = dirlist[dirlevel].dirbuf + dpb.bpc * (long) ncl;
		for (temp = endpoint; temp < end; temp += sizeof(DIRENTRY))
			memset(temp,0x00,sizeof(DIRENTRY));

		val_bad((DIRENTRY *)dirlist[dirlevel].dirbuf,endpoint,&needtowrite);

		savecfile = curfile;

		if (bad_loop(dirstart,endpoint,needtowrite) < 0) {
			up_dirlist();
			return -1;
		}
		curfile = savecfile;
	}

	/*
	 *	process remaining entries in directory (must be files)
	 */
	for ( ; (void *)curfile < (void *)enddir; curfile++) {
		if (curfile->name[0] == DELETION_MARK)
			continue;
		size = get_long((char *)&curfile->size);
		numclus = (size+dpb.bpc-1) >> dpb.cluster_shift;
		if (size < 0)
			numclus = 0;
		i = check_bad(curfile,numclus,size);
		if (i)
			mustwrite = 1;
		if (i < 0)
			curfile->name[0] = DELETION_MARK;
	}

	if (mustwrite) {
		if (dirlevel > 0) {
			st = dstart;
			offset = 0;

			do {
				must_write(dev,dpb.spc,logsector(st),dirlist[dirlevel].dirbuf+offset,DIR);
				st = fat16[st];
				offset += dpb.bpc;
			} while (st != dpb.bitmask);
		} else {
			must_write(dev,dpb.rds,1+2*dpb.spf,dirlist[0].dirbuf,DIR);
		}
	}
	up_dirlist();

	return 0;
}

/*
 *	load directory into memory for 'map bad', handling unreadable sectors
 *
 *	if any directory sector is unreadable, we drop the corresponding cluster,
 *	fixing up the FAT chain and the parent directory entry as required
 *
 *	return code:
 *		1	first cluster in the chain is unreadable; parent
 *			directory entry has been updated to point to the next
 *		0	the directory has been loaded; at least one readable
 *			directory cluster exists
 *		-1	nothing loaded: all of the directory clusters were bad
 */
PRIVATE int load_directory_bad(short *dirstart,int *ncl,short *mustwrite)
{
short st, prev;
long offset;

	offset = 0L;
	st = prev = *dirstart;	/* we only initialise prev to prevent erroneous 'warning 94' messages */
	do {
		if (log_read(dev,dpb.spc,logsector(st),dirlist[dirlevel].dirbuf+offset,DIR)) {
			if (*ncl == 1) {					/* bad first cluster */
				*mustwrite = 1;
				fat16[st] = dpb.badmask;
				curfile->name[0] = DELETION_MARK;
				up_dirlist();
				return -1;
			} else {
				*ncl--;
				if (st == *dirstart) {
					*mustwrite = 1;
					*dirstart = fat16[st];
					fat16[st] = dpb.badmask;
					putword(&curfile->start,*dirstart);
					assert(offset==0L);		/* temporary paranoia */
					return 1;
				} else {
					fat16[prev] = fat16[st];
					st = fat16[st];
					fat16[st] = dpb.badmask;
					continue;
				}
			}
		}
		prev = st;
		st = fat16[st];
		if (is_bad_cluster(st)) {
			*ncl--;
			*mustwrite = 1;
			if (fat16[st] == dpb.bitmask) {		/* bad last cluster */
				fat16[st] = dpb.badmask;
				fat16[prev] = dpb.bitmask;
				break;
			} else {							/* bad middle cluster */
				fat16[prev] = fat16[st];
				fat16[st] = dpb.badmask;
				st = fat16[prev];
			}
		}
		offset += (long) dpb.bpc;
	} while (st != dpb.bitmask);

	return 0;
}

/*
 *	allocate a structure to contain the number of a bad data cluster
 */
PRIVATE int alloc_bad()
{
BAD *temp;

	temp = calloc(1,sizeof(BAD));
	if (!temp) {
		error(sizeof(BAD),MEMALLOC,"");
		return -1;
	}

	if (firstbad)
		curbad->next_bad = temp;
	else firstbad = temp;

	curbad = temp;
	return 0;
}

/*
 *	free all the bad data cluster structures
 */
PRIVATE void free_all_bad()
{
BAD *n, *m;

	for (n = firstbad; n; n = m) {
		m = n->next_bad;
		free(n);
	}
	firstbad = curbad = NULL;
}

/*
 *	search bad-cluster chain
 *
 *	returns TRUE if cluster found in chain, else FALSE
 */
PRIVATE int is_bad_cluster(short cluster)
{
BAD *bad;

	for (bad = firstbad; bad; bad = bad->next_bad)
		if (bad->badclus == cluster)
			return TRUE;

	return FALSE;
}

/*
 *	allocate a structure to contain the number of a bad logical sector
 *	in the FAT/root area
 */
PRIVATE int alloc_fatbad()
{
BAD *temp;

	temp = calloc(1,sizeof(BAD));
	if (!temp) {
		error(sizeof(BAD),MEMALLOC,"");
		return -1;
	}

	if (firstfatbad)
		curfatbad->next_bad = temp;
	else firstfatbad = temp;

	curfatbad = temp;
	return 0;
}

/*
 *	free all the bad logical sector structures
 */
PRIVATE void free_all_fatbad()
{
BAD *n, *m;

	for (n = firstfatbad; n; n = m) {
		m = n->next_bad;
		free(n);
	}
	firstfatbad = curfatbad = NULL;
}

/*********************************************************************/
PRIVATE int do_rr(int passes,char *buf,long bufsize)
{
char *ptr1, *ptr2;
int i, j;
short clus, maxclus, numclus, numsecs;
unsigned short secloc;
long err;

	ptr1 = buf;
	ptr2 = ptr1 + bufsize;
	maxclus = bufsize / dpb.bpc;

	for (i = 0; i < passes; i++) {
		sprintf(medic[NPASS].ob_spec,"%2d",i+1);
		redraw_object(medic,NPASS);
		redraw_object(medic,MDAREA);
		draw_block(0,CLUSTER_OK);
		draw_block(1,CLUSTER_OK);

		for (clus = 2; clus < dpb.maxc; clus += numclus) {
			numclus = min(dpb.maxc-clus,maxclus);
			numsecs = numclus * dpb.spc;
			secloc = logsector(clus);
			if (get_key() < 0)
				return -1;
			if ((err=read_no_retry(dev,numsecs,secloc,ptr1)) < 0L) {
				if (rr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			if (get_key() < 0)
				return -1;
			if ((err=read_no_retry(dev,numsecs,secloc,ptr2)) < 0L) {
				if (rr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			if (compare_memory(ptr1,ptr2,numsecs*dpb.bps)) {
				if (rr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			for (j = 0; j < numclus; j++)
				draw_block(clus+j,CLUSTER_OK);
		}
	}

	return 0;
}

/****************************************************************************/
PRIVATE int rr_bysector(char *ptr1,char *ptr2,short nread,short clus)
{
short nsec, curclus;
unsigned short secloc;
short nerr;

	secloc = logsector(clus);
	for (nsec = 0; nsec < nread; nsec += dpb.spc) {
		nerr = 0;
		if (get_key() < 0)
			return -1;
		if (read_no_retry(dev,dpb.spc,secloc+nsec,ptr1) < 0)
			nerr++;
		if (read_no_retry(dev,dpb.spc,secloc+nsec,ptr2) < 0)
			nerr++;
		if (compare_memory(ptr1,ptr2,dpb.bpc))
			nerr++;
		curclus = clus + (nsec / dpb.spc);

		if (nerr == 0) {
			draw_block(curclus,CLUSTER_OK);
			continue;
		}
		if (is_bad_cluster(curclus))		/* we found this before */
			continue;

		if (alloc_bad() < 0)				/* got a new bad cluster */
			return -1;						/* but can't remember it */
		curbad->badclus = curclus;
		draw_block(curclus,CLUSTER_ERR);
	}

	return 0;
}

/*********************************************************************/
PRIVATE int do_rwr(int passes,char *buf,long bufsize)
{
char *ptr1, *ptr2;
int i, j;
short clus, maxclus, numclus, numsecs;
unsigned short secloc;
long err;

	ptr1 = buf;
	ptr2 = ptr1 + bufsize;
	maxclus = bufsize / dpb.bpc;

	for (i = 0; i < passes; i++) {
		sprintf(medic[NPASS].ob_spec,"%2d",i+1);
		redraw_object(medic,NPASS);
		redraw_object(medic,MDAREA);
		draw_block(0,CLUSTER_OK);
		draw_block(1,CLUSTER_OK);

		for (clus = 2; clus < dpb.maxc; clus += numclus) {
			numclus = min(dpb.maxc-clus,maxclus);
			numsecs = numclus * dpb.spc;
			secloc = logsector(clus);
			if (get_key() < 0)
				return -1;
			if (read_no_retry(dev,numsecs,secloc,ptr1) < 0) {
				if (rwr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			if (get_key() < 0)
				return -1;
			if ((err=write_no_retry(dev,numsecs,secloc,ptr1)) < 0) {
				if (err == -EWRPRO) {			/* write protect? */
					xform_alert(1,MD_WRTPROT_ALERT);
					return -1;
				}
				if (rwr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			if (get_key() < 0)
				return -1;
			if (read_no_retry(dev,numsecs,secloc,ptr2) < 0) {
				if (rwr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			if (compare_memory(ptr1,ptr2,numsecs*dpb.bps)) {
				if (rwr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			for (j = 0; j < numclus; j++)
				draw_block(clus+j,CLUSTER_OK);
		}
	}

	return 0;
}

/****************************************************************************/
PRIVATE int rwr_bysector(char *ptr1,char *ptr2,short nread,short clus)
{
short nsec, curclus;
unsigned short secloc;
short nerr;

	secloc = logsector(clus);
	for (nsec = 0; nsec < nread; nsec += dpb.spc) {
		nerr = 0;
		if (get_key() < 0)
			return -1;
		if (read_no_retry(dev,dpb.spc,secloc+nsec,ptr1) < 0)
			nerr++;
		if (write_no_retry(dev,dpb.spc,secloc+nsec,ptr1) < 0)
			nerr++;
		if (read_no_retry(dev,dpb.spc,secloc+nsec,ptr2) < 0)
			nerr++;
		if (compare_memory(ptr1,ptr2,dpb.bpc))
			nerr++;
		curclus = clus + (nsec / dpb.spc);

		if (nerr == 0) {
			draw_block(curclus,CLUSTER_OK);
			continue;
		}
		if (is_bad_cluster(curclus))		/* we found this before */
			continue;

		if (alloc_bad() < 0)				/* got a new bad cluster */
			return -1;						/* but can't remember it */
		curbad->badclus = curclus;
		draw_block(curclus,CLUSTER_ERR);
	}

	return 0;
}

/*********************************************************************/
PRIVATE int do_wr(int passes,char *buf,long bufsize)
{
char *ptr1, *ptr2;
long err;
int i, j;
short clus, maxclus, numclus, numsecs;
unsigned short secloc;
short maxsecs, nondatasecs;

	ptr1 = buf;
	ptr2 = ptr1 + bufsize;
	maxclus = bufsize / dpb.bpc;

	maxsecs = maxclus * dpb.spc;
	nondatasecs = dpb.nfat * dpb.spf + dpb.rds;

	for (i = 0; i < passes; i++) {
		sprintf(medic[NPASS].ob_spec,"%2d",i+1);
		redraw_object(medic,NPASS);
		redraw_object(medic,MDAREA);
		for (secloc = 1; secloc < nondatasecs; secloc += numsecs) {
			numsecs = min(nondatasecs-secloc,maxsecs);
			if (get_key() < 0)
				return -1;
			if ((err=write_no_retry(dev,numsecs,secloc,ptr1)) < 0) {
				if (err == -EWRPRO) {			/* write protect? */
					xform_alert(1,MD_WRTPROT_ALERT);
					return -1;
				}
				if (wr_fat(ptr1,ptr2,numsecs,secloc) < 0)
					return -1;
				continue;
			}
			if (get_key() < 0)
				return -1;
			if (read_no_retry(dev,numsecs,secloc,ptr2) < 0) {
				if (wr_fat(ptr1,ptr2,numsecs,secloc) < 0)
					return -1;
				continue;
			}
			if (compare_memory(ptr1,ptr2,numsecs*dpb.bps)) {
				if (wr_fat(ptr1,ptr2,numsecs,secloc) < 0)
					return -1;
				continue;
			}
		}

		if (firstfatbad) {
			draw_block(0,CLUSTER_ERR);
			draw_block(1,CLUSTER_ERR);
		} else {
			draw_block(0,CLUSTER_OK);
			draw_block(1,CLUSTER_OK);
		}

		for (clus = 2; clus < dpb.maxc; clus += numclus) {
			numclus = min(dpb.maxc-clus,maxclus);
			numsecs = numclus * dpb.spc;
			secloc = logsector(clus);
			if (write_no_retry(dev,numsecs,secloc,ptr1) < 0) {
				if (wr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			if (read_no_retry(dev,numsecs,secloc,ptr2) < 0) {
				if (wr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			if (compare_memory(ptr1,ptr2,numsecs*dpb.bps)) {
				if (wr_bysector(ptr1,ptr2,numsecs,clus) < 0)
					return -1;
				continue;
			}
			for (j = 0; j < numclus; j++)
				draw_block(clus+j,CLUSTER_OK);
		}
	}

	return 0;
}

/****************************************************************************/
PRIVATE int wr_bysector(char *ptr1,char *ptr2,short nread,short clus)
{
short nsec, curclus;
unsigned short secloc;
short nerr;

	secloc = logsector(clus);
	for (nsec = 0; nsec < nread; nsec += dpb.spc) {
		nerr = 0;
		if (get_key() < 0)
			return -1;
		if (write_no_retry(dev,dpb.spc,secloc+nsec,ptr1) < 0)
			nerr++;
		if (read_no_retry(dev,dpb.spc,secloc+nsec,ptr2) < 0)
			nerr++;
		if (compare_memory(ptr1,ptr2,dpb.bpc))
			nerr++;
		curclus = clus + (nsec / dpb.spc);
		if (nerr == 0) {
			draw_block(curclus,CLUSTER_OK);
			continue;
		}

		if (is_bad_cluster(curclus))		/* we found this before */
			continue;

		if (alloc_bad() < 0)
			return -1;
		curbad->badclus = curclus;
		draw_block(curclus,CLUSTER_ERR);
	}

	return 0;
}

/****************************************************************************/
PRIVATE int wr_fat(char *ptr1,char *ptr2,short nread,unsigned short secloc)
{
short nsec, cursec;
short nerr;
BAD *b, *n;

	for (nsec = 0; nsec < nread; nsec++) {
		nerr = 0;
		if (get_key() < 0)
			return -1;
		if (write_no_retry(dev,dpb.spc,secloc+nsec,ptr1) < 0)
			nerr++;
		if (read_no_retry(dev,dpb.spc,secloc+nsec,ptr2) < 0)
			nerr++;
		if (compare_memory(ptr1,ptr2,dpb.bpc))
			nerr++;
		cursec = secloc + nsec;
		if (nerr == 0)
			continue;

		for (b = firstfatbad; b; b = n) {
			n = b->next_bad;
			if (b->badclus == cursec)		/* we found this before */
				break;
		}
		if (b)
			continue;

		if (alloc_fatbad() < 0)
			return -1;
		curfatbad->badclus = cursec;
	}

	return 0;
}

/**********************************************************************/
PRIVATE int val_bad(DIRENTRY *dirstart,char *end,short *needtowrite)
{
DIRENTRY *dir;
char *temp;
int i, ngarbage;
short start;
long size;

	for (dir = dirstart; (void *) dir < (void *) end; dir++) {
		temp = dir->name;
		start = get_word((char *)&dir->start);
		size = get_long((char *)&dir->size);
		if ((start == 0) && (size == 0L))
			;
		else if (!valid_cluster(start)) {
			*temp = DELETION_MARK;
			*needtowrite = 1;
			continue;
		}
		if (dir->attr & BADATTR_MASK) {
			dir->attr = FA_ARCHIVE;
			*needtowrite = 1;
		}
		for (i = 0, ngarbage = 0; i < NAME_LENGTH; i++) {
			if (!ascii[*(temp + i)]) {
				*(temp + i) = REPLACEMENT_CHAR;
				*needtowrite = 1;
				ngarbage++;
			}
		}
		if (ngarbage > 4) {						//MAGNUM
			*temp = DELETION_MARK;
			*needtowrite = 1;
		}
	}

	return 0;
}

/*
 *	return 	0	OK
 *			1	write entry (size/start has changed)
 *			-1	mark entry as deleted & write it
 */
PRIVATE int check_bad(DIRENTRY *file,short nclus,long size)
{
short st, prev;
short start;
int clusters;
int needtowrite = 0;

	start = get_word((char *)&file->start);
	if (start == 0)
		return 0;

	/*
	 *	check if the file's starting cluster is bad.  if so, mark it bad
	 *	in the FAT, update the file entry to point to the next cluster
	 *	in the FAT chain, and check again.
	 *	if the entire chain is bad, return a code to mark the file to be deleted.
	 */
	for (st = start; is_bad_cluster(st); st = start) {
		if (fat16[st] == dpb.bitmask) {
			fat16[st] = dpb.badmask;
			return -1;
		}
		start = fat16[st];
		fat16[st] = dpb.badmask;
		putword(&file->start,start);
		size -= dpb.bpc;
		putlong(&file->size,size);
		needtowrite = 1;
	}

	/*
	 *	now process the rest of the FAT chain
	 */
	for (clusters = 0; TRUE; ) {
		prev = st;
		st = fat16[st];

		if (is_bad_cluster(st)) {			/* bad cluster: */
			if (fat16[st] == dpb.bitmask) {		/* if it was the end of chain, */
				fat16[st] = dpb.badmask;		/* mark it as bad now          */
				fat16[prev] = dpb.bitmask;		/* and mark the previous cluster as end */
				needtowrite = 1;
				break;
			}
			fat16[prev] = fat16[st];			/* otherwise point the previous to the next, */
			fat16[st] = dpb.badmask;			/* and mark the current as bad */
			st = fat16[prev];					/* reset the next */
			size -= dpb.bpc;					/* adjust the file size (we know how much we've lost) */
			putlong(&file->size,size);
			needtowrite = 1;
		} else clusters++;					/* good cluster: count it */

		if (st == dpb.bitmask)				/* end of chain */
			break;

		if (!valid_cluster(st)) {			/* invalid cluster: */
			fat16[prev] = dpb.bitmask;			/* mark previous as end of chain */
			clusters--;							/* adjust the cluster count */
			needtowrite = 1;
			break;
		}
	}

	if (clusters != nclus) {
		putlong(&file->size,((long)clusters)<<dpb.cluster_shift);
		needtowrite = 1;
	}

	return needtowrite;
}

/*
 *	write message to error file, opening it if necessary
 *
 *	handles variable number of arguments
 */
PRIVATE void write_errfile(char *fmt, ...)
{
va_list args;

	if (!open_errfile())
		return;

	va_start(args,fmt);
	vfprintf(efile,fmt,args);
	va_end(args);
}

PRIVATE int open_errfile()
{
	if (efile)
		return TRUE;

	sprintf(medic_fname,"%s" MEDIC_FILE,opt.filepath,'A'+dev);
	if ((efile=fopen(medic_fname,"wa")) == NULL) {
		error(-1L,OPEN,medic_fname);
		return FALSE;
	}

	return TRUE;
}

/**************************************************************************/
/*
 *	wait specified time for mouse click
 *
 */
PRIVATE int multi_pause(int numsec)
{
short event;
short mx, my, dummy;

	numsec *= 1000;		/* convert to millisecs */
	event = evnt_multi(MU_KEYBD | MU_TIMER | MU_BUTTON,
					   NUM_CLICKS, LEFT_BUTTON, BUTTON_DOWN,
					   0, 0, 0, 0, 0,
					   0, 0, 0, 0, 0,
					   NULL, numsec&0xffff, numsec>>16,
					   &mx, &my, &dummy, &dummy, &dummy, &dummy);

	if (event & MU_BUTTON)
		return objc_find(medic,0,MAX_DEPTH,mx,my);

	return -1;
}

