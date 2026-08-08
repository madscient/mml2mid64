
/*
 *      file    name            dbgmap.c
 */

#include "compat.h" /* must come first: sets feature-test macros */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "charproc.h"
#include "dbgmap.h"

/* mmlproc.c からの extern */
extern long tstep;	/* トラックの現在のステップタイム(絶対tick) */
 /* 今処理しているコマンドが書かれていた位置(解決済み)。cur_line/ppinfoを
    直接読んではならない — 先読みで先へ進んでいる。mmlproc.cの宣言を参照 */
extern int cur_cmd_line, cur_cmd_file;
extern int cur_ch;	/* 現在のチャンネル */

/* mml2mid.c からの extern */
extern int trknum;	/* 現在書いているトラックの番号(テンポマップが0) */

static int level = 0;

 /* 配列はどれも tmap_realloc() (mmlproc.c) と同じ流儀で伸ばす。伸長に失敗した
    場合はマップの生成だけ諦める。SMFの生成を巻き添えにしてはならない */
#define DBG_GROW 512

struct dbgent {
	int track;
	long tick;
	int ch;		/* -1 ならチャンネルを持たないイベント */
	int kind;
	int d1, d2;
	int fileidx;
	int line;
	int seq;	/* 同じ(track,tick)での出現順を保つため */
};

struct dbgtrack {
	int index;
	int channel;	/* まだ判らなければ -1 */
	char *trackid;	/* MML上の指定「0A」「1ML」など。テンポマップはNULL */
	char *name;	/* Cn"..." で付くトラック名。無ければNULL */
};

static char **files;
static int files_ptr, files_amount;

static struct dbgtrack *tracks;
static int tracks_ptr, tracks_amount;

static struct dbgent *lines;
static int lines_ptr, lines_amount;

static struct dbgent *events;
static int events_ptr, events_amount;

 /* テンポは level に関わらず常に集める。イベントテーブル(level 2)と独立に、
    マップ単体で tick↔実時刻の変換ができるようにするため */
struct dbgtempo {
	long tick;
	int usec;	/* 4分音符あたりのマイクロ秒 */
	int seq;
};
static struct dbgtempo *tempos;
static int tempos_ptr, tempos_amount;

static int seq_counter;

 /* 途中でメモリが足りなくなったら立てる。以後は記録をやめ、書き出しも行わない */
static int broken;

static const char *kind_names[] = {
	"on", "off", "ppres", "cc", "prog", "cpres", "bend",
	"sysex", "meta", "tempo", "beat"
};

/* ------------------------------------------------------------------ */

void dbgmap_enable(int lv)
{
	level = lv;
}

int dbgmap_enabled(void)
{
	return !broken && level > 0;
}

 /* 要素1個分の空きを確保する。確保できなければ broken を立てて0を返す */
static int grow(void **area, int ptr, int *amount, size_t size)
{
	void *p;

	if(ptr < *amount) return 1;
	if((p = realloc(*area, (*amount + DBG_GROW) * size)) == NULL){
		broken = 1;
		return 0;
	}
	*area = p;
	*amount += DBG_GROW;
	return 1;
}

static char *dbg_strdup(const char *s)
{
	char *p;

	if(s == NULL) return NULL;
	if((p = malloc(strlen(s) + 1)) == NULL){
		broken = 1;
		return NULL;
	}
	strcpy(p, s);
	return p;
}

 /* ファイル名を files[] に登録し、その添字を返す。NULLは主ファイル(0) */
static int fileidx(const char *name)
{
	int i;

	if(name == NULL || *name == '\0') return 0;
	for(i = 0; i < files_ptr; i++){
		if(strcmp(files[i], name) == 0) return i;
	}
	if(!grow((void **)&files, files_ptr, &files_amount, sizeof(*files)))
		return 0;
	if((files[files_ptr] = dbg_strdup(name)) == NULL) return 0;
	return files_ptr++;
}

