/*
 *	dmirror.c: Diamond Mirror
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 *
 *	Modified for Obsidian Edge (November 2026): BETA version and expiry date removed
 */

/*
 *	version 2.50	roger burrows (january/2003)
 *		rewritten, based on
 *			. revised edge code (as of 14jan2003)
 *			. new-style control file
 *
 *	version 2.57	roger burrows (january/2017)
 *		prepare for public release
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <ctype.h>
#include <dos.h>
#include <portab.h>
#include <string.h>
#include <unistd.h>
#include <vdi.h>

#include "../lib/extendos.h"
#include "../lib/misc.h"
#include "../edgedefs.h"
#include "../section.h"

/*
 *	stuff (currently) exclusive to dmirror
 */
#define PROGRAM_VERSION	"2.57"

#define ABORT_KEY		0x03				/* control-C */

#define MIRROR_BACK		"MIRROR_%c.BAK"		/* template for backup mirror file */

#define DOS_ERROR		1		/* codes for errmsg() function */
#define BIOS_ERROR		2
#define OTHER_ERROR		3

#define MAX_RETRY		1


/*
 *	stuff borrowed from medic1.c
 */
/*
 *	the following filename validation table allows 'illegal' filenames
 *	(equivalent to opt.ignore == TRUE in medic1.c)
 */
