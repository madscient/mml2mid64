/*
 *      file    name            compat.h
 *
 *  Platform compatibility layer (formerly "win.h").
 *
 *  The original sources selected between UNIX / WINDOWS (Win16 GUI) / MSDOS /
 *  BCC / LSI-C via hand-set macros.  Only two of those environments still
 *  exist, so this header now auto-detects them:
 *
 *      _WIN32   ... Windows (MSVC or MinGW), 32/64 bit console build
 *      else     ... POSIX (Linux, macOS, *BSD, Cygwin, ...)
 *
 *  It also holds the message-buffer mechanism (text[] / Msg[]) that the whole
 *  program uses to report progress and errors.
 */

#ifndef MML2MID_COMPAT_H
#define MML2MID_COMPAT_H

/* Ask the C library for the POSIX names this program uses (strdup, isatty,
   fstat, open, ...).  Under a strict -std=cNN glibc hides them unless a
   feature-test macro is set, and the macro has to be set before any system
   header is pulled in -- which is why compat.h is the first #include in every
   .c file here.  Do not move it below the system headers. */
#if !defined(_WIN32)
# ifndef _DEFAULT_SOURCE
#  define _DEFAULT_SOURCE 1
# endif
# ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200809L
# endif
#endif

#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 *  Platform detection and low-level file I/O names
 * ------------------------------------------------------------------ */

#ifdef _WIN32
# include <io.h>
# include <fcntl.h>
# include <sys/types.h>
# include <sys/stat.h>
# if defined(_MSC_VER)
   /* MSVC spells the POSIX functions with a leading underscore. */
#  define open   _open
#  define close  _close
#  define read   _read
#  define write  _write
#  define isatty _isatty
#  define strdup _strdup
# endif
# ifndef O_BINARY
#  define O_BINARY _O_BINARY
# endif
# ifndef S_IREAD
#  define S_IREAD  _S_IREAD
#  define S_IWRITE _S_IWRITE
# endif
  /* Default creation mode for open(..., O_CREAT, ...) */
# define MML_CREAT_MODE (S_IREAD | S_IWRITE)
  /* Put a descriptor into binary mode.  Not spelled `setmode' because the
     BSDs (and macOS) already use that name for something else entirely. */
# if defined(_MSC_VER)
#  define mml_setbinary(fd) ((void)_setmode((fd), _O_BINARY))
# else
#  define mml_setbinary(fd) ((void)setmode((fd), O_BINARY))
# endif
#else
# include <unistd.h>
# include <fcntl.h>
# include <sys/types.h>
# include <sys/stat.h>
# ifndef O_BINARY
#  define O_BINARY 0            /* POSIX makes no text/binary distinction */
# endif
# define MML_CREAT_MODE 0666
# define mml_setbinary(fd) ((void)0)
#endif

/* ------------------------------------------------------------------ *
 *  Global message buffers
 * ------------------------------------------------------------------ */

#ifdef GLOBAL_VALUE_DEFINE
# define GLOBAL
#else
# define GLOBAL extern
#endif

#ifdef MSG_TO_STDOUT
# define STDERR stdout
#else
# define STDERR stderr
#endif

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif

/* Marks the error-exit helpers.  Without it the compiler cannot tell that a
   `case' following one of them is unreachable, and warns about a fall-through
   that can never happen. */
#if defined(_MSC_VER)
# define MML_NORETURN __declspec(noreturn)
#else
# define MML_NORETURN _Noreturn
#endif

GLOBAL char text[8192];  /* accumulated, flushed by msg_flush() */
GLOBAL char Msg[1024];   /* scratch for one formatted line; a title may be
                            up to 256 bytes, so 256 is not enough */

/* Non-zero while the first of the two compile passes is running.  That pass
   only measures how many ticks each `=0' needs to write the state back that
   the `=1' region changed (see psw_flush() in mmlproc.c); its output is
   thrown away, so its progress lines and warnings must stay quiet -- they are
   printed by the second pass.  Errors clear it so they still get reported. */
GLOBAL int psw_collect_pass;

#undef GLOBAL

/* Format into Msg[] without ever overrunning it.  Replaces the old
   `wsprintf(Msg, ...)', which was a bare sprintf() on non-Windows. */
#define msg_printf(...) ((void)snprintf(Msg, sizeof(Msg), __VA_ARGS__))

/* Append to text[], truncating rather than overrunning.  The original code
   used strcat() here, which could overflow text[] on a track containing many
   `S' (status dump) commands. */
static inline void text_cat(const char *s)
{
	size_t used;

	if (psw_collect_pass) return; /* measuring pass: stay quiet */
	used = strlen(text);
	if (used + 1 < sizeof(text))
		strncat(text + used, s, sizeof(text) - used - 1);
}

/* Emit everything accumulated so far and reset the buffer.  This replaces the
   old InvalidateRect()/UpdateWindow() no-op pair. */
static inline void msg_flush(void)
{
	if (text[0] != '\0') {
		fputs(text, STDERR);
		text[0] = '\0';
	}
	fflush(STDERR);
}

#endif /* MML2MID_COMPAT_H */
