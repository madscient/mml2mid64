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

#endif /* MML2MID_MML2MID_H */