char ascii2[256] = {	/* filename validation table, allows 'illegal' filernames */
			   /* 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
	/*   0- 15 */ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/*  16- 31 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/*  32- 47 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
	/*  48- 64 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
	/*  64- 79 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/*  80- 95 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
	/*  96-111 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 112-127 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 128-143 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 144-159 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 160-175 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 176-191 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 192-207 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 208-223 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 224-239 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 241-255 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

#define FATERR		0	/* note that these are (currently) different from edge ... */
#define BADDIR		1
#define READERR 	2
#define BADFNAME	3
#define BADENTRY	4
#define BADSIZE 	5
#define BADFAT		6
#define COLLISION	7
#define LOSTCLUSTER	8
#define MAXERRTYPE	LOSTCLUSTER
#define NERRTYPE	(MAXERRTYPE+1)
short errcnt[NERRTYPE];


/*
 *	other global variables
 */
extern int _XMODE;
int bailout = FALSE;	/* TRUE => user wants to quit */
short dev;				/* device currently being worked on */
long today;				/* today's date in days since 1980/01/01 */
long drivemap;			/* bitmap of valid drives (excluding CDs) */
char app_path[MAX_PATHNAME];
short *fat16, *tfat;
MYBPB dpb;

short dirlevel;
DIRLIST dirlist[MAX_DIR_DEPTH];


struct control {
	unsigned long partitions;	/* bitmap of partitions */
	int frequency;				/* in days between actions */
	char lastpart;				/* last partition processed */
	long lastdate;				/* date last run (in days since 1980/01/01) */
};
struct control test;
int test_detail = FALSE;		/* TRUE => detailed error report */
int test_rotate = FALSE;		/* TRUE => rotate tests */
struct control mirror;
char mirror_path[MAX_EDGEPATH] = "";	/* path for mirror files */

char *copyright = "Diamond Mirror: Copyright ½ 2002-2017 Roger Burrows.  All Rights Reserved";


/*
 *	function prototypes
 */
unsigned long CDmap(void);
int calcdays(unsigned short date);
int check_abort(void);
int check_fat(DIRENTRY *file);
int check_garbage(void);
int compare_fats(char *fat1,char *fat2);
void do_block(short);
int do_tdisk(void);
void errmsg(int type,int errnum,char *msg1,char *msg2);
void fat12_to_fat16(unsigned char *f12,unsigned short *f16);
int get_key(void);
long get_long(char *);
int get_mybpb(MYBPB *,short);
int get_name(DIRENTRY *from,char *fname);
short get_sys(short *p);
unsigned short get_word(char *);
int load_directory(short *dirstart);
int load_mybpb(char *,MYBPB *,BPB *);
char *load_root(void);
unsigned short logsector(short int cluster);
int perform_mirrors(unsigned long mirrorlist);
int perform_tests(unsigned long testlist);
short open_workstation(void);
int save_info(short dev,char *path);
char *sort_dir(char *curdir,int ncl);
char *sort_root(char *);
int test_loop(short dstart,char *enddir);
void up_dirlist(void);
void update_last_run(void);
int val_direntries(DIRENTRY *dirstart,char *end);


/*
 *	private macros
 */
#define sup_on()	{ void *savessp = Super(0L);
#define sup_off()	Super(savessp); }


/*
 *	main line code
 */
int main(int argc,char **argv)
{
long next;
int n;
short boot, myhandle;
short mdo = FALSE, tdo = FALSE;
unsigned long testlist, mirrorlist;
char *p, temp[200];

#ifdef _DEBUG
	_XMODE = 3;
#endif
	if (_XMODE != 3) {		/* if not run from AUTO folder ... */
		appl_init();
		myhandle = open_workstation();
		graf_mouse(ARROW,NULL);
		form_alert(1,"[1][Diamond Mirror must be run |from the AUTO folder during |bootup. |(Use Diamond Edge to |configure Diamond Mirror)][ OK ]");
		v_clsvwk(myhandle);
		appl_exit();
		exit(0);
	}

	today = calcdays(Tgetdate());			/* calc now, because CDmap() expiry date test uses it */

	drivemap = Drvmap() & ~CDmap();			/* eliminate CD-ROM devices from map  */
											/* to prevent possible error messages */
	boot = get_sys(_bootdev);
	sprintf(app_path,DM_PATH,'A'+boot);

	printf("\r\x1bp                                    \n");	/* inverse video */
	printf("        Diamond Mirror v%-6.6s      \n",PROGRAM_VERSION);
	printf("    ½ 1992-2003 by Roger Burrows    \n");
	printf(" based on code by Robert D. Luneski \n");
	printf("                                    \x1bq\n");		/* normal video */
	printf("\n");

	/*
	 *	read INF file, set up test, mirror
	 */
	if (open_section(DM_PREFS_FILE,"ra",TEST_SECTION) == 0) {
		temp[0] = '\0';
		read_section_line(INF_PARTITIONS," %[ -ÿ]",temp);
		for (p = temp; *p; p++) {
			n = *p - 'A';
			if ((n >= FIRST_HD_DEVICE) && (n < NUM_DEVICES))
				test.partitions |= (1L << n);
		}
		read_section_line(INF_FREQUENCY," %d",&test.frequency);
		read_section_line(INF_DETAIL," %d",&test_detail);
		read_section_line(INF_ROTATE," %d",&test_rotate);
		close_section();
	}
	if (open_section(DM_PREFS_FILE,"ra",MIRROR_SECTION) == 0) {
		temp[0] = '\0';
		read_section_line(INF_PARTITIONS," %[ -ÿ]",temp);
		for (p = temp; *p; p++) {
			n = *p - 'A';
			if ((n >= FIRST_HD_DEVICE) && (n < NUM_DEVICES))
				mirror.partitions |= (1L << n);
		}
		read_section_line(INF_FREQUENCY," %d",&mirror.frequency);
		read_section_line(INF_PATH," %[ -ÿ]",mirror_path);
		close_section();
	}
	if (open_section(DM_PREFS_FILE,"ra",LASTRUN_SECTION) == 0) {
		read_section_line(INF_TESTRUN," %c %ld",&test.lastpart,&test.lastdate);
		n = test.lastpart - 'A';
		if ((n < FIRST_HD_DEVICE) || (n >= NUM_DEVICES))
			test.lastpart = 'A' + NUM_DEVICES - 1;	/* so we start with the first partition this time */
		if (test.lastdate > today)
			test.lastdate = today;
		read_section_line(INF_MIRRORRUN," %c %ld",&mirror.lastpart,&mirror.lastdate);
		n = mirror.lastpart - 'A';
		if ((n < FIRST_HD_DEVICE) || (n >= NUM_DEVICES))
			mirror.lastpart = 'A' + NUM_DEVICES - 1;/* so we start with the first partition this time */
		if (mirror.lastdate > today)
			mirror.lastdate = today;
		close_section();
	}

	/*
	 *	determine which test(s) and mirror(s) to execute;
	 *	issue messages if none
	 */
	next = test.lastdate + test.frequency;
	testlist = test.partitions & drivemap;
	if (!testlist)
		printf("No disk tests scheduled\n");
	else if (next > today)
		printf("%d day(s) until next disk test\n",next-today);
	else tdo = TRUE;

	next = mirror.lastdate + mirror.frequency;
	mirrorlist = mirror.partitions & drivemap;
	if (!mirrorlist)
		printf("No disk mirrors scheduled\n");
	else if (next > today)
		printf("%d day(s) until next disk mirror\n",next-today);
	else mdo = TRUE;

	/*
	 *	perform requested Tests
	 */
	if (tdo) {
		if (perform_tests(testlist) < 0)
			bailout = TRUE;
		update_last_run();
		if (bailout) {
			printf("\nDiamond Mirror terminated\n");
			return 1;
		}
	}

	/*
	 *	perform requested Mirrors
	 */
	if (mdo) {
		if (perform_mirrors(mirrorlist) < 0)
			bailout = TRUE;
		update_last_run();
		if (bailout) {
			printf("\nDiamond Mirror terminated\n");
			return 1;
		}
	}

	printf("\nDiamond Mirror completed normally\n");
	return 0;
}

/*
 *	perform tests according to INF file specifications
 *		returns -1 if we should quit
 */
int perform_tests(unsigned long testlist)
{
unsigned long errorlist = 0UL;
int error = 0, n, rc;
short lastdev;

	if (test_rotate) {				/* pick starting partition */
		for (n = 0, dev = test.lastpart - 'A' + 1; n < 32; n++, dev = (dev+1)%32)
			if (testlist & (1L<<dev))
				break;
		lastdev = dev;				/* and ending */
	} else {
		dev = FIRST_HD_DEVICE;
		lastdev = NUM_DEVICES - 1;
	}
	if (!test_detail)
		printf("Testing partition:");
	for ( ; dev <= lastdev; dev++) {
		if (testlist & (1L<<dev)) {
			if (test_detail)
				printf("Testing partition:");
			printf(" %c",'A'+dev);
			fflush(stdout);
			if ((rc=do_tdisk()) > 0)
				errorlist |= (1L<<dev);
			if (rc >= 0) {					/* only update last run info */
				test.lastpart = 'A' + dev;	/* for successful runs ...   */
				test.lastdate = today;
			} else if (bailout) {
				errmsg(OTHER_ERROR,0,"Operation cancelled by user","");
				error = -1;
				break;
			}
			if (test_detail)
				printf("\n");
		}
	}
	if (!test_detail)
		printf("\n");
	if (errorlist) {
		printf("Disk errors detected on drive(s):");
		for (dev = FIRST_HD_DEVICE; dev < NUM_DEVICES; dev++) {
			if (errorlist & (1L<<dev)) {
				printf(" %c",'A'+dev);
				fflush(stdout);
			}
		}
		printf("\nYou should run Diamond Edge IMMEDIATELY");
		printf("\nPress any key to exit Diamond Mirror");
		get_key();
		error = -1;
	} else printf("No disk errors detected\n");

	return error;
}

/*
 *	perform mirrors according to INF file specifications
 *		returns -1 if we should quit
 */
int perform_mirrors(unsigned long mirrorlist)
{
int error = 0;

	printf("Mirroring Partition:");
	if (!mirror_path[0]) {
		errmsg(OTHER_ERROR,0,"No mirror path specified","");
		return -1;
	}
	for (dev = FIRST_HD_DEVICE; dev < NUM_DEVICES; dev++) {
		if (check_abort()) {
			errmsg(OTHER_ERROR,0,"Operation cancelled by user","");
			return -1;
		}
		if (mirrorlist & (1L<<dev)) {
			printf(" %c",'A'+dev);
			fflush(stdout);
			if (save_info(dev,mirror_path) == 0) {
				mirror.lastpart = 'A' + dev;
				mirror.lastdate = today;	/* update for next time */
			} else error = -1;
		}
	}

	if (error < 0) {
		printf("\nMirror errors were detected");
		printf("\nPress any key to exit Diamond Mirror");
		get_key();
	}

	return error;
}

/*
 *	update the Last run section
 */
void update_last_run(void)
{
	if (open_section(DM_PREFS_FILE,"wa",LASTRUN_SECTION) == 0) {
		write_section_line(INF_TESTRUN,"%c %ld",test.lastpart,test.lastdate);
		write_section_line(INF_MIRRORRUN,"%c %ld",mirror.lastpart,mirror.lastdate);
		close_section();
	}
}

/*
 *	test disk device 'dev'
 *		returns	-1 if bailing out, or malloc problem
 *				else number of errors found
 *
 *	source:	medic1.c
 */
int do_tdisk()
{
int n, problems, rc;
char *fat12;
char *fat1, *fat2;

	if (!get_mybpb(&dpb,dev))
		return 1;

	if (dpb.isfat16) {
		if ((fat16=malloc(dpb.bpf*2)) == NULL) {
			errmsg(OTHER_ERROR,0,"Insufficient memory for","FATs");
			return -1;
		}
		fat1 = (char *)fat16;
		fat2 = fat1 + dpb.bpf;
		tfat = (unsigned short *)fat2;
		rc = compare_fats(fat1,fat2);
		if (rc < 0) {
			free(fat16);
			return -1;
		}
		if (rc > 0)				/* FATS don't match, we'll use FAT#1 for the rest */
			errcnt[FATERR] = 1;

		swab(fat16,fat16,dpb.bpf);
		swab(tfat,tfat,dpb.bpf);
	} else {
		if ((fat12=malloc(dpb.bpf*4)) == NULL) {
			errmsg(OTHER_ERROR,0,"Insufficient memory for","FATs");
			return -1;
		}
		fat1 = fat12;
		fat2 = fat1 + 2 * dpb.bpf;
		rc = compare_fats(fat1,fat2);
		if (rc < 0) {
			free(fat12);
			return -1;
		}
		if (rc > 0)				/* FATS don't match, we'll use FAT#1 for the rest */
			errcnt[FATERR] = 1;

		if ((fat16=malloc(dpb.bpf*4)) == NULL) {
			errmsg(OTHER_ERROR,0,"Insufficient memory for","FATs");
			free(fat12);
			return -1;
		}
		fat12_to_fat16(fat12,fat16);
		free(fat12);
		tfat = fat16 + dpb.bpf;
		memcpy(tfat,fat16,dpb.bpf*2);
	}

	dirlevel = 0;
	memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));
	for (n = 0; n < NERRTYPE; n++)
		errcnt[n] = 0;

	test_loop(0,NULL);
	if (!bailout)			/* if we've bailed out, there WILL be "lost clusters" ... */
		check_garbage();

	free(fat16);

	/*
	 *	don't show details if bailing out, it could be misleading
	 */
	if (bailout)
		return -1;
	
	if (test_detail)
		printf(" (");
	for (n = 0, problems = 0; n < NERRTYPE; n++) {
		problems += errcnt[n];
		if (test_detail)
			printf("%d%c",errcnt[n],(n<NERRTYPE-1)?',':')');
	}
		
	return problems;
}

