/*
 *	genreg.c: generate registration code for Diamond Edge
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
