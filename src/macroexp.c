
/*
 *      file    name            macroexp.c
 */

/*
   このファイルでは、「${名前}」「${名前:引数,…}」という形の、長い名前の／
   引数つきのマクロを展開する。

   オリジナル配布物に同梱されている外部プリプロセッサ mmlpp.pl の
   macroexp() / getarg() を本体に取り込んだもので、org-doc/todo.txt の
   「#defineコマンドをつける．（長いマクロ名＋変数）」にあたる。

   getsp() が作ったクック済みイメージ(fp1)を入力に、同じ行数の新しいイメージを
   返す。マクロの本体は必ず1行に収まるので、1入力行＝1出力行が保てる。したがって
   行番号も「# 行番号 "ファイル名"」マーカもずれない。

   波括弧を使わない「$a」「$0a」は従来どおり charproc.c が処理する。ここが
   横取りするのは、波括弧つきの参照と、引数(「#1」など)を含むマクロ定義行だけ。
   */

#include "compat.h" /* must come first: sets feature-test macros */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "charproc.h"
#include "mml2mid.h"
#include "macroexp.h"

extern int mskanji;
extern int bend_range;		/* mml2mid.c 「#bendrange」またはマクロ ${_bend_range_} */
extern int bend_range_fixed;	/* 「#bendrange」で明示された場合に非0 */

 /* マクロ展開の入れ子とマクロ呼び出しの引数の入れ子、どちらの深さ制限も兼ねる。
    mmlpp.pl は再帰マクロを書くと無限ループするが、ここでは打ち切ってエラーに
    する */
#define MAX_MACRO_NEST 100

/* ------------------------------------------------------------------ *
 *  伸びる文字列バッファ
 * ------------------------------------------------------------------ */

typedef struct {
	char *p;
	size_t len, alc;
} strbuf;

static void sb_init(strbuf *sb)
{
	sb->p = NULL, sb->len = sb->alc = 0;
}
static void sb_free(strbuf *sb)
{
	free(sb->p);
	sb_init(sb);
}
static void sb_reset(strbuf *sb)
{
	sb->len = 0;
	if(sb->p != NULL) sb->p[0] = '\0';
}
static const char *sb_str(const strbuf *sb)
{
	return sb->p != NULL ? sb->p : "";
}
static void sb_need(strbuf *sb, size_t n)
{
	size_t need = sb->len + n + 1;
	char *q;

	if(need <= sb->alc) return;
	{
		size_t a = sb->alc != 0 ? sb->alc : 128;

		while(a < need) a *= 2;
		if((q = realloc(sb->p, a)) == NULL) prepro_msg_nomem();
		sb->p = q, sb->alc = a;
	}
}
static void sb_addc(strbuf *sb, int c)
{
	sb_need(sb, 1);
	sb->p[sb->len++] = (char)c;
	sb->p[sb->len] = '\0';
}
static void sb_addn(strbuf *sb, const char *s, size_t n)
{
	sb_need(sb, n);
	memcpy(sb->p + sb->len, s, n);
	sb->len += n;
	sb->p[sb->len] = '\0';
}
static void sb_adds(strbuf *sb, const char *s)
{
	sb_addn(sb, s, strlen(s));
}

/* ------------------------------------------------------------------ *
 *  マクロの表
 * ------------------------------------------------------------------ */

#define MTAB_SIZE 211

typedef struct macrodef {
	struct macrodef *next;
	char *name;
	char *body;
} macrodef;

static macrodef *mtab[MTAB_SIZE];

static unsigned mhash(const char *s)
{
	unsigned h = 0;

	while(*s) h = h * 31u + (unsigned char)*s++;
	return h % MTAB_SIZE;
}

static macrodef *mfind(const char *name)
{
	macrodef *m;

	for(m = mtab[mhash(name)]; m != NULL; m = m->next){
		if(!strcmp(m->name, name)) return m;
	}
	return NULL;
}

 /* 同名のマクロが2度定義されていてもエラーとせず、後の定義を有効にする
    (mmlpp.pl と同じ) */
