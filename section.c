/*
 *	section.c
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 *
 *	Modified for Obsidian Edge (November 2026): GCC port: <sys/stat.h> included for GCC, local vsscanf() renamed
 */

/*
 *	this code was stolen from windform.  the following minimal changes
 *	have been made for integration:
 *		. the include for windform.h has been removed; as a result:
 *			FILE *fp_inf, app_path are now declared here (app_path
 *			is initialised by main.c)
 *			TRUE / FALSE are defined
 *		. vsscanf() & friends have been added (from prefs.c)
 *
 *
 *
 *	windform extension routines: information file 'section' handling
 *
 *	by roger burrows, anodyne software
 *
 *	These routines allow the creation and maintenance of essentially
 *	independent sections within the .INF file.
 *
 *	The new structure of a .INF file is:
 *
 *		[sectionABC]
 *		line1
 *		line2
 *		...
 *		lineN
 *		[sectionXYZ]
 *		anotherline1
 *		...
 *		anotherlineN
 *
 *	and so on.
 *
 *
 *	The following routines are implemented:
 *
 *	open_section()
 *
 *		This routine opens a 'section' within a windform v3 information file.
 *		It is used instead of the standard open_inf() to allow the creation
 *		of separate sections (e.g. for different functions) within a single
 *		file.  Once the section is open, you should process it via the new
 *		routines: read_section_line(), write_section_line(), close_section().
 *
 *		syntax:
 *			int open_section(char *filename,char *mode,char *sectionname)
 *
 *			. filename and mode are exactly as in open_inf(); in particular,
 *			  filename is _unqualified_
 *			. sectionname is the name of the 'section'; within the .INF file
 *			  it is written out on its own line, surrounded by square brackets
 *			. returns 0 if successful
 *
 *
 *	read_section_line()
 *		This routine reads a line within a section.  It is similar to
 *		read_inf_line(), except that the search for a matching line stops
 *		at the end of the current section.
 *
 *		syntax:
 *			void read_section_line(char *pattern,char *fmt,...)
 *
 *			. the arguments are exactly as used with read_inf_line()
 *
 *
 *	write_section_line()
 *		This routine writes a line within a section.  It provides essentially
 *		the same function as write_inf_line().
 *
 *		syntax:
 *			int write_section_line(char *pattern,char *fmt,...)
 *
 *			. the arguments are exactly as used with write_inf_line()
 *			. returns 0 if successful
 *
 *
 *	close_section()
 *		This routine closes a section.  It provides essentially the same
 *		function as close_inf().
 *
 *		syntax:
 *			void close_section(void)
 *
 *
 *	IMPORTANT NOTE:
 *		These routines use the same file pointer as the original .INF file
 *		processing routines.  This allows them to work with save_windform()
 *		and init_windform() more-or-less transparently.  If you plan to use
 *		these routines with save_windform()/init_windform(), ensure that you
 *		use open_section(), rather than open_inf().  This will position the
 *		file pointers correctly for init_windform() when reading, and will
 *		preserve other sections when writing via save_windform().
 *
 *
 *	version 1.0 (september 1998)
 *		Original version
 *
 *	version 1.1 (april 2000)
 *		Add return codes for possible failure modes
 *		Changed source filename from windfrmx.c to section.c
 *
 *	version 1.2 (february 2001)
 *		Fixed bug that may have caused the 'disappearing options' problem:
 *		open_file() uses the fully-qualified filename (including app_path),
 *		but stat() and process_existing_file() use the unqualified filename.
 *		if app_path() isn't the current directory, this will cause stat()
 *		to fail, which will cause the file to be overwritten with just the
 *		section specified via open_section().  Fixed by generating the
 *		fully-qualified filename in open_section() rather than open_file().
 *
 *	version 2.57 (january/2017)
 *		prepare for public release
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <ctype.h>
#include <osbind.h>
#include <stdarg.h>
#include <string.h>
#ifdef __TURBOC__
	#include <ext.h>
#endif
#if defined(LATTICE) || defined(__GNUC__)
	#include <sys/types.h>
	#include <sys/stat.h>
#endif

#ifdef __GNUC__
	#define vsscanf section_vsscanf		/* MiNTLib has its own vsscanf() */
