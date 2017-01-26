/*
 *	protection module for Diamond Edge
 *
 *	the scheme is as follows:
 *	 1. we supply the user with a serial number and a registration code.
 *		the registration code is an 8 hex digit number (actually an
 *		unsigned long) generated from the serial number via the encrypt()
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
#ifndef BETA
MLOCAL struct pgm_protect prot = { PROTECT_MAGIC1, 0UL, 0UL, PROTECT_MAGIC2 };
#endif

int protect()
{
#ifdef BETA
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