static void mput(const char *name, const char *body)
{
	macrodef *m = mfind(name);
	char *nb = strdup(body);

	if(nb == NULL) prepro_msg_nomem();
	if(m != NULL){
		free(m->body), m->body = nb;
		return;
	}
	if((m = malloc(sizeof(*m))) == NULL) prepro_msg_nomem();
	if((m->name = strdup(name)) == NULL) prepro_msg_nomem();
	m->body = nb;
	m->next = mtab[mhash(name)], mtab[mhash(name)] = m;
}

static void mfree_all(void)
{
	int i;

	for(i = 0; i < MTAB_SIZE; i++){
		macrodef *m = mtab[i], *next;

		for(; m != NULL; m = next){
			next = m->next;
			free(m->name), free(m->body), free(m);
		}
		mtab[i] = NULL;
	}
}

/* ------------------------------------------------------------------ *
 *  エラー報告
 * ------------------------------------------------------------------ */

 /* 現在読んでいる行の位置。エラー表示にしか使わない */
typedef struct {
	int lineno;
	char *fname;	/* 「# 行番号 "ファイル名"」マーカ由来。無ければNULL */
} mectx;

 /* msgにMsgを渡してもよい。prepro_msg_error()はまずtext_cat()で内容を退避する */
MML_NORETURN static void me_err(const mectx *ctx, const char *msg)
{
	prepro_msg_error(msg, ctx->lineno, ctx->fname);
}

/* ------------------------------------------------------------------ *
 *  字句の細かい処理
 * ------------------------------------------------------------------ */

 /* マクロ名が旧方式(charproc.cが扱う「$a」「$0a」)の形かどうか */
static int legacy_shaped(const char *name)
{
	if(is_lower(name[0]) && name[1] == '\0') return 1;
	return is_digit(name[0]) && is_lower(name[1]) && name[2] == '\0';
}

 /* pは「"」または「'」を指している。閉じ引用符までをoutへ逐語コピーし、その
    次の位置を返す。getLine_cooked()が既に対応を検査済みなので閉じてない文字列は
    来ないはずだが、来ても行末で止まる。
    outがNULLならコピーせず読み飛ばすだけ */
static const char *copy_string(const char *p, strbuf *out)
{
	int qc = *p;

	if(out != NULL) sb_addc(out, *p);
	p++;
	while(*p != '\0' && *p != qc){
		if(*p == '\\' || (mskanji && ismskanji1(*p))){
			if(out != NULL) sb_addc(out, *p);
			p++;
			if(*p == '\0') break;
		}
		if(out != NULL) sb_addc(out, *p);
		p++;
	}
	if(*p == qc){
		if(out != NULL) sb_addc(out, *p);
		p++;
	}
	return p;
}

 /* 「#数字」「#{数字}」が文字列の外にあるか。あれば引数つきマクロの本体
    ということになる。行中の「#」は他の用途が無い(あればdo_command()が構文
    エラーにする)ので、これで既存のMMLと衝突することはない */
static int has_param(const char *p)
{
	while(*p != '\0'){
		if(*p == '"' || *p == '\''){
			p = copy_string(p, NULL);
			continue;
		}
		if(*p == '#'){
			const char *q = p + 1;

			if(is_digit(*q)) return 1;
			if(*q == '{'){
				for(q++; is_digit(*q); q++);
				if(q[0] == '}' && q[-1] != '{') return 1;
			}
		}
		p++;
	}
	return 0;
}

/* ------------------------------------------------------------------ *
 *  マクロ呼び出しの引数
 * ------------------------------------------------------------------ */

typedef struct {
	char **v;
	int n, alc;
} arglist;

static void al_init(arglist *al)
{
	al->v = NULL, al->n = al->alc = 0;
}
static void al_free(arglist *al)
{
	int i;

	for(i = 0; i < al->n; i++) free(al->v[i]);
	free(al->v);
	al_init(al);
}
static void al_push(arglist *al, const char *s)
{
	char *d;

	if(al->n >= al->alc){
		char **nv = realloc(al->v, (size_t)(al->alc += 8) * sizeof(*nv));

		if(nv == NULL) prepro_msg_nomem();
		al->v = nv;
	}
	if((d = strdup(s)) == NULL) prepro_msg_nomem();
	al->v[al->n++] = d;
}

static const char *parse_args(const char *, arglist *, int, const mectx *);

