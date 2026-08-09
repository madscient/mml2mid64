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

/* ---- トラック名 -------------------------------------------------------
   トラック名は「[A-Z]」または「[A-Z][0-9]」の1〜2文字。内部ではこれを
   0 〜 NTRKNAME-1 の通し番号(talf)で表す。

	talf = (1文字目 - 'A') * TRK_SUFFIXES + 2文字目コード

   2文字目コードは、2文字目が無い場合が0、'0'〜'9'が1〜10。よって
   talf == 0 は従来どおりトラック「A」を表す。 */
#define TRK_SUFFIXES 11
#define NTRKNAME (26 * TRK_SUFFIXES)		/* 286 */
#define trkname_letter(t) ((t) / TRK_SUFFIXES + 'A')
#define trkname_suffix(t) ((t) % TRK_SUFFIXES)	/* 0ならトラック名は1文字 */
#define trkname_index(c, s) (((c) - 'A') * TRK_SUFFIXES + (s))
 /* 2文字目コードと実際の文字との変換。文字が無い場合は0 */
#define trksuffix2char(s) ((s) ? '0' + (s) - 1 : 0)
#define trkchar2suffix(c) ((c) ? (c) - '0' + 1 : 0)

/* track_map[]は従属トラック番号(0〜9)ごとにTRKMAP_STRIDE個の枠を持つ。
   最後の1枠はワイルドカード用の予約(オリジナル版から引き継いだもので、
   現在は常に0のまま) */
#define TRKMAP_STRIDE (NTRKNAME + 1)
#define TRKMAP_WILD NTRKNAME
#define TRKMAP_SIZE (10 * TRKMAP_STRIDE)

const char *trkname_str(int);
void build_track_map(void);
int resolve_src_line(int, const char **);
void reset_ppinfo(void);

#define mml_warn(i) mml_err(-(i))

#endif /* MML2MID_CHARPROC_H */