#endif

#ifdef MALLOC_DEBUG
	#include "trmalloc/trmalloc.h"			/* these two are just for trmalloc() stuff ... */
	#define MODULE_NUMBER	21
#endif

#define TRUE			1
#define FALSE			0


#define	SECTION_MARKER	'['

/*
 *	return codes (for debugging purposes)
 */
#define CANT_OPEN_INPUT		-301
#define CANT_CREATE_OUTPUT	-302
#define CANT_MALLOC_BUFFER	-303
#define CANT_OPEN_OLD_FILE	-304
#define INPUT_NOT_OPEN		-305
#define CANT_READ_FILE		-306
#define CANT_OVERWRITE_FILE	-307
#define OUTPUT_NOT_OPEN		-308
#define CANT_WRITE_FILE		-309

FILE *fp_inf;
extern char app_path[];

/*
 *	internal routines
 */
static void close_file(void);
static int open_file(char *filename,char *mode);
static int process_existing_file(char *filename,char *mode,char *sectionname,char *buf,long size);
static int read_line(char *marker);
static int write_line(char *line);
/* the following are imported from windform's prefs.c */
static int _scanf(unsigned char *ip,int (*get)(char *ip),int (*unget)(char c,char *ip),unsigned char *fmt,char **args);
static int get(char *s);
static int unget(char c,char *s);
static int vsscanf(char *ss,char *fmt,va_list args);

int open_section(char *basename,char *mode,char *sectionname)
{
struct stat statbuf;
char filename[256];
char header[100];
char *buf;
int rc;

	sprintf(filename,"%s%s",app_path,basename);
	sprintf(header,"%c%s]",SECTION_MARKER,sectionname);

	/* handle input file */

	if (strchr(mode,'w') == NULL) {			/* if not write, assume input */
		if (open_file(filename,mode) < 0)
			return CANT_OPEN_INPUT;
		return read_line(header);			/* read up to section start */
	}

	/* handle output file */

	if (stat(filename,&statbuf)) {			/* if file doesn't exist, */
		if (open_file(filename,mode) < 0)
			return CANT_CREATE_OUTPUT;
		return write_line(header);			/* create section */
	}

	/* handle existing output file */

	if ((buf=malloc(statbuf.st_size)) == NULL)	/* insufficient memory ... */
		return CANT_MALLOC_BUFFER;
	rc = process_existing_file(filename,mode,header,buf,statbuf.st_size);
	free(buf);

	return rc;
}

void read_section_line(char *pattern,char *fmt,...)
{
size_t len, pos;
int found = FALSE;
char buf[256];
va_list args;

	if (!fp_inf)				/* if file not open, */
		return;					/*  return silently  */

	buf[0] = '\0';
	len = strlen(pattern);
	va_start(args,fmt);

	pos = ftell(fp_inf);		/* remember where we were */
	while(fgets(buf,256,fp_inf)) {
		if (buf[0] == SECTION_MARKER)
			break;
		if (strncmp(buf,pattern,len) == 0) {
			vsscanf(buf+len,fmt,args);
			found = TRUE;
			break;
		}
	}
	if (!found)						/* if not found, */
		fseek(fp_inf,pos,SEEK_SET);	/* restore original position */

	va_end(args);
}

int write_section_line(char *pattern,char *fmt,...)
{
char str[80], buf[256];
va_list args;
int rc;

	sprintf(str,"%s %s",pattern,fmt);
	va_start(args,fmt);

	vsprintf(buf,str,args);
	rc = write_line(buf);
	va_end(args);

	return rc;
}

void close_section(void)
{
	close_file();
}


/*
 *	internal routines
 */
