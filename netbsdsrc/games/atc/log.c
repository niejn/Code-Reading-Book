/*	$NetBSD: log.c,v 1.6 1997/10/11 02:01:02 lukem Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ed James.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1987 by Ed James, UC Berkeley.  All rights reserved.
 *
 * Copy permission is hereby granted provided that this notice is
 * retained on all partial or complete copies.
 *
 * For more info on this and all of my stuff, mail edjames@berkeley.edu.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)log.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: log.c,v 1.6 1997/10/11 02:01:02 lukem Exp $");
#endif
#endif not lint

#include "include.h"
#include "pathnames.h"

int
compar(va, vb)
	const void *va, *vb;
{
	SCORE	*a, *b;

	a = (SCORE *)va;
	b = (SCORE *)vb;
	if (b->planes == a->planes)
		return (b->time - a->time);
	else
		return (b->planes - a->planes);
}

#define SECAMIN		60
#define MINAHOUR	60
#define HOURADAY	24
#define SECAHOUR	(SECAMIN * MINAHOUR)
#define SECADAY		(SECAHOUR * HOURADAY)
#define DAY(t)		((t) / SECADAY)
#define HOUR(t)		(((t) % SECADAY) / SECAHOUR)
#define MIN(t)		(((t) % SECAHOUR) / SECAMIN)
#define SEC(t)		((t) % SECAMIN)

char	*
timestr(t)
	int t;
{
	static char	s[80];

	if (DAY(t) > 0)
		(void)sprintf(s, "%dd+%02dhrs", DAY(t), HOUR(t));
	else if (HOUR(t) > 0)
		(void)sprintf(s, "%d:%02d:%02d", HOUR(t), MIN(t), SEC(t));
	else if (MIN(t) > 0)
		(void)sprintf(s, "%d:%02d", MIN(t), SEC(t));
	else if (SEC(t) > 0)
		(void)sprintf(s, ":%02d", SEC(t));
	else
		*s = '\0';

	return (s);
}

int
log_score(list_em)
	int list_em;
{
	int		i, fd, num_scores = 0, good, changed = 0, found = 0;
	struct passwd	*pw;
	FILE		*fp;
	char		*cp;
	SCORE		score[100], thisscore;
	struct utsname	name;

	umask(0);
	fd = open(_PATH_SCORE, O_CREAT|O_RDWR, 0644);
	if (fd < 0) {
		warn("open %s", _PATH_SCORE);
		return (-1);
	}
	/*
	 * This is done to take advantage of stdio, while still 
	 * allowing a O_CREAT during the open(2) of the log file.
	 */
	fp = fdopen(fd, "r+");
	if (fp == NULL) {
		warn("fdopen %s", _PATH_SCORE);
		return (-1);
	}
#ifdef BSD
	if (flock(fileno(fp), LOCK_EX) < 0)
#endif
#ifdef SYSV
	while (lockf(fileno(fp), F_LOCK, 1) < 0)
#endif
	{
		warn("flock %s", _PATH_SCORE);
		return (-1);
	}
	for (;;) {
		good = fscanf(fp, "%s %s %s %d %d %d",
			score[num_scores].name, 
			score[num_scores].host, 
			score[num_scores].game,
			&score[num_scores].planes, 
			&score[num_scores].time,
			&score[num_scores].real_time);
		if (good != 6 || ++num_scores >= NUM_SCORES)
			break;
	}
	if (!test_mode && !list_em) {
		if ((pw = (struct passwd *) getpwuid(getuid())) == NULL) {
			fprintf(stderr, 
				"getpwuid failed for uid %d.  Who are you?\n",
				getuid());
			return (-1);
		}
		strcpy(thisscore.name, pw->pw_name);
		uname(&name);
		strncpy(thisscore.host, name.sysname, sizeof(thisscore.host)-1);
		thisscore.host[sizeof(thisscore.host) - 1] = '\0';

		cp = strrchr(file, '/');
		if (cp == NULL) {
			fprintf(stderr, "log: where's the '/' in %s?\n", file);
			return (-1);
		}
		cp++;
		strcpy(thisscore.game, cp);

		thisscore.time = clck;
		thisscore.planes = safe_planes;
		thisscore.real_time = time(0) - start_time;

		for (i = 0; i < num_scores; i++) {
			if (strcmp(thisscore.name, score[i].name) == 0 &&
			    strcmp(thisscore.host, score[i].host) == 0 &&
			    strcmp(thisscore.game, score[i].game) == 0) {
				if (thisscore.time > score[i].time) {
					score[i].time = thisscore.time;
					score[i].planes = thisscore.planes;
					score[i].real_time =
						thisscore.real_time;
					changed++;
				}
				found++;
				break;
			}
		}
		if (!found) {
			for (i = 0; i < num_scores; i++) {
				if (thisscore.time > score[i].time) {
					if (num_scores < NUM_SCORES)
						num_scores++;
					memcpy(&score[num_scores - 1],
					       &score[i],
					       sizeof (score[i]));
					memcpy(&score[i], &thisscore,
					       sizeof (score[i]));
					changed++;
					break;
				}
			}
		}
		if (!found && !changed && num_scores < NUM_SCORES) {
			memcpy(&score[num_scores], &thisscore,
			       sizeof (score[num_scores]));
			num_scores++;
			changed++;
		}

		if (changed) {
			if (found)
				puts("You beat your previous score!");
			else
				puts("You made the top players list!");
			qsort(score, num_scores, sizeof (*score), compar);
			rewind(fp);
			for (i = 0; i < num_scores; i++)
				fprintf(fp, "%s %s %s %d %d %d\n",
					score[i].name, score[i].host, 
					score[i].game, score[i].planes,
					score[i].time, score[i].real_time);
		} else {
			if (found)
				puts("You didn't beat your previous score.");
			else
				puts("You didn't make the top players list.");
		}
		putchar('\n');
	}
#ifdef BSD
	flock(fileno(fp), LOCK_UN);
#endif
#ifdef SYSV
	/* lock will evaporate upon close */
#endif
	fclose(fp);
	printf("%2s:  %-8s  %-8s  %-18s  %4s  %9s  %4s\n", "#", "name", "host", 
		"game", "time", "real time", "planes safe");
	puts("-------------------------------------------------------------------------------");
	for (i = 0; i < num_scores; i++) {
		cp = strchr(score[i].host, '.');
		if (cp != NULL)
			*cp = '\0';
		printf("%2d:  %-8s  %-8s  %-18s  %4d  %9s  %4d\n", i + 1,
			score[i].name, score[i].host, score[i].game,
			score[i].time, timestr(score[i].real_time),
			score[i].planes);
	}
	putchar('\n');
	return (0);
}

void
log_score_quit(dummy)
	int dummy;
{
	(void)log_score(0);
	exit(0);
}
