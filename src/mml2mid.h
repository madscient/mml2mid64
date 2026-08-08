/*
 *      file    name            mml2mid.h
 */

#ifndef MML2MID_MML2MID_H
#define MML2MID_MML2MID_H

#include "compat.h"

#define numberof(array) (sizeof(array) / sizeof(*array)) /* Nide */

 /* プリプロセス中のエラー終了。行番号とファイル名(無ければNULL)を添えて
    表示する。prepro_error()の中身を macroexp.c からも使えるようにしたもの */
MML_NORETURN void prepro_msg_error(const char *, int, const char *);
MML_NORETURN void prepro_msg_nomem(void);

 /* 「--diag=json」用。エラー・警告を1行のJSONとしてstderrに出す。統合環境が
    vscode.Diagnosticへ変換できるようにするためのもので、人間向けの従来の
    メッセージは何も変わらない。
    fileがNULLなら入力MMLのファイル名を使う。codeは err_msgs[]/warn_msgs[] の
    添字で、該当しない段階(プリプロセス・マクロ展開)では0 */
int diag_json_enabled(void);
void diag_json(const char *severity, int code, const char *file, int line,
	       const char *message);

#endif /* MML2MID_MML2MID_H */