/*
   マクロ呼び出しの引数を取ってくる。pには「:」あるいは「{」の次の位置を与える。
   返り値は対応する「}」の次の位置。

   「,」で区切られた各引数を al に積む。mmlpp.pl の getarg() の移植で、
     ・「\」+1文字 は「\」を剥ぎ取って次の文字そのものとして扱う
     ・「{ }」で囲んだ部分は、中の「,」を区切りと見なさず波括弧ごと引数に含める
       (中身は1段だけ「\」が剥がれる)
     ・「"…"」「'…'」の中は逐語
   という規則も含めて同じ挙動になるようにしてある。
   */
static const char *parse_args(const char *p, arglist *al, int depth,
			      const mectx *ctx)
{
	strbuf cur;
	int flg = 0;	/* 1文字でも読んだら最後の引数を積む (「${a:}」は引数0個) */

	if(depth > MAX_MACRO_NEST)
		me_err(ctx, "ERROR! macro argument nesting too deep");

	sb_init(&cur);
	for(;;){
		if(*p == '\0'){
			sb_free(&cur);
			me_err(ctx, "ERROR! macro argument list not closed");
		}
		if(*p == '}'){
			p++;
			break;
		}
		flg = 1;
		if(*p == ','){
			al_push(al, sb_str(&cur));
			sb_reset(&cur);
			p++;
		} else
		if(*p == '{'){
			arglist inner;
			int i;

			al_init(&inner);
			p = parse_args(p + 1, &inner, depth + 1, ctx);
			sb_addc(&cur, '{');
			for(i = 0; i < inner.n; i++){
				if(i != 0) sb_addc(&cur, ',');
				sb_adds(&cur, inner.v[i]);
			}
			sb_addc(&cur, '}');
			al_free(&inner);
		} else
		if(*p == '\\'){
			p++;
			if(*p == '\0'){
				sb_free(&cur);
				me_err(ctx, "ERROR! macro argument list not closed");
			}
			sb_addc(&cur, *p++);
		} else
		if(*p == '"' || *p == '\''){
			p = copy_string(p, &cur);
		} else {
			sb_addc(&cur, *p++);
		}
	}
	if(flg) al_push(al, sb_str(&cur));
	sb_free(&cur);
	return p;
}

/* ------------------------------------------------------------------ *
 *  展開
 * ------------------------------------------------------------------ */

static void expand_text(const char *, strbuf *, int, const mectx *);

 /* マクロ本体中の「#番号」「#{番号}」を引数で置き換えてoutへ。
    引数の個数は検査しない。足りなければ空、余ったら捨てる (mmlpp.pl と同じ) */
static void subst_params(const char *p, const arglist *al, strbuf *out)
{
	while(*p != '\0'){
		int idx = 0;
		const char *q;

		if(*p == '"' || *p == '\''){
			p = copy_string(p, out);
			continue;
		}
		if(*p != '#'){
			sb_addc(out, *p++);
			continue;
		}

		q = p + 1;
		if(is_digit(*q)){
			idx = dtoi(*q), q++;
		} else
		if(*q == '{' && is_digit(q[1])){
			for(q++; is_digit(*q); q++) idx = idx * 10 + dtoi(*q);
			if(*q != '}'){ /* 「#{12」など。「#」をそのまま出す */
				sb_addc(out, *p++);
				continue;
			}
			q++;
		} else { /* 引数指定ではない「#」 */
			sb_addc(out, *p++);
			continue;
		}

		if(1 <= idx && idx <= al->n) sb_adds(out, al->v[idx - 1]);
		p = q;
	}
}

 /* pは「${」を指している。展開結果をoutへ入れ、参照の次の位置を返す */
