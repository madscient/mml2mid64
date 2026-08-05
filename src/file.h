/*
 *      file    name            file.h
 *
 *  A "file" here is really a memory image: the whole input is slurped into
 *  RAM on open, and output is accumulated in RAM and written out on close.
 *  getc2()/putc2()/fseek2()/... are the operations on that image.
 *
 *  Modernised: the DOS far-pointer (LSI-C) and Borland-C variants are gone,
 *  and file offsets are ptrdiff_t rather than long, because they are derived
 *  from pointer differences.  On Win64 long is 32 bits, so the old code
 *  truncated every position it computed.
 */

#ifndef MML2MID_FILE_H
#define MML2MID_FILE_H

#include "compat.h"
#include <stddef.h>
#include <stdio.h>

/* Offset into an in-memory file image.  Used like fpos_t by fgetpos2() and
   fsetpos2(), but it is a plain arithmetic type on purpose: the code compares
   positions and does arithmetic on them. */
typedef ptrdiff_t Fpos_t;

struct File {
	char *top_adr;        /* start of the image                       */
	char *cur_adr;        /* current position                         */
	char *end_adr;        /* one past the last valid byte             */
	char *alloc_end_adr;  /* one past the allocated region            */
	int   flag;           /* 0 = read, 1 = write                      */
	int   f;              /* underlying file descriptor, -1 if none   */
};
typedef struct File file;
typedef file *fileptr;

fileptr fopen2(const char *, const char *);
fileptr fdopen2(int, const char *);
void fclose2(fileptr);
void fclose3(fileptr, fileptr);  /* write fpa then fpb into fpa's fd */
void fclose4(fileptr, fileptr);  /* write only fpb into fpa's fd     */
fileptr fmalloc(void);
void ffree(fileptr);

MML_NORETURN void owari(void);
extern void check_alloc_amount_aux(fileptr);

/* end_adr trails cur_adr by design; see check_alloc_amount(). */
static inline void adjust_end_address(fileptr fp)
{
	if (fp->cur_adr > fp->end_adr) fp->end_adr = fp->cur_adr;
}

/* Grow the image if the next write would leave the allocated region, and keep
   end_adr up to date.  This used to be a GCC statement-expression macro with
   a separate out-of-line copy for other compilers; one inline function serves
   every compiler now. */
static inline void check_alloc_amount(fileptr fp)
{
	if (fp->cur_adr >= fp->alloc_end_adr) check_alloc_amount_aux(fp);
	adjust_end_address(fp);
}

static inline void putc2(int ca, fileptr fp)
{
	check_alloc_amount(fp);
	*(fp->cur_adr++) = (char)ca;
}

static inline int getc2(fileptr fp)
{
	if (fp->cur_adr == fp->end_adr) return EOF;
	return (int)(unsigned char)*(fp->cur_adr++);
}

static inline Fpos_t ftell2(fileptr fp)
{
	return fp->cur_adr - fp->top_adr;
}

static inline void fseek2(fileptr fp, Fpos_t l, int whence)
{
	adjust_end_address(fp);
	switch (whence) {
	case SEEK_CUR:
		fp->cur_adr += l;
		break;
	case SEEK_SET:
		fp->cur_adr = fp->top_adr + l;
		break;
	case SEEK_END:
		fp->cur_adr = fp->end_adr + l;
		break;
	}
	check_alloc_amount(fp);
}

static inline void fgetpos2(fileptr fp, Fpos_t *l)
{
	*l = ftell2(fp);
}

static inline void fsetpos2(fileptr fp, const Fpos_t *l)
{
	fseek2(fp, *l, SEEK_SET);
}

/* Unlike ungetc(), the pushed-back character cannot be chosen. */
static inline void ungetc2(fileptr fp)
{
	fseek2(fp, -1, SEEK_CUR);
}

#endif /* MML2MID_FILE_H */
