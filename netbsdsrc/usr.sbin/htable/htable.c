/*
 * Copyright (c) 1983 Regents of the University of California.
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)htable.c	5.10 (Berkeley) 2/6/91";
#else
__RCSID("$NetBSD: htable.c,v 1.4 1997/10/17 08:00:38 lukem Exp $");
#endif
#endif /* not lint */

/*
 * htable - convert NIC host table into a UNIX format.
 * NIC format is described in RFC 810, 1 March 1982.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htable.h"		/* includes <sys/types.h> */

#define	DATELINES	3	/* these lines usually contain the date */
#define	MAXNETS		30	/* array size for local, connected nets */

FILE	*hf;			/* hosts file */
FILE	*gf;			/* gateways file */
FILE	*nf;			/* networks file */

int connected_nets[MAXNETS];
int nconnected;
int local_nets[MAXNETS];
int nlocal;

int	addlocal __P((char *, int *));
int	connectedto __P((u_int32_t));
void	copycomments __P((FILE *, FILE *, int));
void	copygateways __P((char *));
void	copylocal __P((FILE *, char *));
void	dogateways __P((void));
void	freeaddrs __P((struct addr *));
void	freenames __P((struct name *));
struct gateway *gatewayto __P((int));
int	gethostaddr __P((char *, u_int32_t *));
int	getnetaddr __P((char *, int *));
int	local __P((u_int32_t));
char   *lower __P((char *));
int	main __P((int, char **));
void	printgateway __P((int, char *, int));
void	putaddr __P((FILE *, struct in_addr));
void	putnet __P((FILE *, int));
struct gateway *savegateway __P((struct name *, int, struct in_addr, int));
void	usage __P((void));
int	yyparse __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int errs;

	infile = "(stdin)";
	argc--;
	argv++;
	while (argc--) {
		if (*argv[0] == '-') {
			switch (argv[0][1]) {
			case 'c':
				nconnected = addlocal(argv[1], connected_nets);
				argv++;
				argc--;
				break;
			case 'l':
				nlocal = addlocal(argv[1], local_nets);
				argv++;
				argc--;
				break;
			default:
				usage();
				/*NOTREACHED*/
			}
		} else {
			infile = argv[0];
			if (freopen(infile, "r", stdin) == NULL)
				err(1, "reopen `%s'", infile);
		}
		argv++;
	}
	hf = fopen("hosts", "w");
	if (hf == NULL)
		err(1, "hosts");
	copylocal(hf, "localhosts");
	gf = fopen("gateways", "w");
	if (gf == NULL)
		err(1 ,"gateways");
	copygateways("localgateways");
	nf = fopen("networks", "w");
	if (nf == NULL)
		err(1, "networks");
	copylocal(nf, "localnetworks");
	copycomments(stdin, hf, DATELINES);
	errs = yyparse();
	dogateways();
	exit(errs);
}

void
usage()
{
	extern char *__progname;

	fprintf(stderr,
	"usage: %s [ -c connected-nets ] [-l local-nets ] [ input-file ]\n",
		__progname);
	exit(1);
}

/*
 *  Turn a comma-separated list of network names or numbers in dot notation
 *  (e.g.  "arpanet, 128.32") into an array of net numbers.
 */
int
addlocal(arg, nets)
	char *arg;
	int *nets;
{
	char *p, c;
	int nfound = 0;

	do {
		p = arg;
		while (*p && *p != ',' && !isspace(*p))
			p++;
		c = *p;
		*p = 0;
		while (*arg && isspace(*arg))
			arg++;
		if (*arg == 0)
			continue;
		if (nfound == MAXNETS) {
			warnx("Too many networks in list");
			return (nfound);
		}
		if (getnetaddr(arg, &nets[nfound]))
			nfound++;
		else {
			warnx("%s: unknown network", arg);
			exit(1);
		}
		arg = p + 1;
	} while (c);
	return (nfound);
}

struct name *
newname(str)
	char *str;
{
	char *p;
	struct name *nm;

	p = malloc(strlen(str) + 1);
	strcpy(p, str);
	nm = (struct name *)malloc(sizeof (struct name));
	nm->name_val = p;
	nm->name_link = NONAME;
	return (nm);
}

char *
lower(str)
	char *str;
{
	char *cp = str;

	while (*cp) {
		if (isupper(*cp))
			*cp = tolower(*cp);
		cp++;
	}
	return (str);
}

void
do_entry(keyword, addrlist, namelist, cputype, opsys, protos)
	int keyword;
	struct addr *addrlist;
	struct name *namelist, *cputype, *opsys, *protos;
{
	struct addr *al, *al2;
	struct name *nl;
	struct addr *connect_addr;
	char *cp;