int dbgmap_fileidx(const char *name)
{
	if(!dbgmap_enabled()) return 0;
	return fileidx(name);
}

void dbgmap_mainfile(const char *name)
{
	if(!dbgmap_enabled()) return;
	 /* files[0] は主ファイル。fileidx()は0を「主ファイル」の意味で返すので
	    ここで先に埋めておく */
	if(files_ptr == 0){
		if(!grow((void **)&files, 0, &files_amount, sizeof(*files)))
			return;
		if((files[0] = dbg_strdup(name == NULL ? "" : name)) == NULL)
			return;
		files_ptr = 1;
	}
}

/* ------------------------------------------------------------------ */

static struct dbgtrack *find_track(int index)
{
	int i;

	for(i = 0; i < tracks_ptr; i++){
		if(tracks[i].index == index) return &tracks[i];
	}
	return NULL;
}

void dbgmap_track(int index, const char *trackid)
{
	struct dbgtrack *t;

	if(!dbgmap_enabled()) return;
	if((t = find_track(index)) == NULL){
		if(!grow((void **)&tracks, tracks_ptr, &tracks_amount,
			 sizeof(*tracks)))
			return;
		t = &tracks[tracks_ptr++];
		t->index = index;
		t->channel = -1;
		t->trackid = NULL;
		t->name = NULL;
	}
	free(t->trackid);
	t->trackid = dbg_strdup(trackid);
}

void dbgmap_trackname(int index, const char *name)
{
	struct dbgtrack *t;

	if(!dbgmap_enabled()) return;
	if((t = find_track(index)) == NULL) return;
	free(t->name);
	t->name = dbg_strdup(name);
}

/* ------------------------------------------------------------------ */

 /* 行テーブルへ1件積む。直前の1件と(track, fileidx, line)が同じなら畳む。
    write時にもソート後に同じ規則で畳み直すので、ここでの畳み込みは
    もっぱらメモリ節約のためのもの */
static void add_line(int track, long tick, int fidx, int line, int seq)
{
	struct dbgent *e;

	if(lines_ptr > 0){
		e = &lines[lines_ptr - 1];
		if(e->track == track && e->fileidx == fidx && e->line == line)
			return;
	}
	if(!grow((void **)&lines, lines_ptr, &lines_amount, sizeof(*lines)))
		return;
	e = &lines[lines_ptr++];
	e->track = track;
	e->tick = tick;
	e->ch = -1;
	e->kind = 0;
	e->d1 = e->d2 = 0;
	e->fileidx = fidx;
	e->line = line;
	e->seq = seq;
}

static void add_event(int track, long tick, int ch, int kind, int d1, int d2,
		      int fidx, int line, int seq)
{
	struct dbgent *e;

	if(level < 2) return;
	if(!grow((void **)&events, events_ptr, &events_amount, sizeof(*events)))
		return;
	e = &events[events_ptr++];
	e->track = track;
	e->tick = tick;
	e->ch = ch;
	e->kind = kind;
	e->d1 = d1;
	e->d2 = d2;
	e->fileidx = fidx;
	e->line = line;
	e->seq = seq;
}

void dbgmap_event(int kind, int ch, int d1, int d2)
{
	int line = cur_cmd_line, fidx = cur_cmd_file, seq;
	struct dbgtrack *t;

	if(!dbgmap_enabled()) return;

	 /* メタイベントとSysExはMIDI上チャンネルを持たない */
	if(kind == DBG_META || kind == DBG_SYSEX) ch = -1;
	else if(ch < 0) ch = cur_ch;
	seq = seq_counter++;

	 /* トラックのチャンネルはヘッダを書く時点では未確定(Cコマンドはその後に
	    処理される)なので、最初のイベントが来た時点で埋める */
	if((t = find_track(trknum)) != NULL && t->channel < 0) t->channel = cur_ch;

	 /* 行テーブルにはキーオフを入れない。「このtickではこの行が鳴っている」を
	    表すのが行テーブルの役目で、キーオフの位置はそれを歪めるだけ。
	    イベントテーブル(level 2)には残す */
	if(kind != DBG_NOTEOFF) add_line(trknum, tstep, fidx, line, seq);
	add_event(trknum, tstep, ch, kind, d1, d2, fidx, line, seq);
}