static const char *expand_ref(const char *p, strbuf *out, int depth,
			      const mectx *ctx)
{
	strbuf name, sub;
	arglist al;
	macrodef *m;

	sb_init(&name);
	al_init(&al);

	for(p += 2; is_alnum(*p) || *p == '_'; p++) sb_addc(&name, *p);
	if(*p == ':'){
		p = parse_args(p + 1, &al, depth + 1, ctx);
	} else
	if(*p == '}'){
		p++;
	} else {
		sb_free(&name), al_free(&al);
		me_err(ctx, "ERROR! illegal macro reference '${...}'");
	}
	if(name.len == 0){
		sb_free(&name), al_free(&al);
		me_err(ctx, "ERROR! macro name is empty");
	}

	if((m = mfind(sb_str(&name))) == NULL){
		 /* 旧方式の形の名前なら「$名前」を出して charproc.c に委ねる。
		    mmlppdoc.txt〔1.2〕の「${0a}」＝「$0a」がこれで成り立つ */
		if(al.n == 0 && legacy_shaped(sb_str(&name))){
			sb_addc(out, '$');
			sb_adds(out, sb_str(&name));
			sb_free(&name), al_free(&al);
			return p;
		}
		msg_printf("ERROR! undefined macro '${%s}'", sb_str(&name));
		sb_free(&name), al_free(&al);
		me_err(ctx, Msg);
	}

	sb_init(&sub);
	subst_params(m->body, &al, &sub);
	expand_text(sb_str(&sub), out, depth + 1, ctx);
	sb_free(&sub);

	sb_free(&name), al_free(&al);
	return p;
}

 /* 文字列の外の「${…}」を展開しながらoutへ写す */
static void expand_text(const char *p, strbuf *out, int depth, const mectx *ctx)
{
	if(depth > MAX_MACRO_NEST)
		me_err(ctx, "ERROR! macro expansion too deep (recursive macro?)");

	while(*p != '\0'){
		if(*p == '"' || *p == '\''){
			p = copy_string(p, out);
		} else
		if(*p == '$' && p[1] == '{'){
			p = expand_ref(p, out, depth, ctx);
		} else {
			sb_addc(out, *p++);
		}
	}
}

/* ------------------------------------------------------------------ *
 *  行の分類
 * ------------------------------------------------------------------ */

#define DEF_NONE   0	/* マクロ定義行ではない */
#define DEF_NEW    1	/* ここで取り込む定義行 */
#define DEF_LEGACY 2	/* charproc.c に任せる定義行 */

/*
   行がマクロ定義行かどうかを調べる。定義行なら name にマクロ名を、
   *body に本体(名前の後の空白・タブを除いた位置)を入れる。
   *braced には「${名前}」の形だったかどうかが入る。

   引数(「#1」など)を含む本体は charproc.c では扱えないので、旧方式の形の
   名前であってもこちらで引き取る。行中の「#」は今まで構文エラーにしか
   ならなかったので、これで意味が変わる既存のMMLは無い。
   */
static int classify_def(const char *line, strbuf *name, const char **body,
			int *braced)
{
	const char *p = line;

	sb_reset(name);
	if(*p != '$') return DEF_NONE;
	p++;

	if(*p == '{'){
		for(p++; is_alnum(*p) || *p == '_'; p++) sb_addc(name, *p);
		if(name->len == 0 || *p != '}'){
			sb_reset(name);
			return DEF_NONE; /* 参照として展開を試みる */
		}
		p++;
		*braced = 1;
	} else {
		if(is_digit(*p)) sb_addc(name, *p++);
		if(!is_lower(*p)){
			sb_reset(name);
			return DEF_NONE;
		}
		sb_addc(name, *p++);
		*braced = 0;
	}

	while(*p == ' ' || *p == '\t') p++;
	*body = p;

	if(*braced && !legacy_shaped(sb_str(name))) return DEF_NEW;
	return has_param(p) ? DEF_NEW : DEF_LEGACY;
}

/* ------------------------------------------------------------------ *
 *  入力の走査
 * ------------------------------------------------------------------ */

 /* 1行読む。行末の「\n」は含めない。EOFで何も読めなければ0 */
static int read_line(fileptr fp, strbuf *sb)
{
	int c = getc2(fp);

	sb_reset(sb);
	if(c == EOF) return 0;
	while(c != EOF && c != '\n'){
		sb_addc(sb, c);
		c = getc2(fp);
	}
	return 1;
}

/*
   getsp() が書いた「# 行番号 "ファイル名"」マーカなら、ctxを更新して1を返す。
   fp1 に現れる「#」で始まる行はこのマーカだけ(他の「#…」はgetsp()が処理済み)。
   行番号の意味は getppinfo() / read_ppinfo() と揃えてある。
   */