	switch (keyword) {

	case KW_NET:
		nl = namelist;
		if (nl == NONAME) {
			fprintf(stderr, "htable: net");
			putnet(stderr, inet_netof(addrlist->addr_val));
			fprintf(stderr, " missing names.\n");
			break;
		}
		fprintf(nf, "%-16.16s", lower(nl->name_val));
		al2 = addrlist;
		while ((al = al2) != NULL) {
			char *cp;

			putnet(nf, inet_netof(al->addr_val));
			cp = "\t%s";
			while ((nl = nl->name_link) != NULL) {
				fprintf(nf, cp, lower(nl->name_val));
				cp = " %s";
			}
			putc('\n', nf);
			al2 = al->addr_link;
			free((char *)al);
		}
		break;

	case KW_GATEWAY:
		/* locate locally connected address, if one */
		for (al = addrlist; al; al = al->addr_link)
			if (connectedto(inet_netof(al->addr_val)))
				break;
		if (al == NULL) {
			/*
			 * Not connected to known networks.  Save for later.
			 */
			struct gateway *gw, *firstgw = (struct gateway *) NULL;

			for (al = addrlist; al; al = al->addr_link) {
				int net;

				net = inet_netof(al->addr_val);
				gw = savegateway(namelist, net,
				    al->addr_val, 0);
				if (firstgw == (struct gateway *) NULL)
					firstgw = gw;
				gw->g_firstent = firstgw;
			}
			freeaddrs(addrlist);
			goto dontfree;
		}
		/*
		 * Connected to a known network.
		 * Mark this as the gateway to all other networks
		 * that are on the addrlist (unless we already have
		 * gateways to them).
		 */
		connect_addr = al;
		for (al = addrlist; al; al = al->addr_link) {
			int net;

			/* suppress duplicates -- not optimal */
			net = inet_netof(al->addr_val);
			if (connectedto(net) || gatewayto(net))
				continue;
			printgateway(net, namelist->name_val, 1);
			(void) savegateway(namelist, net, al->addr_val, 1);
		}
		/*
		 * Put the gateway in the hosts file.
		 */
		putaddr(hf, connect_addr->addr_val);
		cp = "%s";
		for (nl = namelist; nl; nl = nl->name_link) {
			fprintf(hf, cp, lower(nl->name_val));
			cp = " %s";
		}
		fprintf(hf, "\t# gateway\n");
		freeaddrs(addrlist);
		goto dontfree;

	case KW_HOST:
		al2 = addrlist;
		while ((al = al2) != NULL) {
			if (!local(inet_netof(al->addr_val))) {
				char *cp;

				putaddr(hf, al->addr_val);
				cp = "%s";
				for (nl = namelist; nl; nl = nl->name_link) {
					fprintf(hf, cp, lower(nl->name_val));
					cp = " %s";
				}
				putc('\n', hf);
			}
			al2 = al->addr_link;
			free((char *)al);
		}
		break;

	default:
		warnx("Unknown keyword: %d.", keyword);
	}
	freenames(namelist);
dontfree:
	freenames(protos);
}

void
printgateway(net, name, metric)
	int net;
	char *name;
	int metric;
{
	struct netent *np;

	fprintf(gf, "net ");
	np = getnetbyaddr(net, AF_INET);
	if (np)
		fprintf(gf, "%s", np->n_name);
	else
		putnet(gf, net);
	fprintf(gf, " gateway %s metric %d passive\n",
		lower(name), metric);
}

void
copylocal(f, filename)
	FILE *f;
	char *filename;
{
	FILE *lhf;
	int cc;
	char buf[BUFSIZ];

	lhf = fopen(filename, "r");
	if (lhf == NULL) {
		if (errno != ENOENT)
			err(1, "open `%s'", filename);
		warnx("Warning, no %s file.", filename);
		return;
	}
	while ((cc = fread(buf, 1, sizeof(buf), lhf)) > 0)
		fwrite(buf, 1, cc, f);
	fclose(lhf);
}

void
copygateways(filename)
	char *filename;
{
	FILE *lhf;
	struct name *nl;
	char type[80];
	char dname[80];
	char gname[80];
	char junk[80];
	char buf[500];
	struct in_addr addr;
	int net, metric;

	lhf = fopen(filename, "r");
	if (lhf == NULL) {
		if (errno != ENOENT)
			err(1, "open `%s'", filename);
		warnx("Warning, no %s file.", filename);
		return;
	}
	/* format: {net | host} XX gateway XX metric DD [passive]\n */
	for (;;) {
		junk[0] = 0;
		if (fgets(buf, sizeof(buf), lhf) == (char *)NULL)
			break;
		fputs(buf, gf);
		if (buf[0] == '#' ||
		    sscanf(buf, "%s %s gateway %s metric %d %s",
		    type, dname, gname, &metric, junk) < 5)
			continue;
		if (strcmp(type, "net"))
			continue;
		if (!getnetaddr(dname, &net))
			continue;
		if (!gethostaddr(gname, &addr.s_addr))
			continue;
		nl = newname(gname);
		(void) savegateway(nl, net, addr, metric);
	}
	fclose(lhf);
}

