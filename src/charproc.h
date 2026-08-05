/*
 *      file    name            charproc.h
 */

#ifndef MML2MID_CHARPROC_H
#define MML2MID_CHARPROC_H

#include "compat.h"
#include <ctype.h>

void free_all_macros(void);
void init_all_macros(void);
void scanmacro(void);
int Getbyte(int, int);
#define getbyte(x) Getbyte(x, 0)
MML_NORETURN void owari(void);

/* isascii() is not standard C (it is an obsolescent POSIX extension, and MSVC
   only offers it as __isascii), so define the test directly.  Note that the
   argument is frequently EOF/-1 here, which must test false. */
#define is_ascii(c) ((unsigned int)(c) < 128u)

#ifndef isoctal
#define isoctal(c) (isdigit(c) && (c) < '8')
#endif
#define is_alpha(c) (is_ascii(c) && isalpha(c))
#define is_upper(c) (is_ascii(c) && isupper(c))
#define is_lower(c) (is_ascii(c) && islower(c))
#define to_upper(c) (is_lower(c) ? toupper(c) : (c))
#define to_lower(c) (is_upper(c) ? tolower(c) : (c))
#define is_alnum(c) (is_ascii(c) && isalnum(c))
#define is_digit(c) (is_ascii(c) && isdigit(c))
#define is_octal(c) (is_ascii(c) && isoctal(c))
#define is_xdigit(c) (is_ascii(c) && isxdigit(c))
#define is_space(c) (is_ascii(c) && isspace(c))
 /* toupperの定義域が英小文字に限られたり、isupperなどの定義域が7ビット文字に
    限られたりするC処理系もあるので要注意 */
#define xtoi(c) ((c) - ((c)>='a' ? 'a'-10 : (c)>='A' ? 'A'-10 : '0'))
#define dtoi(c) ((c) - '0')
#define Ismskanji1(c) (0x81<=(c) && ((c)<=0x9f || (0xe0<=(c) && (c)<=0xfc)))
#define ismskanji1(c) Ismskanji1((unsigned char)(c))

struct master_step {
	long step;
	int linenum;
};

#define mml_warn(i) mml_err(-(i))

#endif /* MML2MID_CHARPROC_H */