static int process_existing_file(char *filename,char *mode,char *header,char *buf,long size)
{
FILE *fp;
char line[100], *p, *q;
size_t rc, hdrlen;
int skip;

	if ((fp=fopen(filename,"rb")) == NULL)	/* must be 'b' (binary) so that file size matches */
		return CANT_OPEN_OLD_FILE;
	rc = fread(buf,size,1,fp);
	fclose(fp);
	if (rc != 1)
		return CANT_READ_FILE;

	hdrlen = strlen(header);

	if (open_file(filename,mode) < 0)	/* create a new file, omitting existing section (if any) */
		return CANT_OVERWRITE_FILE;
	for (p = buf, skip = 0; p < buf+size; ) {
		for (q = line; p < buf+size; )
			if ((*q++=*p++) == '\n') {	/* copy until end of line */
				q--;					/* back up to nl */
				break;
			}
		while(q > line)
			if (isgraph(*--q))			/* back up past cr (if present) and any spaces */
				break;					/* (because write_section_line can add them ...) */
		*++q = '\0';
										/* the following section skips the section we are  */
										/* going to write ... it will be placed at the end */
		if (strncmp(line,header,hdrlen) == 0)	/* matching section: start skipping */
			skip++;
		else if (line[0] == '[')				/* any other section: stop skipping */
			skip = 0;
		if (!skip)
			if (rc=write_line(line))
				return rc;
	}
	rc = write_line(header);			/* recreate start of section */

	return rc;
}

static int open_file(char *filename,char *mode)
{
	close_file();						/* precaution */

	fp_inf = fopen(filename,mode);
	return fp_inf?0:-1;
}

static int read_line(char *marker)
{
char buf[256];
size_t len;

	if (!fp_inf)
		return INPUT_NOT_OPEN;

	len = strlen(marker);
	while(fgets(buf,256,fp_inf))
		if (strncmp(buf,marker,len) == 0)
			break;
	return 0;
}

static int write_line(char *line)
{
	if (!fp_inf)
		return OUTPUT_NOT_OPEN;

	if (fputs(line,fp_inf) < 0) {
		close_file();
		return CANT_WRITE_FILE;
	}

	fputc('\n',fp_inf);
	return 0;
}

static void close_file()
{
	if (fp_inf)
		fclose(fp_inf);
	fp_inf = 0;
}


/*****	the following code was originally in windform's prefs.c	*****/

#define	skip()	while(isspace(c)) { if ((c=(*get)(ip))<1) goto done; }

