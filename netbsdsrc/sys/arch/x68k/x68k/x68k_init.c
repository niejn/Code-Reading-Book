/*	$NetBSD: x68k_init.c,v 1.4 1997/10/11 11:13:54 oki Exp $	*/

/*
 * Copyright (c) 1996 Masaru Oki.
 * All rights reserved.
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
 *      This product includes software developed by Masaru Oki.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>

#include <x68k/x68k/iodevice.h>
#define zschan IODEVbase->io_inscc.zs_chan

volatile struct IODEVICE *IODEVbase = (volatile struct IODEVICE *) PHYS_IODEV;

void intr_reset __P((void));

/*
 * disable all interrupt.
 */
void
intr_reset()
{
	/* I/O Controller */
	ioctlr.intr = 0;

	/* Internal RS-232C port */
	zschan[1].zc_csr = 1;
	asm("nop");
	zschan[1].zc_csr = 0;
	asm("nop");

	/* mouse */
	zschan[0].zc_csr = 1;
	asm("nop");
	zschan[0].zc_csr = 0;
	asm("nop");
	while(!(mfp.tsr & MFP_TSR_BE))
		;
	mfp.udr = 0x41;

	/* MFP (hard coded interrupt vector XXX) */
	mfp.vr = 0x40;
}
