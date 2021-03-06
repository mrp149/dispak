/*
 * Decoding output buffer of native extracode e64.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this program and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "defs.h"
#include "disk.h"
#include "gost10859.h"
#include "encoding.h"

#define PARASZ  (256 * 6)

static unsigned char	para[PARASZ];
static uchar		line[129];
static int		pos;
static int		maxp;
static int		done;
static int		bytes_total, bytes_tail;
static unsigned char	lastc;

static void
rstline(void)
{
	memset(line, GOST_SPACE, 128);
	line[128] = 0;
	pos = 0;
	maxp = -1;
}

static void
put(FILE * fout, uchar c)
{
	if (line[pos] != GOST_SPACE) {
		int saved = pos;
		if (trace_e64)
			putchar('\n');
		gost_write(line, maxp+1, fout);
		rstline();
		pos = saved;
		fputs("\\\n", fout);
	}
	if (pos > maxp)
		maxp = pos;
	line[pos++] = (c);
}

static void
dump(FILE *fout, unsigned sz)
{
	unsigned char   *cp, rc;

	for (cp = para + 12; cp - para < sz; ++cp) {
		if (*cp && trace_e64) {
			printf("%03o-", *cp <= 0140 ? *cp-1 : *cp);
		}
		switch (*cp) {
		case 0177:
			if (trace_e64)
				printf("%03o-", cp[1]);
			for (rc = *++cp; rc; --rc)
				put(fout, lastc);
			continue;
		case 0174:
		case 0175:
		case 0:
			if (trace_e64)
				putchar('\n');
			return;
		}
		if (*cp & 0200) {
			pos = *cp & ~0200;
			continue;
		}
		if (*cp <= 0140) {
			lastc = *cp - 1;
			put(fout, lastc);
			continue;
		}
		if (*cp == 0141) {
			pos = 0;
			continue;
		}
		if (maxp >= 0) {
			if (trace_e64)
				putchar('\n');
			gost_write(line, maxp + 1, fout);
			rstline();
		}
		if (*cp == 0176)
			utf8_puts("\f", fout);
		else
			for (rc = *cp - 0141; rc; --rc)
				putc('\n', fout);
	}
}

static void
decode (FILE *fout, char *data)
{
	if (done)
		return;
	memmove (para, data, PARASZ);
	if (! bytes_total) {
		bytes_total = (para[4] << 8 & 0x300) | para[5];
		if (bytes_total) {
			bytes_total *= 6;
			bytes_tail = para[4] >> 7 | para[3] << 1;
			bytes_tail = ((bytes_tail ^ 0xf) + 1) & 0xf;
			bytes_tail = 6 - bytes_tail;
		} else
			dump(fout, PARASZ);
	}
	if (bytes_total) {
		if (bytes_total > PARASZ) {
			bytes_total -= PARASZ;
			dump(fout, PARASZ);
		} else {
			if (bytes_tail) {
				memmove(para + bytes_total - 6,
					para + bytes_total - bytes_tail,
					bytes_tail);
				dump(fout, bytes_total - 6 + bytes_tail);
			} else
				dump(fout, bytes_total);
			done = 1;
		}
	}
}

void
pout_decode (char *outname)
{
	char		buf[6144];
	FILE		*fout;
	int		z;

	if (outname) {
		fout = fopen (outname, "w");
		if (! fout) {
			perror (outname);
			return;
		}
	} else
		fout = stdout;

	bytes_total = 0;
	bytes_tail = 0;
	done = 0;
	rstline();
	for (z=0; ; ++z) {
            if (disk_readi(disks[OSD_NOMML3].diskh, z, buf, NULL, NULL,
		    DISK_MODE_LOUD) != DISK_IO_OK)
			break;
		decode (fout, buf);
		decode (fout, buf + PARASZ);
		decode (fout, buf + 2*PARASZ);
		decode (fout, buf + 3*PARASZ);
	}
	decode (fout, (char*) (core + 0160000));
	decode (fout, (char*) (core + 0160000) + PARASZ);
	decode (fout, (char*) (core + 0160000) + 2*PARASZ);
	decode (fout, (char*) (core + 0160000) + 3*PARASZ);

	if (maxp >= 0) {
		gost_write(line, maxp + 1, fout);
		putc('\n', fout);
	}
	if (fout != stdout)
		fclose (fout);
}

void
pout_decode_file (char *inname, char *outname)
{
	char		buf [PARASZ];
	FILE		*fin, *fout;

	fin = fopen (inname, "r");
	if (! fin) {
		perror (inname);
		return;
	}
	if (outname) {
		fout = fopen (outname, "w");
		if (! fout) {
			perror (outname);
			fclose (fin);
			return;
		}
	} else
		fout = stdout;
	fflush (stderr);

	bytes_total = 0;
	bytes_tail = 0;
	done = 0;
	rstline();
	while (fread(buf, 1, PARASZ, fin) == PARASZ)
		decode (fout, buf);
	fclose (fin);

	if (maxp >= 0) {
		gost_write(line, maxp + 1, fout);
		putc('\n', fout);
	}
	fflush (fout);
	if (fout != stdout)
		fclose (fout);
}