static int read_marker(const char *p, mectx *ctx)
{
	int n = 0;

	if(*p != '#') return 0;
	for(p++; *p == ' ' || *p == '\t'; p++);
	if(!is_digit(*p)) return 0;
	for(; is_digit(*p); p++) n = n * 10 + dtoi(*p);
	for(; *p == ' ' || *p == '\t'; p++);

	ctx->lineno = n - 1;	/* 次に読む行がn行目 */
	free(ctx->fname), ctx->fname = NULL;
	if(*p == '"'){
		strbuf f;

		sb_init(&f);
		p = copy_string(p, &f);
		 /* 前後の「"」を落とす */
		if(f.len >= 2){
			f.p[f.len - 1] = '\0';
			if((ctx->fname = strdup(f.p + 1)) == NULL) prepro_msg_nomem();
			if(ctx->fname[0] == '\0') free(ctx->fname), ctx->fname = NULL;
		}
		sb_free(&f);
	}
	return 1;
}

 /* パス1。マクロ定義を集める。行の書き換えはしない */
static void collect_defs(fileptr fpi)
{
	strbuf line, name;
	mectx ctx;
	const char *body;
	int braced;

	sb_init(&line), sb_init(&name);
	ctx.lineno = 0, ctx.fname = NULL;

	fseek2(fpi, 0L, SEEK_SET);
	while(read_line(fpi, &line)){
		ctx.lineno++;
		if(read_marker(sb_str(&line), &ctx)) continue;

		if(classify_def(sb_str(&line), &name, &body, &braced) != DEF_NEW)
			continue;

		mput(sb_str(&name), body);

		 /* 予約マクロ ${_bend_range_} (mmlppbnd.txt) 。「#bendrange」で
		    明示されていればそちらを優先する */
		if(!strcmp(sb_str(&name), "_bend_range_") && !bend_range_fixed){
			const char *q = body;
			int v = 0;

			if(!is_digit(*q))
				me_err(&ctx, "ERROR! ${_bend_range_} must be a number");
			for(; is_digit(*q); q++) v = v * 10 + dtoi(*q);
			while(*q == ' ' || *q == '\t') q++;
			if(*q != '\0' || v < 1 || v > 24)
				me_err(&ctx, "ERROR! ${_bend_range_} must be 1 to 24");
			bend_range = v;
		}
	}

	free(ctx.fname);
	sb_free(&line), sb_free(&name);
}

 /* パス2。参照を展開しながら新しいイメージへ書き出す */
static fileptr expand_all(fileptr fpi)
{
	fileptr fpo;
	strbuf line, name, out;
	mectx ctx;
	const char *body;
	int braced;

	if((fpo = fmalloc()) == NULL) prepro_msg_nomem();

	sb_init(&line), sb_init(&name), sb_init(&out);
	ctx.lineno = 0, ctx.fname = NULL;

	fseek2(fpi, 0L, SEEK_SET);
	while(read_line(fpi, &line)){
		ctx.lineno++;
		sb_reset(&out);

		if(read_marker(sb_str(&line), &ctx)){
			sb_adds(&out, sb_str(&line));
		} else {
			int kind = classify_def(sb_str(&line), &name, &body, &braced);

			if(kind == DEF_NEW){
				 /* 定義はパス1で取り込み済み。行番号を保つため空行を出す */
			} else
			if(kind == DEF_LEGACY && braced){
				 /* 「${0a} …」を charproc.c が読める「$0a …」に直す */
				sb_addc(&out, '$');
				sb_adds(&out, sb_str(&name));
				sb_addc(&out, ' ');
				expand_text(body, &out, 0, &ctx);
			} else {
				 /* 「$a …」も含め、元の字面のまま参照だけ展開する */
				expand_text(sb_str(&line), &out, 0, &ctx);
			}
		}

		{
			const char *q = sb_str(&out);

			while(*q != '\0') putc2(*q++, fpo);
			putc2('\n', fpo);
		}
	}

	free(ctx.fname);
	sb_free(&line), sb_free(&name), sb_free(&out);
	return fpo;
}

fileptr expand_macros(fileptr fpi)
{
	fileptr fpo;

	collect_defs(fpi);
	fpo = expand_all(fpi);
	mfree_all();	/* 本体が要るのはパス2の間だけ */
	fseek2(fpo, 0L, SEEK_SET);
	return fpo;
}
