/*	$NetBSD: disksubr.c,v 1.1 1997/10/14 06:47:34 sakamoto Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *
 *	@(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>

#define	b_cylin	b_resid

int fat_types[] = { DOSPTYP_FAT12, DOSPTYP_FAT16S,
		    DOSPTYP_FAT16B, DOSPTYP_FAT16C, -1 };

/*
 * Attempt to read a disk label from a device
 * using the indicated stategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 *
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Also, if bad block
 * table needed, attempt to extract it as well. Return buffer
 * for use in signalling errors if requested.
 *
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat) __P((struct buf *));
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct dos_partition *dp;
	struct partition *pp;
	struct dkbad *bdp;
	struct buf *bp;
	struct disklabel *dlp;
	char *msg = NULL;
	int dospartoff, cyl, i, *ip;

	/* minimal requirements for archtypal disk label */
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[i].p_size == 0)
		lp->d_partitions[i].p_size = 0x1fffffff;
	lp->d_partitions[i].p_offset = 0;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;
	if (osdep && (dp = osdep->dosparts) != NULL) {
		/* read master boot record */
		bp->b_blkno = DOSBBSECTOR;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = DOSBBSECTOR / lp->d_secpercyl;
		(*strat)(bp);

		/* if successful, wander through dos partition table */
		if (biowait(bp)) {
			msg = "dos partition I/O error";
			goto done;
		} else {
			/* XXX how do we check veracity/bounds of this? */
			bcopy(bp->b_data + DOSPARTOFF, dp,
			    NDOSPART * sizeof(*dp));
			for (i = 0; i < NDOSPART; i++, dp++) {
				/* Install in partition e, f, g, or h. */
				pp = &lp->d_partitions[RAW_PART + 1 + i];
				pp->p_offset = dp->dp_start;
				pp->p_size = dp->dp_size;
				for (ip = fat_types; *ip != -1; ip++) {
				    if (dp->dp_typ == *ip)
					pp->p_fstype = FS_MSDOS;
				}

				/* is this ours? */
				if (dp->dp_size && dp->dp_typ == DOSPTYP_386BSD
				    && dospartoff == 0) {
					/* need sector address for SCSI/IDE,
					   cylinder for ESDI/ST506/RLL */
					dospartoff = dp->dp_start;
					cyl = DPCYL(dp->dp_scyl, dp->dp_ssect);

					/* update disklabel with details */
					lp->d_partitions[0].p_size =
					    dp->dp_size;
					lp->d_partitions[0].p_offset = 
					    dp->dp_start;
					lp->d_ntracks = dp->dp_ehd + 1;
					lp->d_nsectors = DPSECT(dp->dp_esect);
					lp->d_secpercyl =
					    lp->d_ntracks * lp->d_nsectors;
				}
			}
			lp->d_npartitions = RAW_PART + 1 + i;
		}
	}
	
	/* next, dig out disk label */
	bp->b_blkno = dospartoff + LABELSECTOR;
	bp->b_cylin = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		msg = "disk label I/O error";
		goto done;
	}
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
			if (msg == NULL)
				msg = "no disk label";
		} else if (dlp->d_npartitions > MAXPARTITIONS ||
			   dkcksum(dlp) != 0)
			msg = "disk label corrupted";
		else {
			*lp = *dlp;
			msg = NULL;
			break;
		}
	}

	if (msg)
		goto done;

	/* obtain bad sector table if requested and present */
	if (osdep && (bdp = &osdep->bad) != NULL && (lp->d_flags & D_BADSECT)) {
		struct dkbad *db;

		i = 0;
		do {
			/* read a bad sector table */
			bp->b_flags = B_BUSY | B_READ;
			bp->b_blkno = lp->d_secperunit - lp->d_nsectors + i;
			if (lp->d_secsize > DEV_BSIZE)
				bp->b_blkno *= lp->d_secsize / DEV_BSIZE;
			else
				bp->b_blkno /= DEV_BSIZE / lp->d_secsize;
			bp->b_bcount = lp->d_secsize;
			bp->b_cylin = lp->d_ncylinders - 1;
			(*strat)(bp);

			/* if successful, validate, otherwise try another */
			if (biowait(bp)) {
				msg = "bad sector table I/O error";
			} else {
				db = (struct dkbad *)(bp->b_data);
#define DKBAD_MAGIC 0x4321
				if (db->bt_mbz == 0
					&& db->bt_flag == DKBAD_MAGIC) {
					msg = NULL;
					*bdp = *db;
					break;
				} else
					msg = "bad sector table corrupted";
			}
		} while ((bp->b_flags & B_ERROR) && (i += 2) < 10 &&
			i < lp->d_nsectors);
	}

done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return (msg);
}