/*
 *	compare 2 copies of FAT
 *		returns:	-1	can't read FAT(s) (error message has been issued)
 *					0	FATs match
 *					1	FATs don't match
 *
 *	source:	medic1.c
 */
int compare_fats(char *fat1,char *fat2)
{
int i, rc;

	for (i = 0; i <= MAX_RETRY; i++)
		if ((rc=Rwabs(2,fat1,dpb.spf,1,dev)) == 0)
			break;
	if (i > MAX_RETRY) {
		errmsg(BIOS_ERROR,rc,"reading","FAT #1");
		return -1;
	}

	for (i = 0; i <= MAX_RETRY; i++)
		if ((rc=Rwabs(2,fat2,dpb.spf,1+dpb.spf,dev)) == 0)
			break;
	if (i > MAX_RETRY) {
		errmsg(BIOS_ERROR,rc,"reading","FAT #2");
		return -1;
	}

	if (compare_memory(fat1,fat2,dpb.bpf) == 0)			/* all ok */
		return 0;

	return 1;
}

/*
 *	validate directory entries
 *		returns -1 iff error(s), else 0
 *
 *	source:	medic1.c
 */
int val_direntries(DIRENTRY *dirstart,char *end)
{
DIRENTRY *dir, *sdir;
char *temp;
int ngarbage, namerr, duperr;
short start, i;
int badflag = 0;
long fsize;

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

		for (i = 0, temp = dir->name; i < NAME_LENGTH; i++,temp++) {
			if (!ascii2[*temp]) {
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
				ngarbage += 2;
			}
			if ((fsize < 0L) || (fsize > dpb.tbyte)) {
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
					dir->name[0] = DELETION_MARK;
					break;
				}
			}
		}

		if (namerr) {
			errcnt[BADENTRY]++;
			badflag = -1;
		}
		if (duperr) {
			errcnt[BADFNAME]++;
			badflag = -1;
		}
		if (ngarbage > 1) {
			if (!namerr)
				errcnt[BADENTRY]++;
			dir->name[0] = DELETION_MARK;
			badflag = -1;
		}
	}

	return badflag;
}