void dbgmap_tempo_event(int kind, long tick, int d1, int d2,
			int line, int fidx)
{
	int seq;

	if(!dbgmap_enabled()) return;

	seq = seq_counter++;

	if(kind == DBG_TEMPO &&
	   grow((void **)&tempos, tempos_ptr, &tempos_amount, sizeof(*tempos))){
		tempos[tempos_ptr].tick = tick;
		tempos[tempos_ptr].usec = d1;
		tempos[tempos_ptr].seq = seq;
		tempos_ptr++;
	}

	 /* テンポトラックは常に出力トラック0 */
	add_line(0, tick, fidx, line, seq);
	add_event(0, tick, -1, kind, d1, d2, fidx, line, seq);
}

/* ------------------------------------------------------------------ */

static int ent_comp(const void *p1, const void *p2)
{
	const struct dbgent *a = p1, *b = p2;

	if(a->track != b->track) return a->track < b->track ? -1 : 1;
	if(a->tick  != b->tick)  return a->tick  < b->tick  ? -1 : 1;
	return a->seq < b->seq ? -1 : 1; /* 同じ値同士の順番は保存 */
}

void json_write_string(FILE *fp, const char *s)
{
	putc('"', fp);
	if(s != NULL){
		for(; *s != '\0'; s++){
			unsigned char c = (unsigned char)*s;

			switch(c){
			case '"':  fputs("\\\"", fp); break;
			case '\\': fputs("\\\\", fp); break;
			case '\b': fputs("\\b", fp);  break;
			case '\f': fputs("\\f", fp);  break;
			case '\n': fputs("\\n", fp);  break;
			case '\r': fputs("\\r", fp);  break;
			case '\t': fputs("\\t", fp);  break;
			default:
				 /* 0x80以上は入力の文字コードのまま通す。従って
				    -m(MS漢字)で読んだソースのトラック名などは
				    JSONとして妥当なUTF-8にならない。
				    doc/DEBUGMAP.mdに明記してある */
				if(c < 0x20) fprintf(fp, "\\u%04x", c);
				else putc(c, fp);
				break;
			}
		}
	}
	putc('"', fp);
}