/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	register struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	register i;
	register struct partition *opp, *npp;

	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0
		|| (nlp->d_secsize % DEV_BSIZE) != 0)
			return(EINVAL);

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);

	/* XXX missing check if other dos partitions will be overwritten */

	while (openmask != 0) {
		i = ffs(openmask) - 1;
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			npp->p_fstype = opp->p_fstype;
			npp->p_fsize = opp->p_fsize;
			npp->p_frag = opp->p_frag;
			npp->p_cpg = opp->p_cpg;
		}
	}
 	nlp->d_checksum = 0;
 	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;
	return (0);
}


/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat) __P((struct buf *));
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct dos_partition *dp;
	struct buf *bp;
	struct disklabel *dlp;
	int error, dospartoff, cyl, i;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;
	if (osdep && (dp = osdep->dosparts) != NULL) {
		/* read master boot record */
		bp->b_blkno = DOSBBSECTOR;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = DOSBBSECTOR / lp->d_secpercyl;
		(*strat)(bp);

		if ((error = biowait(bp)) == 0) {
			/* XXX how do we check veracity/bounds of this? */
			bcopy(bp->b_data + DOSPARTOFF, dp,
			    NDOSPART * sizeof(*dp));
			for (i = 0; i < NDOSPART; i++, dp++)
				/* is this ours? */
				if (dp->dp_size && dp->dp_typ == DOSPTYP_386BSD
				    && dospartoff == 0) {
					/* need sector address for SCSI/IDE,
					   cylinder for ESDI/ST506/RLL */
					dospartoff = dp->dp_start;
					cyl = DPCYL(dp->dp_scyl, dp->dp_ssect);
				}
		}
			
	}
	
#ifdef maybe
	/* disklabel in appropriate location? */
	if (lp->d_partitions[0].p_offset != 0
		&& lp->d_partitions[0].p_offset != dospartoff) {
		error = EXDEV;		
		goto done;
	}
#endif

	/* next, dig out disk label */
	bp->b_blkno = dospartoff + LABELSECTOR;
	bp->b_cylin = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if ((error = biowait(bp)) != 0)
		goto done;
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_flags = B_BUSY | B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}
	error = ESRCH;