/*
 *	populate MYBPB from device
 *		if error, issues msg and returns 0
 *		else returns 1
 *
 *	source:	disk.c
 */
int get_mybpb(MYBPB *mybpb,short device)
{
int i, rc;
long n;
BPB *tbpb;
char *bbuf;

	_mediach(device);
	n = (device>=FIRST_HD_DEVICE) ? MAX_HD_LOGSECSIZE : MAX_FL_LOGSECSIZE;
	if ((bbuf=malloc(n)) == NULL) {
		errmsg(OTHER_ERROR,0,"Insufficient memory for","boot sector");
		return 0;
	}

	for (i = 0; i <= MAX_RETRY; i++)
		if ((rc=Rwabs(2,bbuf,1,0,device)) == 0)
			break;
	if (i > MAX_RETRY) {
		errmsg(BIOS_ERROR,rc,"reading","boot sector");
		free(bbuf);
		return 0;
	}

	rc = 1;
	if ((tbpb=Getbpb(device)) == NULL) {
		errmsg(OTHER_ERROR,0,"Getbpb() failed","");
		rc = 0;
	} else if (load_mybpb(bbuf,mybpb,tbpb) == 0) {
		errmsg(OTHER_ERROR,0,"Invalid","boot sector");
		rc = 0;
	}

	free(bbuf);

	return rc;
}

/*
 *	decode MYBPB from sector 0 ...
 *		if error, returns 0
 *		else returns 1
 *
 *	source:	disk.c
 */
int load_mybpb(char *buff,MYBPB *mybpb,BPB *tbpb)
{
int n;
long maxclus;

	if (tbpb)
		mybpb->isfat16 = tbpb->bflags & 0x01;

	if (memcmp(buff+DATALITE_ID_OFFSET,DATALITE_ID,4) == 0)
		mybpb->datalite = 1;
	else mybpb->datalite = 0;

	mybpb->tsec = get_word(buff + 19);		/* total sectors */
	if (mybpb->datalite)
		mybpb->tsec = *(unsigned short *)(buff+DATALITE_SEC_OFFSET);
	mybpb->bps = get_word(buff + 11);		/* bytes per sector */
	mybpb->spc = buff[13];					/* sec per cluster */
	mybpb->spt = get_word(buff + 24);		/* sec per track */
	if ((mybpb->spt > 999) || (mybpb->spt < 0))
		mybpb->spt = 0;						/* silently fix cosmetic error */

	mybpb->tpc = get_word(buff + 26);		/* track per cylinder */
	if ((mybpb->tpc > 999) || (mybpb->tpc < 0))
		mybpb->tpc = 0;						/* silently fix cosmetic error */

	mybpb->media = buff[21];
	mybpb->spf = get_word(buff + 22);		/* sec per fat */
	mybpb->nfat = buff[16];					/* number of fat */
	mybpb->ndir = get_word(buff + 17);		/* max dir root entries */

	mybpb->rds = (short)((mybpb->ndir*sizeof(DIRENTRY)+mybpb->bps-1) / mybpb->bps);
											/* rootdir in sectors */
	mybpb->nhide = get_word(buff + 28);		/* hidden sectors */
	if ((mybpb->nhide > 999) || (mybpb->nhide < 0))
		mybpb->nhide = 0;					/* silently fix */

	mybpb->nres = get_word(buff + 14);		/* number reserved */
	mybpb->firstd = (mybpb->nfat*mybpb->spf) + mybpb->rds + mybpb->nres;

	mybpb->cdata = (short) ((mybpb->tsec-mybpb->firstd) / mybpb->spc);

	mybpb->bpf = (long)mybpb->spf * mybpb->bps;
	mybpb->rdb = (long)mybpb->rds * mybpb->bps;	/* root dir bytes */
	mybpb->bpc = (long)mybpb->spc * mybpb->bps;
	mybpb->tbyte = (long)mybpb->cdata * mybpb->bpc;

	for (n = mybpb->bps, mybpb->sector_shift = 0; n > 1; n >>= 1)
		mybpb->sector_shift++;
	for (n = mybpb->bpc, mybpb->cluster_shift = 0; n > 1; n >>= 1)
		mybpb->cluster_shift++;

	if (mybpb->isfat16) {
		mybpb->bitmask = ENDCHAIN16;
		mybpb->badmask = BADCLUST16;
		maxclus = (mybpb->bpf / 2L) - 2;
	} else {
		mybpb->bitmask = ENDCHAIN12;
		mybpb->badmask = BADCLUST12;
		maxclus = ((mybpb->bpf * 2L) / 3L) - 2;
	}

	if (maxclus < mybpb->cdata)				/* shouldn't happen */
		mybpb->cdata = maxclus;

	mybpb->maxc = mybpb->cdata + 2;			/* max cluster # + 1 */

	/*
	 *	perform final validation
	 */
	for (n = MIN_LOGSECSIZE; n <= MAX_HD_LOGSECSIZE; n <<= 1)
		if (mybpb->bps == n)
			break;
	if (n > MAX_HD_LOGSECSIZE)
		return 0;

	if ((mybpb->spc > 16) || (mybpb->spc < 0))				//MAGNUM
		return 0;

	if (mybpb->nfat != 2)
		return 0;

	if ((mybpb->cdata < 0) || (mybpb->cdata > 32767))		//MAGNUM
		return 0;

	return 1;
}