int
getnetaddr(name, addr)
	char *name;
	int *addr;
{
	struct netent *np = getnetbyname(name);

	if (np == 0) {
		*addr = inet_network(name);
		return (*addr != -1);
	} else {
		if (np->n_addrtype != AF_INET)
			return (0);
		*addr = np->n_net;
		return (1);
	}
}

int
gethostaddr(name, addr)
	char *name;
	u_int32_t *addr;
{
	struct hostent *hp;

	hp = gethostbyname(name);
	if (hp) {
		*addr = *(u_int32_t *)(hp->h_addr);
		return (1);
	}
	*addr = inet_addr(name);
	return (*addr != -1);
}

void
copycomments(in, out, ccount)
	FILE *in, *out;
	int ccount;
{
	int count;
	char buf[BUFSIZ];

	for (count=0; count < ccount; count++) {
		if ((fgets(buf, sizeof(buf), in) == NULL) || (buf[0] != ';'))
			return;
		buf[0] = '#';
		fputs(buf, out);
	}
}

#define	UC(b)	(((int)(b))&0xff)

/*
 * Print network number in internet-standard dot notation;
 * v is in host byte order.
 */
void
putnet(f, v)
	FILE *f;
	int v;
{
	if (v < 128)
		fprintf(f, "%d", v);
	else if (v < 65536)
		fprintf(f, "%d.%d", UC(v >> 8), UC(v));
	else
		fprintf(f, "%d.%d.%d", UC(v >> 16), UC(v >> 8), UC(v));
}

void
putaddr(f, v)
	FILE *f;
	struct in_addr v;
{
	fprintf(f, "%-16.16s", inet_ntoa(v));
}

void
freenames(list)
	struct name *list;
{
	struct name *nl, *nl2;

	nl2 = list;
	while ((nl = nl2) != NULL) {
		nl2 = nl->name_link;
		free(nl->name_val);
		free((char *)nl);
	}
}

void
freeaddrs(list)
	struct addr *list;
{
	struct addr *al, *al2;

	al2 = list;
	while ((al = al2) != NULL)
		al2 = al->addr_link, free((char *)al);
}

struct gateway *gateways = 0;
struct gateway *lastgateway = 0;

struct gateway *
gatewayto(net)
	int net;
{
	struct gateway *gp;

	for (gp = gateways; gp; gp = gp->g_link)
		if ((gp->g_net == net) && (gp->g_metric > 0))
			return (gp);
	return ((struct gateway *) NULL);
}

struct gateway *
savegateway(namelist, net, addr, metric)
	struct name *namelist;
	int net;
	struct in_addr addr;
	int metric;
{
	struct gateway *gp;

	gp = (struct gateway *)malloc(sizeof (struct gateway));
	if (gp == 0)
		errx(1, "out of memory");
	gp->g_link = (struct gateway *) NULL;
	if (lastgateway)
		lastgateway->g_link = gp;
	else
		gateways = gp;
	lastgateway = gp;
	gp->g_name = namelist;
	gp->g_net = net;
	gp->g_addr = addr;
	gp->g_metric = metric;
	if (metric == 1)
		gp->g_dst = gp;
	return (gp);
}

int
connectedto(net)
	u_int32_t net;
{
	int i;

	for (i = 0; i < nconnected; i++)
		if (connected_nets[i] == net)
			return(1);
	return(0);
}

int
local(net)
	u_int32_t net;
{
	int i;

	for (i = 0; i < nlocal; i++)
		if (local_nets[i] == net)
			return(1);
	return(0);
}

#define	MAXHOPS	10

/*
 * Go through list of gateways, finding connections for gateways
 * that are not yet connected.
 */
void
dogateways()
{
	struct gateway *gp, *gw, *ggp;
	int hops, changed = 1;
	struct name *nl;
	char *cp;

	for (hops = 0; hops < MAXHOPS && changed; hops++, changed = 0) {
	    for (gp = gateways; gp; gp = gp->g_link)
		if ((gp->g_metric == 0) && (gw = gatewayto(gp->g_net))) {
		    /*
		     * Found a new connection.
		     * For each other network that this gateway is on,
		     * add a new gateway to that network.
		     */
		    changed = 1;
		    gp->g_dst = gw->g_dst;
		    gp->g_metric = gw->g_metric + 1;
		    for (ggp = gp->g_firstent; ggp->g_name == gp->g_name;
			ggp = ggp->g_link) {
			    if (ggp == gp)
				continue;
			    if (gatewayto(ggp->g_net))
				continue;
			    ggp->g_dst = gp->g_dst;
			    ggp->g_metric = gp->g_metric;
			    printgateway(ggp->g_net,
				    gw->g_dst->g_name->name_val, gp->g_metric);
		    }
		    /*
		     * Put the gateway in the hosts file,
		     * using the address for the connected net.
		     */
		    putaddr(hf, gp->g_addr);
		    cp = "%s";
		    for (nl = gp->g_name; nl; nl = nl->name_link) {
			    fprintf(hf, cp, lower(nl->name_val));
			    cp = " %s";
		    }
		    fprintf(hf, "\t# gateway\n");
		}
	}
}