done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return (error);
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	int wlabel;
{
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsector = lp->d_partitions[2].p_offset + LABELSECTOR;
	int sz;

	sz = howmany(bp->b_bcount, lp->d_secsize);

	if (bp->b_blkno + sz > p->p_size) {
		sz = p->p_size - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise, truncate request. */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* Overwriting disk label? */
	if (bp->b_blkno + p->p_offset <= labelsector &&
#if LABELSECTOR != 0
	    bp->b_blkno + p->p_offset + sz > labelsector &&
#endif
	    (bp->b_flags & B_READ) == 0 && !wlabel) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylin = (bp->b_blkno + p->p_offset) /
	    (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
done:
	return (0);
}

/*
 * This is called by main to set dumplo and dumpsize.
 */
void
cpu_dumpconf()
{
	int nblks;		/* size of dump device */
	int skip;
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = physmem;

	/* Skip enough blocks at start of disk to preserve an eventual disklabel. */
	skip = LABELSECTOR + 1;
	skip += ctod(1) - 1;
	skip = ctod(dtoc(skip));
	if (dumplo < skip)
		dumplo = skip;

	/* Put dump at end of partition */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

#if 0
/*	$NetBSD: disksubr.c,v 1.1 1997/10/14 06:47:34 sakamoto Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/systm.h>

static inline unsigned short get_short __P((void *p));
static inline unsigned long get_long __P((void *p));
static int get_netbsd_label __P((dev_t dev, void (*strat)(struct buf *),
				 struct disklabel *lp, daddr_t bno));
static int mbr_to_label __P((dev_t dev, void (*strat)(struct buf *),
			     daddr_t bno, struct disklabel *lp,
			     unsigned short *pnpart,
			     struct cpu_disklabel *osdep, daddr_t off));

/*
 * Little endian access routines
 */
static inline unsigned short
get_short(p)
	void *p;
{
	unsigned char *cp = p;

	return cp[0] | (cp[1] << 8);
}

static inline unsigned long
get_long(p)
	void *p;
{
	unsigned char *cp = p;

	return cp[0] | (cp[1] << 8) | (cp[2] << 16) | (cp[3] << 24);
}

/*
 * Get real NetBSD disk label
 */
static int
get_netbsd_label(dev, strat, lp, bno)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	daddr_t bno;
{
	struct buf *bp;
	struct disklabel *dlp;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* Now get the label block */
	bp->b_blkno = bno + LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	(*strat)(bp);

	if (biowait(bp))
		goto done;

	for (dlp = (struct disklabel *)bp->b_data;
	     dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize - sizeof (*dlp));
	     dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC
		    && dlp->d_magic2 == DISKMAGIC
		    && dlp->d_npartitions <= MAXPARTITIONS
		    && dkcksum(dlp) == 0) {
			*lp = *dlp;
			brelse(bp);
			return 1;
		}
	}
done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return 0;
}

/*
 * Construct disklabel entries from partition entries.
 */
static int
mbr_to_label(dev, strat, bno, lp, pnpart, osdep, off)
	dev_t dev;
	void (*strat)();
	daddr_t bno;
	struct disklabel *lp;
	unsigned short *pnpart;
	struct cpu_disklabel *osdep;
	daddr_t off;
{
	static int recursion = 0;
	struct mbr_partition *mp;
	struct partition *pp;
	struct buf *bp;
	int i, found = 0;

	/* Check for recursion overflow. */
	if (recursion > MAXPARTITIONS)
		return 0;

	/*
	 * Extended partitions seem to be relative to their first occurence?
	 */
	if (recursion++ == 1)
		off = bno;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* Now get the MBR */
	bp->b_blkno = bno;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	(*strat)(bp);

	if (biowait(bp))
		goto done;

	if (get_short(bp->b_data + MBRMAGICOFF) != MBRMAGIC)
		goto done;

	/* Extract info from MBR partition table */
	mp = (struct mbr_partition *)(bp->b_data + MBRPARTOFF);
	for (i = 0; i < NMBRPART; i++, mp++) {
		if (get_long(&mp->mbr_size)) {
			switch (mp->mbr_type) {
			case MBR_EXTENDED:
				if (*pnpart < MAXPARTITIONS) {
					pp = lp->d_partitions + *pnpart;
					bzero(pp, sizeof *pp);
					pp->p_size = get_long(&mp->mbr_size);
					pp->p_offset = off + get_long(&mp->mbr_start);
					++*pnpart;
				}
				if (found = mbr_to_label(dev, strat,
							 off + get_long(&mp->mbr_start),
							 lp, pnpart, osdep, off))
					goto done;
				break;
			case MBR_NETBSD:
				/* Found the real NetBSD partition, use it */
				osdep->cd_start = off + get_long(&mp->mbr_start);
				if (found = get_netbsd_label(dev, strat, lp, osdep->cd_start))
					goto done;
				/* FALLTHROUGH */
			default:
				if (*pnpart < MAXPARTITIONS) {
					pp = lp->d_partitions + *pnpart;
					bzero(pp, sizeof *pp);
					pp->p_size = get_long(&mp->mbr_size);
					pp->p_offset = off + get_long(&mp->mbr_start);
					++*pnpart;
				}
				break;
			}
		}
	}
done:
	recursion--;
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return found;
}

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 *
 * If we can't find a NetBSD label, we attempt to fake one
 * based on the MBR (and extended partition) information
 */
char *
readdisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct mbr_partition *mp;
	struct buf *bp;
	char *msg = 0;
	int i;

	/* Initialize disk label with some defaults */
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = 1;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x7fffffff;
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < MAXPARTITIONS; i++) {
		if (i != RAW_PART) {
			lp->d_partitions[i].p_size = 0;
			lp->d_partitions[i].p_offset = 0;
		}
	}
	if (lp->d_partitions[RAW_PART].p_size == 0) {
		lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
		lp->d_partitions[RAW_PART].p_offset = 0;
	}

	osdep->cd_start = -1;

	mbr_to_label(dev, strat, MBRSECTOR, lp, &lp->d_npartitions, osdep, 0);
	return 0;
}

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0
	    || (nlp->d_secsize % DEV_BSIZE) != 0)
		return EINVAL;

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return 0;
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC
	    || dkcksum(nlp) != 0)
		return EINVAL;

	/* openmask parameter ignored */

	*olp = *nlp;
	return 0;
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct buf *bp;
	int error;
	struct disklabel label;

	/*
	 * Try to re-read a disklabel, in case he changed the MBR.
	 */
	label = *lp;
	readdisklabel(dev, strat, &label, osdep);
	if (osdep->cd_start < 0)
		return EINVAL;

	/* get a buffer and initialize it */
	bp = geteblk(lp->d_secsize);
	bp->b_dev = dev;

	bp->b_blkno = osdep->cd_start + LABELSECTOR;
	bp->b_cylinder = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_WRITE;

	bcopy((caddr_t)lp, (caddr_t)bp->b_data, sizeof *lp);

	(*strat)(bp);
	error = biowait(bp);

	bp->b_flags |= B_INVAL;
	brelse(bp);

	return error;
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaris of the partition.  Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	int wlabel;
{
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int sz;

	sz = howmany(bp->b_bcount, lp->d_secsize);

	if (bp->b_blkno + sz > p->p_size) {
		sz = p->p_size - bp->b_blkno;
		if (sz == 0) {
			/* If axactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise truncate request. */
		bp->b_bcount = sz * lp->d_secsize;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + p->p_offset)
			 / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;

	return 1;

bad:
	bp->b_flags |= B_ERROR;
done:
	return 0;
}
#endif 0
