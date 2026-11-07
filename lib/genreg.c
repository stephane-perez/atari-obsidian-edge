/*
 *	genreg.c: generate registration code for Diamond Edge
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

/*
 *	version 2.57	roger burrows (january/2017)
 *		prepare for public release
 *
 *	Note: this is no longer needed, but may be of historical interest
 */
#include <stdio.h>
#include "encrypt.h"

int main(int argc,char **argv)
{
unsigned long n;
char sernum[20], regcode[20];

	printf("GENREG: generates registration codes for Diamond Edge\n");
	while(1) {
		printf("Enter serial number: ");
		gets(sernum);
		if (sernum[0] == '\0')
			break;
		n = encrypt(sernum,"EDGE.INF");
		hex_to_external(regcode,n);
		printf("Serial number %s: registration code %s (%08lx)\n",sernum,regcode,n);
	}

	return 0;
}
