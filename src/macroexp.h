/*
 *      file    name            macroexp.h
 */

#ifndef MML2MID_MACROEXP_H
#define MML2MID_MACROEXP_H

#include "compat.h"
#include "file.h"

/* 「${名前}」「${名前:引数,…}」を展開した新しいイメージを返す。
   入力イメージの解放は呼び出し側の責任。エラー時は戻ってこない。 */
fileptr expand_macros(fileptr fpi);

#endif /* MML2MID_MACROEXP_H */
