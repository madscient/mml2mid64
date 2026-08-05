/*
 *      file    name            file.c
 */

/*
 * file.c holds the functions dealing with the following:
 *   - opening and closing input/output files
 *   - allocating and releasing memory for them
 *   - access to that memory
 */

#include "compat.h" /* must come first: sets feature-test macros */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "file.h"

extern void mml_err(int);

/* Initial allocation for a write image.  It grows on demand, so this only
   needs to be big enough to keep the early reallocs cheap. */
#define INITIAL_WRITE_SIZE 65536L

static long file_length(int fd)
{
	struct stat sb;

	if (fstat(fd, &sb) != 0) return -1;
	if ((sb.st_mode & S_IFMT) != S_IFREG) return -1;
	return (long)sb.st_size;
}

/* read()/write() may transfer less than requested, and may be interrupted by a
   signal.  Loop until the whole amount is done. */
static long full_read(int fd, char *buf, long amount)
{
	long total = 0;

	while (amount > 0) {
		int chunk = amount > 0x40000000L ? 0x40000000 : (int)amount;
		int got = (int)read(fd, buf, (unsigned int)chunk);

		if (got < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		if (got == 0) break;          /* short file */
		buf += got, amount -= got, total += got;
	}
	return total;
}

static long full_write(int fd, const char *buf, long amount)
{
	long total = 0;

	while (amount > 0) {
		int chunk = amount > 0x40000000L ? 0x40000000 : (int)amount;
		int put = (int)write(fd, buf, (unsigned int)chunk);

		if (put < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		buf += put, amount -= put, total += put;
	}
	return total;
}

MML_NORETURN static void write_error(void)
{
	perror("write() or close() error");
	owari();
}

void check_alloc_amount_aux(fileptr fp) /* Add Nide */
{
	char *prev_top_adr;
	ptrdiff_t amount;

	prev_top_adr = fp->top_adr;
	amount = (fp->cur_adr - fp->top_adr) + 40000;
	fp->top_adr = realloc(fp->top_adr, (size_t)amount);
	if (fp->top_adr == NULL) mml_err(61);

	fp->cur_adr = fp->top_adr + (fp->cur_adr - prev_top_adr);
	fp->end_adr = fp->top_adr + (fp->end_adr - prev_top_adr);
	fp->alloc_end_adr = fp->top_adr + amount;
}

/* Read a whole stream whose length is not known in advance (a pipe, say). */
static fileptr slurp_stream(file *fp, int f)
{
	size_t cap = INITIAL_WRITE_SIZE, used = 0;
	char *buf = malloc(cap);

	if (buf == NULL) { free(fp); return NULL; }
	for (;;) {
		long got = full_read(f, buf + used, (long)(cap - used));

		if (got < 0) { free(buf); free(fp); return NULL; }
		used += (size_t)got;
		if (used < cap) break; /* hit EOF */
		cap *= 2;
		{
			char *nb = realloc(buf, cap);

			if (nb == NULL) { free(buf); free(fp); return NULL; }
			buf = nb;
		}
	}
	fp->top_adr = fp->cur_adr = buf;
	fp->alloc_end_adr = buf + cap;
	fp->end_adr = buf + used;
	return fp;
}

fileptr fdopen2(int f, const char *c2)
{
	long l;
	file *fp = (file *)malloc(sizeof(file));

	if (fp == NULL) return NULL; /* abnormal termination */
	fp->top_adr = NULL; /* so that a fclose2() before top_adr is allocated
			       does not make ffree() free an unset pointer */
	fp->f = f;

	/* When c2[1] is 'b' the descriptor must be put into binary mode.  On
	   POSIX this is a no-op; on Windows it matters. */
	if (c2[1] == 'b') mml_setbinary(f);

	if (c2[0] == 'r') {
		fp->flag = 0;
		l = file_length(f);
		if (l < 0) return slurp_stream(fp, f); /* length unknown */

		fp->top_adr = fp->cur_adr = malloc(l > 0 ? (size_t)l : 1);
		if (fp->top_adr == NULL) { /* abnormal termination */
			free(fp); /* f is not closed here */
			return NULL;
		}
		if (l > 0 && full_read(f, fp->top_adr, l) < 0) {
			free(fp->top_adr);
			free(fp);
			return NULL;
		}
		fp->alloc_end_adr = fp->end_adr = fp->top_adr + l;
	} else { /* c2[0] == 'w' */
		fp->flag = 1;

		l = INITIAL_WRITE_SIZE; /* grows via realloc as needed */
		fp->top_adr = fp->cur_adr = malloc((size_t)l);
		if (fp->top_adr == NULL) { /* abnormal termination */
			free(fp); /* f is not closed here */
			return NULL;
		}
		fp->end_adr = fp->top_adr;
		fp->alloc_end_adr = fp->top_adr + l;
	}
	return fp; /* normal termination */
}

fileptr fopen2(const char *c1, const char *c2)
{
	file *fp;
	int f;

	if (c2[0] == 'r') {
		f = open(c1, c2[1] == 'b' ? O_RDONLY | O_BINARY : O_RDONLY);
	} else { /* c2[0] == 'w' */
		f = open(c1,
			 c2[1] == 'b' ? O_CREAT | O_TRUNC | O_WRONLY | O_BINARY
				      : O_CREAT | O_TRUNC | O_WRONLY,
			 MML_CREAT_MODE);
	}
	if (f == -1) return NULL; /* abnormal termination */

	fp = fdopen2(f, c2);
	if (fp == NULL) close(f);
	return fp;
}

void fclose2(fileptr fp)
{
	if (fp->f != -1) {
		if (fp->flag == 1) {
			adjust_end_address(fp);
			if (full_write(fp->f, fp->top_adr,
				       (long)(fp->end_adr - fp->top_adr)) < 0)
				write_error();
		}
		if (close(fp->f) == -1) write_error();
	}

	ffree(fp);
}

/* Append fpb after fpa, then close fpa. */
/* fpb's memory is neither freed nor closed here. */
void fclose3(fileptr fpa, fileptr fpb)
{
	adjust_end_address(fpa);
	adjust_end_address(fpb);

	if (full_write(fpa->f, fpa->top_adr,
		       (long)(fpa->end_adr - fpa->top_adr)) < 0 ||
	    full_write(fpa->f, fpb->top_adr,
		       (long)(fpb->end_adr - fpb->top_adr)) < 0 ||
	    close(fpa->f) == -1) {
		write_error();
	} /* the return values of write() and close() must be checked; same in
	     fclose4() */

	ffree(fpa);
}

/* Allocate a file image without opening any file. */
/* Returns NULL if the allocation fails. */
fileptr fmalloc(void)
{
	long l;
	file *fp = (file *)malloc(sizeof(file));

	if (fp == NULL) return NULL; /* abnormal termination */

	fp->f = -1;
	fp->flag = 1;

	l = INITIAL_WRITE_SIZE;
	fp->top_adr = fp->cur_adr = malloc((size_t)l);
	if (fp->top_adr == NULL) {
		free(fp);
		return NULL; /* abnormal termination */
	}
	fp->end_adr = fp->top_adr;
	fp->alloc_end_adr = fp->top_adr + l;

	return fp; /* normal termination */
}

/* Release fp's memory. */
void ffree(fileptr fp)
{
	if (fp == NULL) return;
	free(fp->top_adr);
	free(fp);
}

/* Write fpb's contents into fpa, then close fpa. */
/* fpb's memory is neither freed nor closed here. */
void fclose4(fileptr fpa, fileptr fpb)
{
	adjust_end_address(fpa);
	adjust_end_address(fpb);

	if (full_write(fpa->f, fpb->top_adr,
		       (long)(fpb->end_adr - fpb->top_adr)) < 0 ||
	    close(fpa->f) == -1) {
		write_error();
	}

	ffree(fpa);
}