/*
 *	convert in-memory little-endian 12-bit FAT to 16-bit big-endian equivalent
 *
 *	source:	disk.c
 */
void fat12_to_fat16(unsigned char *f12,unsigned short *f16)
{
int j;

	/*
	 *	in loop n, we convert clusters 2n and 2n+1 ...
	 */
	for (j = 0; j < (dpb.maxc+1)/2; j++, f12 += 3, f16 += 2) {
		*f16 = (*f12 | (*(f12+1)<<8)) & 0x0fff;
		*(f16+1) = ((*(f12+1)>>4) | (*(f12+2)<<4)) & 0x0fff;
	}
}

/*
 *	save boot sector + FAT to mirror file
 *	(if mirror file already exists, renames it to .bak first after removing previous .bak)
 *	returns 0 if ok, else -1
 *
 *	source:	info.c
 */
int save_info(short dev,char *path)
{
int i, nsec, rc;
long ntowrite;
FILE *fp;
char fbootf[MAX_PATHNAME], bak[MAX_PATHNAME];
MYBPB tpb;
char *rootbuf;

	if (!get_mybpb(&tpb,dev))
		return -1;					/* msg has already been issued */

	nsec = 1 + 2 * tpb.spf + tpb.rds;

	if ((rootbuf=malloc(nsec*tpb.bps)) == NULL) {
		errmsg(OTHER_ERROR,0,"Insufficient memory for","boot sector/FATs");
		return -1;
	}

	for (i = 0; i <= MAX_RETRY; i++)
		if ((rc=Rwabs(2,rootbuf,nsec,0,dev)) == 0)
			break;
	if (i > MAX_RETRY) {
		errmsg(BIOS_ERROR,rc,"reading","boot sector/FATs");
		free(rootbuf);
		return -1;
	}

	/*
	 *	new format only saves FAT#1 + root, so move root data
	 *	immediately after FAT#1 data
	 */
	memmove(rootbuf+tpb.bps+tpb.bpf,rootbuf+tpb.bps+2*tpb.bpf,tpb.rdb);
	ntowrite = (long) tpb.bps + tpb.bpf + tpb.rdb;

	sprintf(fbootf,"%s" MIRROR_FILE,path,'A'+dev);
	sprintf(bak,"%s" MIRROR_BACK,path,'A'+dev);
	if (access(fbootf,0) == 0) {
		remove(bak);
		rename(fbootf,bak);
	}

	if (!(fp=fopen(fbootf,"wb"))) {
		errmsg(DOS_ERROR,_OSERR,"creating",fbootf);
		free(rootbuf);
		return -1;
	}

	for (i = 0; i <= MAX_RETRY; i++)
		if ((rc=fwrite(rootbuf,ntowrite,1,fp)) == 1)
			break;

	if (i > MAX_RETRY) {
		if (rc == 0)
			errmsg(OTHER_ERROR,0,"Disk is full","");
		else errmsg(DOS_ERROR,_OSERR,"writing",fbootf);
		rc = -1;
	} else rc = 0;

	fclose(fp);
	free(rootbuf);
	return rc;
}

/*
 *	load root directory into memory and validate it
 *
 *	source:	medic1.c
 */
char *load_root()
{
int i, rc;
char *endpoint, *end;
char *temp;

	if ((dirlist[0].dirbuf=calloc(1,dpb.rdb)) == NULL) {
		errmsg(OTHER_ERROR,0,"Insufficient memory for","root directory");
		return NULL;
	}

	for (i = 0; i <= MAX_RETRY; i++)
		if ((rc=Rwabs(2,dirlist[0].dirbuf,dpb.rds,1+2*dpb.spf,dev)) == 0)
			break;

	if (i > MAX_RETRY) {
		errmsg(BIOS_ERROR,rc,"reading","root directory");
		free(dirlist[0].dirbuf);	/* added by rfb */
		return NULL;
	}

	endpoint = sort_root(dirlist[0].dirbuf);
	end = dirlist[0].dirbuf + dpb.rdb;

	for (temp = endpoint; temp < end; temp += sizeof(DIRENTRY)) {
		for (i = 0; i < sizeof(DIRENTRY); i++) {
			if (*(temp + i)) {
				errcnt[BADENTRY]++;
				break;
			}
		}
	}

	val_direntries((DIRENTRY *)dirlist[0].dirbuf,endpoint);

	return endpoint;
}

