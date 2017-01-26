/*
 *	encrypt: simple one-way encryption for program protection
 *
 *	implemented functions:
 *		unsigned long encrypt(char *s,char *init)
 *			s		is the string to encrypt
 *			init	is a (locally) well-known string which is used to complicate
 *					the encryption mechanism
 *			returns encrypted value
 *
 *		void hex_to_external(char *s,unsigned long hex)
 *			s		is the (returned) external encryption string
 *			hex		is the internal encryption value
 *
 *		unsigned long external_to_hex(char *s)
 *			s		is the external encryption string
 *			returns the corresponding internal encryption value
 */
#include <math.h>
#include "encrypt.h"

#define ERROR_RETURN 0xffffffffUL

static char *convert = "ABCDEFGHKLMNRSTU";

static unsigned long string_to_ulong(char *p);

unsigned long encrypt(char *s,char *init)
{
int i, imax;
unsigned long randinit, rands;
unsigned long m, n;

	randinit = string_to_ulong(init);
	rands = string_to_ulong(s);

	/*
	 *	init the random number generator,
	 *	then call it a random number of times
	 *	(but make sure we call it at least once)
	 */
	srand48(randinit);
	imax = rands % 97;				/* a prime modulus */
	for (i = 0; i <= imax; i++)
		m = mrand48();

	srand48(rands);
	imax = randinit % 79;			/* a different prime modulus */
	for (i = 0; i <= imax; i++)
		n = mrand48();

	return m^n;
}

void hex_to_external(char *s,unsigned long hex)
{
int i, n;

	for (i = 0; i < 8; i++) {
		n = (hex >> ((7-i)*4)) & 0x0f;
		*s++ = convert[n];
	}
	*s = '\0';
}

unsigned long external_to_hex(char *s)
{
int i, d;
unsigned long n = 0UL;
char *p;

	for (i = 0; i < 8; i++, s++) {
		for (p = convert; *p; p++)
			if (*p == *s)
				break;
		d = p - convert;
		if (d > 0x0f)
			return ERROR_RETURN;
		n = (n<<4) | d;
	}
	if (*s)
		return ERROR_RETURN;

	return n;
}

static unsigned long string_to_ulong(char *p)
{
union {
	unsigned long ulseed;
	char cseed[4];
} temp;
int i;
unsigned long n = 0UL;

	while(*p) {
		for (i = 0; i < 4; i++) {
			temp.cseed[i] = *p ? (*p++&0x0f)<<4 : 0;
			temp.cseed[i] |= *p ? (*p++&0x0f) : 0;
		}
		n ^= temp.ulseed;
	}

	return n;
}
