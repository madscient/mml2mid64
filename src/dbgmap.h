/*
 *      file    name            dbgmap.h
 */

#ifndef MML2MID_DBGMAP_H
#define MML2MID_DBGMAP_H

#include <stdio.h>

 /* JSON文字列リテラルを1つ書く(前後の引用符を含む)。sがNULLなら空文字列。
    「--diag=json」の診断出力もこれを使う。0x80以上のバイトは入力の文字コードの
    まま通すので、-m(MS漢字)で読んだソース由来の文字列は妥当なUTF-8にならない */
void json_write_string(FILE *fp, const char *s);

/* デバッグマップ(tick ↔ MMLソース位置の対応表)の生成。
   統合環境が「再生位置に合わせてソース行をハイライトする」ために使う。
   形式の仕様は doc/DEBUGMAP.md を参照。

   マップは後から導出できない。SMFの出力はストリーム書き込み＋巻き戻し書き換え
   (write_length()が控えたlastlenposへfsetpos2で戻る)で行われ、絶対tickを持った
   イベント配列がどこにも作られないためである。よってイベントを書き出すその場で
   dbgmap_event()を呼んで記録する。

   巻き戻しの対象になるのは常に「まだイベントのバイト列が後続していない、宙ぶらり
   んのデルタ」だけなので、イベント発行の時点で tstep を読めば確定した絶対tickに
   なっている。write_length()にフックを置いてはならない。 */

/* イベント種別。doc/DEBUGMAP.md の "kind" 文字列と1対1に対応する */
#define DBG_NOTEON	0
#define DBG_NOTEOFF	1
#define DBG_PPRES	2
#define DBG_CC		3
#define DBG_PROG	4
#define DBG_CPRES	5
#define DBG_BEND	6
#define DBG_SYSEX	7
#define DBG_META	8
#define DBG_TEMPO	9
#define DBG_BEAT	10

 /* 0=無効 1=行テーブルのみ 2=イベントテーブルも。dbgmap_enabled()が0を返す間は
    他の関数を呼んでも何もしない */
void dbgmap_enable(int level);
int dbgmap_enabled(void);

 /* 入力MMLのファイル名(files[0]になる)。#includeされたファイルは
    dbgmap_event()がppinfoから拾うので、ここで渡すのは主ファイルだけでよい */
void dbgmap_mainfile(const char *name);

 /* 出力トラックが1本増えるとき(write_header())に呼ぶ。
    indexは出力SMFでのトラック番号、trackidはMML上の指定(「0A」「1ML」など) */
void dbgmap_track(int index, const char *trackid);
 /* トラック名(Cn"..."で付く)。同じトラックに対して何度呼んでも最後が残る */
void dbgmap_trackname(int index, const char *name);

 /* イベント1個を記録する。tick/行番号/トラック番号/チャンネルは
    グローバル(tstep, cur_line, ppinfo, trknum, cur_ch)から拾う。
    chに-1を渡すとcur_chを使う */
void dbgmap_event(int kind, int ch, int d1, int d2);

 /* ファイル名を登録して添字を返す(主ファイルは0)。テンポマップのように、
    記録の時点と書き出しの時点が離れていて、ppinfo.fnameの寿命を当てにできない
    場合に使う */
int dbgmap_fileidx(const char *name);

 /* テンポ・拍子。write_tmap()がソート後の絶対tickと行番号を持って呼ぶ。
    DBG_TEMPO なら d1 は4分音符あたりのマイクロ秒、
    DBG_BEAT なら d1 が分子、d2 が分母(4分の4なら4と4) */
void dbgmap_tempo_event(int kind, long tick, int d1, int d2,
			int line, int fidx);

 /* pathへJSONを書き出す。失敗しても警告のみで、SMFの生成は妨げない */
void dbgmap_write(const char *path, int timebase, int format);
void dbgmap_free(void);

 /* 出力MIDIのパスから「.mmlmap.json」のパスを作る。要free()。失敗時はNULL */
char *dbgmap_path_for(const char *midipath);

#endif /* MML2MID_DBGMAP_H */