/*
 *	check FAT chain for current file
 *		returns 0 if OK, -1 if error
 *
 *	source:	medic1.c
 */
int check_fat(DIRENTRY *file)
{
short st, prev;
short start, nclus;
int i = 0;
int ncollision = 0, rc = 0;
long size;

	size = get_long((char *)&file->size);
	nclus = (size+dpb.bpc-1) >> dpb.cluster_shift;
	if (size < 0) {						//rfb: shouldn't we flag this ???
		nclus = 0;
	}

	st = start = get_word((char *)&file->start);
	if (st == 0) {
		if (size != 0L) {
			errcnt[BADSIZE]++;
			return -1;
		}
		return 0;
	}

	if (size == 0L)
		nclus = 1;

	if (tfat[st] == 1) {		/* collision in first cluster ... */
		errcnt[COLLISION]++;
		return -1;
	}

	for (i = 0; i <= dpb.cdata; ) {
		prev = st;
		st = fat16[st];
		i++;

		if (st == dpb.bitmask)
			break;

		if (!valid_cluster(st)) {
			errcnt[BADFAT]++;
			fat16[prev] = dpb.bitmask;
			i = nclus;		/* force match to avoid BADSIZE error as well */
			rc = -1;
			break;
		}

		if (tfat[st] == 1) {
			if (ncollision == 0) {
				errcnt[COLLISION]++;
				ncollision++;
				rc = -1;
			}
		}
	}

	if (i > dpb.cdata) {	/* loop in FAT? */
		errcnt[BADFAT]++;
		rc = -1;
	}

	if (i != nclus) {		/* mismatched file length */
		errcnt[BADSIZE]++;
		rc = -1;
	}

	do_block(start);

	return rc;
}

/*
 *	check for lost clusters
 *
 *	source:	medic1.c
 */
int check_garbage()
{
int i, rc = 0;

	for (i = 2; i < dpb.maxc; i++) {
		if ((tfat[i] != 0) && (tfat[i] != 1)) {
			if (tfat[i] != dpb.badmask) {
				errcnt[LOSTCLUSTER]++;
				rc = -1;					/* found a lost cluster ... */
			}
		}
	}

	return rc;
}

/*
 *	sets 'this cluster belongs to a file' indicator in tfat[]
 *
 *	source:	do_novisblock() in medic1.c
 */
void do_block(short start)
{
short st;

	st = start;
	do {
		tfat[st] = 1;
		st = fat16[st];
	} while (st != dpb.bitmask);
}

/*
 *	test all files and directories below a specified start point
 *
 *	source:	medic1.c
 */
int test_loop(short dstart,char *enddir)
{
short st, sc;
short dirstart, ncl;
DIRENTRY *curfile;
DIRENTRY *dotfile;
int i;
short start;
//long offset;
char *endpoint, *end, *temp;
char dname[MAX_FILENAME];
//int rc;

	if (check_abort())
		return -1;

	if (dstart == 0) {
		if ((enddir=load_root()) == NULL)
			return -1;
	}

	curfile = (DIRENTRY *) dirlist[dirlevel].dirbuf;
	if (dirlevel == 0) {
		if (curfile->attr == FA_VOLUME)
			curfile++;
	} else curfile += 2;

	for ( ; curfile->attr == FA_SUBDIR; curfile++) {
		if (curfile->name[0] == DELETION_MARK)
			continue;
		dirlevel++;
		st = dirstart = get_word((char *)&curfile->start);
		ncl = 1;
		while (fat16[st] != dpb.bitmask) {
			sc = st;
			st = fat16[st];
			if (!valid_cluster(st)) {
				errcnt[BADDIR]++;
				fat16[sc] = dpb.bitmask;
				tfat[sc] = dpb.bitmask;
				break;
			}
			ncl++;
		}

		if ((dirlist[dirlevel].dirbuf=calloc(ncl,dpb.bpc)) == NULL) {
			errmsg(OTHER_ERROR,0,"Insufficient memory for","directory");
			dirlevel--;
			if (dirlevel > 0)
				up_dirlist();
			return -1;
		}

		if (load_directory(&dirstart) < 0)
			continue;

#if 0
		offset = 0L;
		st = dirstart;
		do {
			for (i = 0; i <= MAX_RETRY; i++)
				if ((rc=Rwabs(2,dirlist[dirlevel].dirbuf+offset,dpb.spc,logsector(st),dev) == 0))
					break;
			if (i > MAX_RETRY) {
				errcnt[READERR]++;
				do_block(dirstart);
				up_dirlist();
				goto enddir1;
			} 
			sc = st;
			st = fat16[st];
			offset += dpb.bpc;
		} while (st != dpb.bitmask);
#endif

		dotfile = (DIRENTRY *)dirlist[dirlevel].dirbuf;
		get_name(dotfile,dname);
		start = get_word((char *)&dotfile->start);
		if ((strcmp(dname,".") != 0) || (start != dirstart)) {
			errcnt[BADDIR]++;
			fat16[dirstart] = 0;
			curfile->name[0] = DELETION_MARK;
			up_dirlist();
			continue;
		}
		dotfile++;
		get_name(dotfile,dname);
		start = get_word((char *)&dotfile->start);
		if ((strcmp(dname,"..") != 0) || (start != dstart)) {
			errcnt[BADDIR]++;
			fat16[dirstart] = 0;
			curfile->name[0] = DELETION_MARK;
			up_dirlist();
			continue;
		}
		endpoint = sort_dir(dirlist[dirlevel].dirbuf,ncl);
		end = dirlist[dirlevel].dirbuf + dpb.bpc * ncl;
		for (temp = endpoint; temp < end; temp += sizeof(DIRENTRY)) {
			for (i = 0; i < sizeof(DIRENTRY); i++) {
				if (*(temp + i)) {
					errcnt[BADENTRY]++;
					memset(temp,0x00,sizeof(DIRENTRY));
					break;
				}
			}
		}

		val_direntries((DIRENTRY *)dirlist[dirlevel].dirbuf,endpoint);
		do_block(dirstart);

		if (test_loop(dirstart,endpoint) < 0) {
			up_dirlist();
			return -1;
		}

//enddir1:
//		;
	}

	while ((void *) curfile < (void *) enddir) {
		if (curfile->name[0] != DELETION_MARK) {
			if (check_fat(curfile) < 0) {
				curfile->name[0] = DELETION_MARK;
			}
		}
		curfile++;
	}

	up_dirlist();
	return 0;
}