void dbgmap_write(const char *path, int timebase, int format)
{
	FILE *fp;
	int i, prev_track = -1, prev_file = -1, prev_line = -1, first;

	if(!dbgmap_enabled()) return;
	if(path == NULL) return;

	qsort(lines,  (size_t)lines_ptr,  sizeof(*lines),  ent_comp);
	qsort(events, (size_t)events_ptr, sizeof(*events), ent_comp);

	 /* "wb" なのは改行をLFに固定するため。マップは機械が読むものなので、
	    同じMMLからは全プラットフォームで同一のバイト列が出るのが望ましい */
	if((fp = fopen(path, "wb")) == NULL){
		msg_printf("Warning: cannot write debug map <%s>\n", path);
		text_cat(Msg);
		msg_flush();
		return;
	}

	fputs("{\n", fp);
	fputs("  \"version\": 1,\n", fp);
	fputs("  \"generator\": \"mml2mid 5.30b (mml2mid64 fork)\",\n", fp);
	fprintf(fp, "  \"level\": %d,\n", level);
	fprintf(fp, "  \"timebase\": %d,\n", timebase);
	fprintf(fp, "  \"format\": %d,\n", format);

	fputs("  \"files\": [", fp);
	for(i = 0; i < files_ptr; i++){
		if(i) fputs(", ", fp);
		json_write_string(fp, files[i]);
	}
	fputs("],\n", fp);

	fputs("  \"tracks\": [\n", fp);
	for(i = 0; i < tracks_ptr; i++){
		const struct dbgtrack *t = &tracks[i];

		fprintf(fp, "    {\"index\": %d, \"kind\": \"%s\", \"name\": ",
			t->index, t->trackid == NULL ? "tempo" : "mml");
		json_write_string(fp, t->name == NULL ? "" : t->name);
		fprintf(fp, ", \"channel\": %d", t->channel);
		if(t->trackid != NULL){
			fputs(", \"trackid\": ", fp);
			json_write_string(fp, t->trackid);
		}
		fprintf(fp, "}%s\n", i + 1 < tracks_ptr ? "," : "");
	}
	fputs("  ],\n", fp);

	fputs("  \"tempo\": [", fp);
	for(i = 0, first = 1; i < tempos_ptr; i++){
		if(!first) fputs(", ", fp);
		first = 0;
		fprintf(fp, "[%ld, %d]", tempos[i].tick, tempos[i].usec);
	}
	fputs("],\n", fp);

	 /* 行テーブル。ソート後に(track, file, line)が直前と同じものを畳む */
	fputs("  \"lines\": [\n", fp);
	for(i = 0, first = 1; i < lines_ptr; i++){
		const struct dbgent *e = &lines[i];

		if(e->track == prev_track && e->fileidx == prev_file &&
		   e->line == prev_line)
			continue;
		prev_track = e->track, prev_file = e->fileidx;
		prev_line = e->line;
		if(!first) fputs(",\n", fp);
		first = 0;
		fprintf(fp, "    [%d, %ld, %d, %d]",
			e->track, e->tick, e->fileidx, e->line);
	}
	fputs(first ? "  ]" : "\n  ]", fp);

	if(level >= 2){
		fputs(",\n  \"events\": [\n", fp);
		for(i = 0, first = 1; i < events_ptr; i++){
			const struct dbgent *e = &events[i];

			if(!first) fputs(",\n", fp);
			first = 0;
			fprintf(fp, "    [%d, %ld, %d, \"%s\", %d, %d, %d, %d]",
				e->track, e->tick, e->ch,
				kind_names[e->kind], e->d1, e->d2,
				e->fileidx, e->line);
		}
		fputs(first ? "  ]" : "\n  ]", fp);
	}
	fputs("\n}\n", fp);

	if(ferror(fp) || fclose(fp) != 0){
		msg_printf("Warning: error writing debug map <%s>\n", path);
		text_cat(Msg);
		msg_flush();
		return;
	}
	msg_printf("debug map: %s\n", path);
	text_cat(Msg);
	msg_flush();
}

void dbgmap_free(void)
{
	int i;

	for(i = 0; i < files_ptr; i++) free(files[i]);
	for(i = 0; i < tracks_ptr; i++){
		free(tracks[i].trackid);
		free(tracks[i].name);
	}
	free(files), files = NULL, files_ptr = files_amount = 0;
	free(tracks), tracks = NULL, tracks_ptr = tracks_amount = 0;
	free(lines), lines = NULL, lines_ptr = lines_amount = 0;
	free(events), events = NULL, events_ptr = events_amount = 0;
	free(tempos), tempos = NULL, tempos_ptr = tempos_amount = 0;
}

/* ------------------------------------------------------------------ */

char *dbgmap_path_for(const char *midipath)
{
	static const char suffix[] = ".mmlmap.json";
	size_t len;
	const char *dot, *p;
	char *out;

	if(midipath == NULL) return NULL;

	 /* 最後の「.」を探す。ただしディレクトリ区切りより後ろのものだけ */
	dot = NULL;
	for(p = midipath; *p != '\0'; p++){
		if(*p == '/' || *p == '\\') dot = NULL;
		else if(*p == '.') dot = p;
	}
	len = (dot != NULL) ? (size_t)(dot - midipath) : strlen(midipath);

	if((out = malloc(len + sizeof(suffix))) == NULL) return NULL;
	memcpy(out, midipath, len);
	strcpy(out + len, suffix);
	return out;
}
