/*	$NetBSD: idprom.c,v 1.1 1997/10/13 21:57:58 gwr Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine ID PROM - system type and serial number
 */

#include <sys/types.h>
#include <machine/idprom.h>
#include <machine/mon.h>
#include "stand.h"

static int idprom_cksum __P((u_char *));

/*
 * This structure is what this driver is all about.
 * It is copied from the device early in startup.
 */
struct idprom identity_prom;

/*
 * This is called during startup to fetch a copy of the idprom.
 * Rather than do all the map-in/probe work to find the idprom,
 * we can cheat!  We _know_ the monitor already made of copy of
 * the IDPROM in its data page.  All we have to do is find it.
 */
void
idprom_init()
{
	u_char *p;
	int x;

	/* Search for it.  Range determined empirically. */
	for (p = (u_char *)(MONDATA + 0x0400);
	     p < (u_char *)(MONDATA + 0x1c00); p++)
	{
		/* first some quick checks */
		if (p[0] != 1)
			continue;
		if ((p[1] & 0xfc) != 0x40)
			continue;
		if (p[2] != 8)
			continue;
		if (p[3] != 0)
			continue;
		/* Looks plausible.  Try the checksum. */
		x = idprom_cksum(p);
		if (x == 0)
			goto found;
	}
	panic("idprom: not found in monitor data\n");

found:
	printf("idprom: copy found at 0x%x\n", (int)p);
	bcopy(p, &identity_prom, sizeof(struct idprom));
}

static int
idprom_cksum(p)
	u_char *p;
{
	int len, x;

	len = IDPROM_SIZE;
	x = 0;	/* calculated as xor of data */
	do x ^= *p++;
	while (--len > 0);
	return (x);
}

void
idprom_etheraddr(eaddrp)
	u_char *eaddrp;
{

	if (identity_prom.idp_format == 0)
		idprom_init();

	bcopy(identity_prom.idp_etheraddr, eaddrp, 6);
}