/*
 *	load directory into memory, handling unreadable sectors
 *
 *	return code:
 *		0	no read errors
 *		-1	a read error was detected
 *
 *	source:	medic1.c
 */
int load_directory(short *dirstart)
{
short st, prev;
int i;
long offset, rc;

	offset = 0L;
	st = prev = *dirstart;	/* we only initialise prev to prevent erroneous 'warning 94' messages */
	do {
		for (i = 0; i <= MAX_RETRY; i++)
			if ((rc=Rwabs(2,dirlist[dirlevel].dirbuf+offset,dpb.spc,logsector(st),dev) == 0L))
				break;
		if (i > MAX_RETRY) {
			/* diskerr(2); */
			errcnt[READERR]++;
			do_block(*dirstart);
			up_dirlist();
			return -1;
		}
		prev = st;
		st = fat16[st];
		offset += dpb.bpc;
	} while (st != dpb.bitmask);

	return 0;
}

/*
 *	go 'up' the dirlist
 *
 *	source:	medic1.c
 */
void up_dirlist()
{
	free(dirlist[dirlevel].dirbuf);
	memset(&dirlist[dirlevel],0x00,sizeof(DIRLIST));
	dirlevel--;
}

/*
 *	sort root directory into the following order:
 *		volume label (if present)
 *		all subdirectory entries
 *		file entries (we preserve the existing entry order)
 *
 *	returns pointer to last entry + 1
 *
 *	source:	opt2.c
 */
char *sort_root(char *dirbuf)
{
char temp[sizeof(DIRENTRY)];
char *curpoint;
char *inspoint;
char *endpoint;
int volume_labels_allowed;

	inspoint = dirbuf;	/* no . or .. in root */

	for (endpoint = dirbuf+dpb.rdb-sizeof(DIRENTRY); endpoint >= inspoint; endpoint -= sizeof(DIRENTRY))
		if (*endpoint != 0x00)
			break;

	endpoint += sizeof(DIRENTRY);						/* endpoint is last entry used + 1 */

	/*
	 *	we now scan the entire directory, deleting any extra volume labels
	 *	(early TOS versions didn't check for an existing vol label when creating one)
	 */
	volume_labels_allowed = 1;

	for (curpoint = inspoint; curpoint < endpoint; curpoint += sizeof(DIRENTRY)) {
		if (*curpoint && (fattr(curpoint)&FA_VOLUME)) {
			if (volume_labels_allowed)
				volume_labels_allowed--;
			else *curpoint = DELETION_MARK;
		}
	}

	if (*inspoint && (fattr(inspoint)&FA_VOLUME))		/* skip any initial volume label */
		inspoint += sizeof(DIRENTRY);

	/*
	 *	move insertion point to first NON-subdirectory entry
	 */
	while(*inspoint && (fattr(inspoint)&FA_SUBDIR))
		inspoint += sizeof(DIRENTRY);

	/*
	 *	scan through remainder of directory:
	 *	. if not a deleted entry:
	 *		. if a subdirectory entry, move directory entries up 1 & insert it
	 *		. else just skip it
	 *	. else it's a deleted entry:
	 *		. just skip it
	 */
	curpoint = inspoint + sizeof(DIRENTRY);
	while (curpoint < endpoint) {
		if (*curpoint != DELETION_MARK) {
			if (*curpoint && (fattr(curpoint)&FA_SUBDIR)) {
				memcpy(temp,curpoint,sizeof(DIRENTRY));
				memmove(inspoint+sizeof(DIRENTRY),inspoint,curpoint-inspoint);
				memcpy(inspoint,temp,sizeof(DIRENTRY));
				inspoint += sizeof(DIRENTRY);
			}
			curpoint += sizeof(DIRENTRY);
		} else {
			curpoint += sizeof(DIRENTRY);
		}
	}

	return endpoint;
}

/*
 *	sort directory into the following order:
 *		all subdirectory entries
 *		file entries (we preserve the existing entry order)
 *
 *	returns pointer to last entry + 1
 *
 *	source:	opt2.c
 */
