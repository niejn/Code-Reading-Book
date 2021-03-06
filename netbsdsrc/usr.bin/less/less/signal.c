/*	$NetBSD: signal.c,v 1.1.1.3 1997/09/21 12:23:21 mrg Exp $	*/

/*
 * Copyright (c) 1984,1985,1989,1994,1995,1996  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Routines dealing with signals.
 *
 * A signal usually merely causes a bit to be set in the "signals" word.
 * At some convenient time, the mainline code checks to see if any
 * signals need processing by calling psignal().
 * If we happen to be reading from a file [in iread()] at the time
 * the signal is received, we call intread to interrupt the iread.
 */

#include "less.h"
#include <signal.h>

/*
 * "sigs" contains bits indicating signals which need to be processed.
 */
public int sigs;

extern int sc_width, sc_height;
extern int screen_trashed;
extern int lnloop;
extern int linenums;
extern int wscroll;
extern int reading;

/*
 * Interrupt signal handler.
 */
	/* ARGSUSED*/
	static RETSIGTYPE
u_interrupt(type)
	int type;
{
#if OS2
	LSIGNAL(SIGINT, SIG_ACK);
#endif
	LSIGNAL(SIGINT, u_interrupt);
	sigs |= S_INTERRUPT;
#if MSDOS_COMPILER==DJGPPC
	/*
	 * If a keyboard has been hit, it must be Ctrl-C
	 * (as opposed to Ctrl-Break), so consume it.
	 * (Otherwise, Less will beep when it sees Ctrl-C from keyboard.)
	 */
	if (kbhit())
		getkey();
#endif
	if (reading)
		intread();
}

#ifdef SIGTSTP
/*
 * "Stop" (^Z) signal handler.
 */
	/* ARGSUSED*/
	static RETSIGTYPE
stop(type)
	int type;
{
	LSIGNAL(SIGTSTP, stop);
	sigs |= S_STOP;
	if (reading)
		intread();
}
#endif

#ifdef SIGWINCH
/*
 * "Window" change handler
 */
	/* ARGSUSED*/
	public RETSIGTYPE
winch(type)
	int type;
{
	LSIGNAL(SIGWINCH, winch);
	sigs |= S_WINCH;
	if (reading)
		intread();
}
#else
#ifdef SIGWIND
/*
 * "Window" change handler
 */
	/* ARGSUSED*/
	public RETSIGTYPE
winch(type)
	int type;
{
	LSIGNAL(SIGWIND, winch);
	sigs |= S_WINCH;
	if (reading)
		intread();
}
#endif
#endif

#if MSDOS_COMPILER==WIN32C
/*
 * Handle CTRL-C and CTRL-BREAK keys.
 */
#include "windows.h"

	static BOOL WINAPI 
wbreak_handler(dwCtrlType)
	DWORD dwCtrlType;
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		sigs |= S_INTERRUPT;
		return (TRUE);
	default:
		break;
	}
	return (FALSE);
}
#endif

/*
 * Set up the signal handlers.
 */
	public void
init_signals(on)
	int on;
{
	if (on)
	{
		/*
		 * Set signal handlers.
		 */
		(void) LSIGNAL(SIGINT, u_interrupt);
#if MSDOS_COMPILER==WIN32C
		SetConsoleCtrlHandler(wbreak_handler, TRUE);
#endif
#ifdef SIGTSTP
		(void) LSIGNAL(SIGTSTP, stop);
#endif
#ifdef SIGWINCH
		(void) LSIGNAL(SIGWINCH, winch);
#else
#ifdef SIGWIND
		(void) LSIGNAL(SIGWIND, winch);
#endif
#endif
	} else
	{
		/*
		 * Restore signals to defaults.
		 */
		(void) LSIGNAL(SIGINT, SIG_DFL);
#if MSDOS_COMPILER==WIN32C
		SetConsoleCtrlHandler(wbreak_handler, FALSE);
#endif
#ifdef SIGTSTP
		(void) LSIGNAL(SIGTSTP, SIG_DFL);
#endif
#ifdef SIGWINCH
		(void) LSIGNAL(SIGWINCH, SIG_IGN);
#endif
#ifdef SIGWIND
		(void) LSIGNAL(SIGWIND, SIG_IGN);
#endif
	}
}

/*
 * Process any signals we have received.
 * A received signal cause a bit to be set in "sigs".
 */
	public void
psignals()
{
	register int tsignals;

	if ((tsignals = sigs) == 0)
		return;
	sigs = 0;

#ifdef SIGTSTP
	if (tsignals & S_STOP)
	{
		/*
		 * Clean up the terminal.
		 */
#ifdef SIGTTOU
		LSIGNAL(SIGTTOU, SIG_IGN);
#endif
		clear_bot();
		deinit();
		flush();
		raw_mode(0);
#ifdef SIGTTOU
		LSIGNAL(SIGTTOU, SIG_DFL);
#endif
		LSIGNAL(SIGTSTP, SIG_DFL);
		kill(getpid(), SIGTSTP);
		/*
		 * ... Bye bye. ...
		 * Hopefully we'll be back later and resume here...
		 * Reset the terminal and arrange to repaint the
		 * screen when we get back to the main command loop.
		 */
		LSIGNAL(SIGTSTP, stop);
		raw_mode(1);
		init();
		screen_trashed = 1;
		tsignals |= S_WINCH;
	}
#endif
#ifdef S_WINCH
	if (tsignals & S_WINCH)
	{
		int old_width, old_height;
		/*
		 * Re-execute scrsize() to read the new window size.
		 */
		old_width = sc_width;
		old_height = sc_height;
		get_term();
		if (sc_width != old_width || sc_height != old_height)
		{
			wscroll = (sc_height + 1) / 2;
			screen_trashed = 1;
		}
	}
#endif
	if (tsignals & S_INTERRUPT)
	{
		bell();
		/*
		 * {{ You may wish to replace the bell() with 
		 *    error("Interrupt", NULL_PARG); }}
		 */

		/*
		 * If we were interrupted while in the "calculating 
		 * line numbers" loop, turn off line numbers.
		 */
		if (lnloop)
		{
			lnloop = 0;
			if (linenums == 2)
				screen_trashed = 1;
			linenums = 0;
			error("Line numbers turned off", NULL_PARG);
		}

	}
}
