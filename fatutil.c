/*
 *	fatutil.c: FAT helpers shared by Obsidian Edge and Obsidian Mirror
 *
 *	Copyright (C) 2026 the Obsidian Edge contributors
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

#include <portab.h>

void fat_normalize_eoc(unsigned short *f16,int maxc,int isfat16);

/*
 *	end of chain: TOS writes $FFFF ($FFF in a 12-bit FAT), but Microsoft's
 *	specification allows any value from $FFF8 to $FFFF ($FF8 to $FFF), and
 *	other systems may write them.  The FAT code of Edge only recognises
 *	dpb.bitmask ($FFFF or $FFF), so the other end-of-chain values of the
 *	in-memory FAT (big-endian, one 16-bit word per cluster, clusters 2 to
 *	maxc-1) are changed into it when the FAT is loaded.  Entries 0 and 1
 *	(media byte) are left alone.
 */
void fat_normalize_eoc(unsigned short *f16,int maxc,int isfat16)
{
unsigned short first, eoc;
int i;

	first = isfat16 ? 0xfff8 : 0x0ff8;
	eoc = isfat16 ? 0xffff : 0x0fff;
	for (i = 2; i < maxc; i++)
		if ((f16[i] >= first) && (f16[i] < eoc))
			f16[i] = eoc;
}