char *sort_dir(char *curdir,int ncl)
{
char temp[sizeof(DIRENTRY)];
char *curpoint;
char *inspoint;
char *endpoint;

	inspoint = curdir + 2*sizeof(DIRENTRY);		/* initial insertion point after . and .. */

	for (endpoint = curdir+dpb.bpc*ncl-sizeof(DIRENTRY); endpoint >= inspoint; endpoint -= sizeof(DIRENTRY))
		if (*endpoint != 0x00)
			break;

	endpoint += sizeof(DIRENTRY);				/* endpoint is last entry used + 1 */

	/*
	 *	move insertion point to first NON-subdirectory entry
	 */
	while(*inspoint && (fattr(inspoint)&FA_SUBDIR))
		inspoint += sizeof(DIRENTRY);

	/*
	 *	scan through remainder of directory:
	 *	. if not a deleted entry:
	 *		. if a subdirectory entry, move directory entries up 1 & insert it
	 *		. else just skip it
	 *	. else it's a deleted entry:
	 *		. just skip it
	 */
	for (curpoint = inspoint + sizeof(DIRENTRY); curpoint < endpoint; ) {
		if (*curpoint != DELETION_MARK) {
			if (*curpoint && (fattr(curpoint)&FA_SUBDIR)) {
				memcpy(temp,curpoint,sizeof(DIRENTRY));
				memmove(inspoint+sizeof(DIRENTRY),inspoint,curpoint-inspoint);
				memcpy(inspoint,temp,sizeof(DIRENTRY));
				inspoint += sizeof(DIRENTRY);
			}
			curpoint += sizeof(DIRENTRY);
		} else {
			curpoint += sizeof(DIRENTRY);
		}
	}

	return endpoint;
}

/*
 *****  S U P P O R T   R O U T I N E S  *****
 */
/*
 *	return bitmap of CD devices
 *
 *	this routine returns a bitmap of CD devices controlled by ExtenDOS/MetaDOS.
 *	the bitmap is organised like the bitmap returned by Drvmap(),
 *	i.e. bit 0 => 'A', bit 1 = > 'B' etc
 */
unsigned long CDmap()
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

/*
 *	calculate days since 1 January 1980
 */
int calcdays(unsigned short date)
{
static short cumdays[13] = { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
int days, yy, mm, dd;

	yy = (date >> 9) & 0x7f;
	mm = (date >> 5) & 0xf;
	dd = date & 0x1f;

	days = yy * 365 + (yy/4);		/* allow for leap years */
	days += cumdays[mm];			/* add in total days of prior months */
	if ((yy%4 == 0) && (mm <= 2))	/* handle current leap year */
		days--;
	days += dd;

	return days;
}

/*
 *	check for user abort
 *		returns 1 & sets bailout if so
 */
int check_abort()
{
int key;

	if (Cconis()) {
		key = Crawcin() & 0xff;
		if (key == ABORT_KEY) {
			bailout = TRUE;
			return 1;
		}
	}
	return 0;
}

/*
 *	issue error message
 */
void errmsg(int type,int errnum,char *msg1,char *msg2)
{
	switch(type) {
	case DOS_ERROR:
		printf("\nDOS error %d %s %s\n",errnum,msg1,msg2);
		break;
	case BIOS_ERROR:
		printf("\nBIOS error %d %s %s\n",errnum,msg1,msg2);
		break;
	default:
		printf("\n%s %s\n",msg1,msg2);
	}
	fflush(stdout);
}

/*
 *	wait for input
 */
int get_key()
{
	return Crawcin() & 0xff;
}

/*
 *	convert a long from Intel byte order
 *
 *	source:	misc.c
 */
long get_long(char *in)
{
long n;

	n = *in++;
	n += *in++ << 8;
	n += *in++ << 16;
	n += *in << 24;

	return n;
}

/*
 *	extract name from directory entry
 *
 *	source:	disk.c
 */
int get_name(DIRENTRY *dir,char *fname)
{
int j;
char *p;

	if (!dir->name[0]) {
		*fname = '\0';
		return 0;
	}

	for (j = 0, p = dir->name; (*p != ' ') && (j < NODE_LENGTH); j++)
		*fname++ = *p++;

	if (dir->ext[0] != ' ') {		/* if there is an extension */
		*fname++ = '.';
		for (j = 0, p = dir->ext; (*p != ' ') && (j < EXT_LENGTH); j++)
			*fname++ = *p++;
	}

	*fname = '\0';

	return 1;
}

/*
 *	get a short system variable
 */
short get_sys(short *p)
{
short n;

	sup_on();
	n = *p;
	sup_off();

	return n;
}

/*
 *	convert an unsigned short from Intel byte order
 *
 *	source:	misc.c
 */
unsigned short get_word(char *in)
{
unsigned short n;

	n = *in++;
	n += *in << 8;

	return n;
}

/*
 *	convert cluster to logical sector number
 *
 *	source:	disk.c
 */
unsigned short logsector(short cluster)
{
	return ((cluster - 2) * dpb.spc + dpb.firstd);
}

/*
 *	open workstation
 *
 *	source:	main.c
 */
short open_workstation(void)
{
int x;
short handle, dummy;
short work_in[11];			/* Input to GSX parameter array */
short work_out[57]; 		/* Output from GSX parameter array */

	work_in[0] = Getrez() + 2;
	for (x = 1; x < 10; work_in[x++] = 1)
		;
	work_in[10] = 2;
	handle = graf_handle(&dummy,&dummy,&dummy,&dummy);
	v_opnvwk(work_in,&handle,work_out);

	if (!handle)
		exit(0);

	return handle;
}