static int _scanf(unsigned char *ip,int (*get)(char *ip),int (*unget)(char c,char *ip),unsigned char *fmt,char **args)
{
register long n;
register int c, width, lval, sval, cnt = 0;
int store, neg, base, wide1, endnull, rngflag, c2;
register unsigned char *p;
unsigned char delim[256], digits[17], *q;

	if (!*fmt)
		return(0);

	c = (*get)(ip);
	while(c > 0) {
		store = FALSE;
		if (*fmt == '%') {
			n		= 0;
			width	= -1;
			wide1	= 1;
			base	= 10;
			lval	= FALSE;
			sval	= FALSE;
			store	= TRUE;
			endnull	= TRUE;
			neg		= -1;

			strcpy(delim,  "\011\012\013\014\015 ");
			strcpy(digits, "01234567890ABCDEF");

			if (fmt[1] == '*') {
				endnull = store = FALSE;
				++fmt;
			}

			while (isdigit(*++fmt)) {	/* width digit(s) */
				if (width == -1)
					width = 0;
				wide1 = width = (width * 10) + (*fmt - '0');
			}
			--fmt;
fmtnxt:
			++fmt;
			switch(tolower(*fmt)) {	/* tolower() is a MACRO! */
			case '*':
				endnull = store = FALSE;
				goto fmtnxt;

			case 'l':	/* long data */
				lval = TRUE;
				goto fmtnxt;

			case 'h':	/* short data */
				sval = TRUE;
				goto fmtnxt;

			case 'i':	/* any-base numeric */
				base = 0;
				goto numfmt;

			case 'b':	/* unsigned binary */
				base = 2;
				goto numfmt;

			case 'o':	/* unsigned octal */
				base = 8;
				goto numfmt;

			case 'x':	/* unsigned hexadecimal */
				base = 16;
				goto numfmt;

			case 'd':	/* SIGNED decimal */
				neg = FALSE;
				/* FALL-THRU */

			case 'u':	/* unsigned decimal */
numfmt:			skip();

				if (isupper(*fmt))
					lval = TRUE;

				if (!base) {
					base = 10;
					neg = FALSE;
					if (c == '%') {
						base = 2;
						goto skip1;
					} else if (c == '0') {
						c = (*get)(ip);
						if (c < 1)
							goto savnum;
						if ((c != 'x')
						 && (c != 'X')) {
							base = 8;
							digits[8]= '\0';
							goto zeroin;
						}
						base = 16;
						goto skip1;
					}
				}

				if ((neg == FALSE) && (base == 10)
				 && ((neg = (c == '-')) || (c == '+'))) {
skip1:
					c = (*get)(ip);
					if (c < 1)
						goto done;
				}

				digits[base] = '\0';
				p = ((unsigned char *) strchr(digits,toupper(c)));

				if ((!c || !p) && width)
					goto done;

				while (p && width-- && c) {
					n = (n * base) + (p - digits);
					c = (*get)(ip);
zeroin:
					p = ((unsigned char *) strchr(digits,toupper(c)));
				}
savnum:
				if (store) {
					p = ((unsigned char *) *args);
					if (neg == TRUE)
						n = -n;
					if (lval)
						*((long *) p) = n;
					else if (sval)
						*((short *) p) = n;
					else
						*((int *) p) = n;
					++cnt;
				}
				break;

			case 'c':	/* character data */
				width = wide1;
				endnull	= FALSE;
				delim[0] = '\0';
				goto strproc;

			case '[':	/* string w/ delimiter set */

				/* get delimiters */
				p = delim;

				if (*++fmt == '^')
					fmt++;
				else
					lval = TRUE;

				rngflag = 2;
				if ((*fmt == ']') || (*fmt == '-')) {
					*p++ = *fmt++;
					rngflag = FALSE;
				}

				while (*fmt != ']') {
					if (*fmt == '\0')
						goto done;
					switch (rngflag) {
					case TRUE:
						c2 = *(p-2);
						if (c2 <= *fmt) {
						    p -= 2;
						    while (c2 < *fmt)
						    	*p++ = c2++;
						    rngflag = 2;
						    break;
						}
					    /* fall thru intentional */

					case FALSE:
						rngflag = (*fmt == '-');
						break;

					case 2:
						rngflag = FALSE;
					}

					*p++ = *fmt++;
				}

				*p = '\0';
				goto strproc;

			case 's':	/* string data */
				skip();
strproc:
				/* process string */
				p = ((unsigned char *) *args);

				/* if the 1st char fails, match fails */
				if (width) {
					q = ((unsigned char *)strchr(delim, c));
					if ((c < 1)
					 || (lval ? !q : (int) q)) {
						if (endnull)
							*p = '\0';
						goto done;
					}
				}

				for ( ; ; ) {	/* FOREVER */
					if (store)
						*p++ = c;
					if (((c = (*get)(ip)) < 1) ||
					    (--width == 0))
						break;

					q = ((unsigned char *)strchr(delim, c));
					if (lval ? !q : (int) q)
						break;
				}

				if (store) {
					if (endnull)
						*p = '\0';
					++cnt;
				}
				break;

			case '\0':	/* early EOS */
				--fmt;
				/* FALL THRU */

			default:
				goto cmatch;
			}
		} else if (isspace(*fmt)) {		/* skip whitespace */
			skip();
		} else {			/* normal match char */
cmatch:
			if (c != *fmt) 
				break;
			c = (*get)(ip);
		}

		if (store)
			args++;

		if (!*++fmt)
			break;
	}

done:						/* end of scan */
	if ((c < 0) && (cnt == 0))
		return(EOF);

	(*unget)(c, ip);
	return(cnt);
}

/* wrapper code follows */

static char *p;

static int get(char *s)
{
	return *p++;
}

static int unget(char c,char *s)
{
	if (p > s) {
		*(--p) = c;
		return c;
	}
	return EOF;
}

static int vsscanf(char *ss,char *fmt,va_list args)
{
char *s;

	p = s = ss;
	return _scanf(ss,get,unget,fmt,(char **)args);
}
