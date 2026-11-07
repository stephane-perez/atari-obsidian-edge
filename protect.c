/*
 *	protection module for Diamond Edge
 *
 *	the scheme is as follows:
 *	 1. we supply the user with a serial number and a registration code.
 *		the registration code is an 8-character string, derived by simple
 *		substitution from an 8 hex-digit number.  the latter is actually an
 *		unsigned long generated from the serial number via the encrypt()
 *		function.
 *	 2. when installing, the user enters the serial number & registration
 *		code, plus his name.  the install program modifies the RSC to
 *		contain the name & serial number in plaintext, and the PRG to
 *		contain the registration number (i.e. encrypted serial number)
 *		and the encrypted user name.
 *	 3.	this code checks that:
 *		. the registration code matches the serial number (prevent serial
 *		  number modification)
 *		. the encrypted name matches the username (prevent username
 *		  modification)
 *
 *	if we find an error, we return -1 & let the caller figure out what to do
 *
 *	NOTE: as of v2.57, this is moot because the installation program fudges
 *	things.
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 *
 *	Modified for Obsidian Edge (November 2026): GCC port: the protection structure is volatile
 */

/*
 *	version 2.50 (april/2003)			roger burrows
 *		original version
 *
 *	version 2.54 (august/2003)			roger burrows
 *		. add DEMO support (no protection)
 *
 *	version 2.57 (january/2017)			roger burrows
 *		prepare for public release
 */ 
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <dos.h>
#include <math.h>
#include <portab.h>
#include <string.h>

#include "edge.h"
#include "edgedefs.h"
#include "proto.h"
#include "version.h"
#include "lib/encrypt.h"

#define MODULE_NUMBER	12		/* used by LOCATION macro */

extern OBJECT *about;

/*
 *	return pointer to text of TEDINFO object
 */
#define	gettext(tree,which)		((char *)(((TEDINFO *)tree[which].ob_spec)->te_ptext))

/*
 *	private variables
 */
#if !defined(BETA) && !defined(DEMO)
#ifdef __GNUC__
/*
 *	INSTALL.PRG finds this structure in the data segment of EDGE.PRG and
 *	writes the registration data into it: GCC must keep it, and must not
 *	replace its fields by their initial values
 */
MLOCAL volatile struct pgm_protect prot = { PROTECT_MAGIC1, 0UL, 0UL, PROTECT_MAGIC2 };
#else
MLOCAL struct pgm_protect prot = { PROTECT_MAGIC1, 0UL, 0UL, PROTECT_MAGIC2 };
#endif
#endif

int protect()
{
#if defined(DEMO)
	form_alert(1,"[1][Diamond Edge demo version.|May be distributed freely.|To purchase the full version, |please contact Anodyne Software|(sales@anodynesoftware.com)][ OK ]");
	strcpy(gettext(about,SERNUM),"0000000000");
	strcpy(gettext(about,USERNAME),"Demo version");
#elif defined(BETA)
	if (Tgetdate() > EXPIRY_DATE) {
		form_alert(1,"[1][This beta test version|has expired!][ OK ]");
		return -1;
	}
	form_alert(1,"[1][This is a beta test version.|Please do not distribute!][ OK ]");
#else
	if (encrypt(gettext(about,SERNUM),PREFS_FILE) != prot.registration_code)
		return -1;
	if (encrypt(gettext(about,USERNAME),PREFS_FILE) != prot.encrypted_name)
		return -1;
#endif

	return 0;
}
