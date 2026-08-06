
/*
 *      file    name            mmlproc.c
 */

#include "compat.h" /* must come first: sets feature-test macros */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "charproc.h"
#include "mmlproc.h"

#define MAX_TEXT_STR 256

#define MAX_TRACK_NAME_STR 256

static char trackname[MAX_TRACK_NAME_STR + 1];

/* mml2mid.c からの extern */
extern fileptr fp2;         /* 出力ファイル(mid) */
extern fileptr fp3;         /* テンポ・マップ用 */
extern void smftrkheader(fileptr, Fpos_t *);
extern int timebase;      /* タイムベース 48,60,80,96,120,160,240,480 */
extern int trknum;        /* トラック数 */
extern Fpos_t trksize;    /* トラックサイズを書き込む場所 */
extern int tnum;          /* トラックを表す 0A 1B 2C などの 0,1,2 */
extern int talf;          /* 0A 1B 2C0 などの A,B,C0 (通し番号) */
extern Fpos_t lastlenpos; /* 最後に音長を書き込んだ場所へのポインタ */
extern long lastlen;      /* 最後に書き込んだ音長の値 */
extern int x68k;
extern int x68k2;
MML_NORETURN extern void remove_file_and_owari(void);
extern int german_scale, backcompati, mskanji, warnmode;

/* note.c からの extern */
extern int andflag;     /* &があったかどうかのフラグ */
extern int koff;	/* キーオフ・ベロシティ */
extern int keyproc_ptr, keyproc_amount;
			/* キーオフ時に処理すべきイベントの個数 */
extern int mmlproc_ptr, mmlproc_amount;
extern struct mmlproc *mmlproc;
extern void note(int); /* 音符、休符処理*/
extern void note_r_noflush(void); /* `r の処理 */
extern void note_off_specific(int); /* `<音名> の処理 */
extern void noteoff(int, int);
extern int soutai0(int);
extern void setcode_F(int, int, int);
extern void setcode_I(int, int, int);
extern void setcode_U(int, int);
int check_proc(int);

/* charproc.c */
extern struct {
	int line, actual_line;
	char *fname;
} ppinfo;
extern int master_count; /* MKR追加 */
extern int master_count_temp, master_count_temp_prev; /* Modified Nide */
extern int escchr(int);

int head;		/* 従属パートでヘッダを書いたかどうかのフラグ */
long tstep;		/* トラックの現在のステップタイム */
int kloop_ptr;		/* ループネスト数 */
int cur_line;		/* 現在のMMLソース上の行番号 */
int cur_ch;		/* 現在のチャンネル */
int run;		/* ランニングステータス */
int octave;		/* オクターブ */
int under;		/* 転調 */
int transcale;		/* 調号 */
static int clength;     /* lコマンドのクロック数 */
int velocity, rvel1, rvel2; /* ベロシティー */
int randvel1, randvel2; /* ランダムベロシティー */
int ktencho;		/* 強制転調コマンドの値 0だと無視される */
int gatetimeQ;		/* Qコマンドの値 ディフォルトは8 */
int gatetime, gatetime2; /* qコマンドの値 */
int ctl_values[KIND_MAX]; /* expression, panpot, …をまとめた配列 */
static struct {
	Fpos_t pos; /* 負の長さを書いた位置 */
	int lineno; /* その原因の入力行番号 */
} last_oddlen; /* 従属トラック位置合わせ不能エラーのチェック用 */
static int kakko_def;
int rpan1, rpan2;
int mod_delay, mod_after;
int koff;		/* キーオフ・ベロシティ */
static struct tmap *tmap;
static int tmap_ptr, tmap_amount;
int tempo_master = 100;
struct keyproc2 *keyproc2;
int keyproc2_ptr, keyproc2_amount;  /* 個数 */
int codef[KIND_MAX][MAX_F_ARG+1]; /* Fコマンド保存用 */
int codeu[KIND_MAX][MAX_U_ARG+1]; /* Uコマンド保存用 */
int codei[KIND_MAX][5];  /* Iコマンド保存用 */
static int rpn_para;
static int nrpn_para;
int psw; /* '='スイッチ */
static int prog; /* プログラムチェンジ */
static int bs1; /* bank select */
static int bs2; /* bank select */
static int var[256]; /* 変数 z0 ... z255 */
int base; /* @+-n */

void write_length(long, fileptr); /* 絶対長->可変長に変換して書きこむ */
static void write_keyproc2(void);
			/* 1トラック変換終了直後に，発音中の音をキーオフ */
static void write_restofmmlproc(void);
static void code_y(void); /* yコマンド処理 */
static void oct(void); /* オクターブ処理 */
int length(void);
int xget(int *i);
static void tempo(void); /* tコマンド処理 */
static void code_B(void); /* Bが来たときの処理 (BS, BB, BR, BT) */
static void add_tmap(int, int, long);
static void beat(void);		/* BTコマンド処理 */
static void bendrange(void);	/* BRコマンドの処理 */
static void bendset(int);	/* BS, BW, BBコマンドの処理 */
static int log2i(int i);	/* log(2,i) */
static void getgatetime(void);
static void getgatetimeQ(void);
static void getktencho(void);
static void getvelocity(void);
static void get_randvel(void);
static void getvolume(void);
static void getexpression(void);
static void getexclusive(int);
static void variables(void);
static void code_E(void);
static void code__(void);
static void code___(void);
static void kakko1(void);
static void kakko2(void);
static void bankselect(void);
static void progchange(void);
static void getpanpot(void);
static void setRT(void);
static void pedal(int);		/* P, Xコマンドの処理 */
static void code_Y(void);	/* Yコマンドの処理（モノモード） */
static void keyoffvelocity(void);
static void code_M(void);
void mml_err(int i); /* エラー処理 */
static void code_F(void);
static void code_U(void);
static void code_I(void);
int check_cntchange(int, int);
static void put_cntchange(int, int, long);
static void put_cntchange0(int, int);
int check_bendchange(int);
static void put_bend(int, long);
static void put_bend0(int);
static void write_rpn(int, int, int); /* RPN書き込み */
static void write_nrpn(int, int, int); /* NRPN書き込み */
static void write_nrpn0(int, int, int); /* NRPN書き込み */
static void detune(void);
static void getnrpn(void);
static void setpsw(void); /* #スイッチ処理 */
void write_header(void);
static void cpres0(void);
static void put_cpres(int);
static void put_cpres0(int);
static void tvf(void);
static void tvf_c(void);
static void tvf_r(void);
static void set_transcale(void);
static void get_trackname(void);
static void write_trackname(void);
static void texts(void);
static void put_midistring(int);
static int mml_getstring(char *, int);
static void do_command(int);

extern int x_values[]; /* x_expression, x_panpot…を全部まとめた配列 */

extern long first_F[];
extern long last_F[];
extern long first_I[];
extern long last_I[];

int alloc_tmap(void)
{
	tmap_ptr = 0;
	tmap = malloc((tmap_amount = 256) * sizeof(*tmap));
	return tmap != NULL ? 0 : -1;
}

 /* mmlprocの各種別に対する値を書く
    (必要なものは)後でwrite_length(0,fp2)もしている */
void do_mmlproc(int kind, int para)
{
	switch(kind){
	case MMLPROC_EXPR:
	case MMLPROC_PANPOT:
	case MMLPROC_VOL:
	case MMLPROC_MOD_ON:
		put_cntchange(kind2param(kind), para, 0);
		break;
	case MMLPROC_BEND:
		put_bend(para, 0);
		break;
	case MMLPROC_CUTOFF:
	case MMLPROC_RESO:
		write_nrpn(1, kind2param(kind), para);
		break;
	case MMLPROC_CPRES:
		put_cpres(para);
		break;
	default: /* case MMLPROC_DTEMPO: */
		add_tmap(TMAP_DIFF, para, tstep);
		break;
	}
}

void do_mmlproc0(int kind, int para, long st)
 /* チェックなしで書く また、必要なら別途write_length()をすること
    stはDTEMPOの場合だけ使われる */
{
	switch(kind){
	case MMLPROC_EXPR:
	case MMLPROC_PANPOT:
	case MMLPROC_VOL:
	case MMLPROC_MOD_ON:
		put_cntchange0(kind2param(kind), para);
		break;
	case MMLPROC_BEND:
		put_bend0(para);
		break;
	case MMLPROC_CUTOFF:
	case MMLPROC_RESO:
		write_nrpn0(1, kind2param(kind), para);
		break;
	case MMLPROC_CPRES:
		put_cpres0(para);
		break;
	default: /* case MMLPROC_DTEMPO: */
		add_tmap(TMAP_DIFF, para, tstep+st);
		break;
	} /* write_length()は呼ばれない */
}

/* control change を書くかどうかをチェックする関数 */
/* 書く場合は0を、書かない場合は -1 を返す。 */
/* p1 == -1 の時は，static 変数の初期化 */
/* p2 == -1 の時は、何もせずにリターン */
int check_cntchange(int p1, int p2)
{
	static int last_modulation = -1;
	static int last_volume = -1;
	static int last_panpot = -1;
	static int last_expression = -1;

	if(p2 == -1) return -1;
	if(p1 == -1){ /* 新たなトラックのコンパイル、または、sコマンド発効 */
		last_modulation = -1;
		last_volume = -1;
		last_panpot = -1;
		last_expression = -1;
		return -1;
	}

	switch(p1){
	case 1:
		if(p2 == last_modulation) return -1;
		last_modulation = p2;
		break;
	case 7:
		if(p2 == last_volume) return -1;
		last_volume = p2;
		break;
	case 10:
		if(p2 == last_panpot) return -1;
		last_panpot = p2;
		break;
	case 11:
		if(p2 == last_expression) return -1;
		last_expression = p2;
		break;
	default:
		break;
	}
	return 0;
}

/* control change を書く関数。書くかどうかチェックを入れる。 */
/* 書いた後にステップタイムも書く */
static void put_cntchange(int p1, int p2, long l)
{
	if(psw == 0){
		if(check_cntchange(p1, p2) == -1) return;
		put_cntchange0(p1, p2);
		write_length(l, fp2);
	}
}

/* control change を書く関数。チェック無しで書く。 */
static void put_cntchange0(int p1, int p2)
{
	if(run != 0xb){
		putc2(0xb0 + cur_ch, fp2);
		run = 0xb;
	}
	putc2(p1, fp2);
	putc2(p2, fp2);
}

/* bend を書くかどうかをチェックする関数 */
/* 書く場合は0を、書かない場合は -1 を返す。 */
/* p == -1 の時は，static 変数の初期化 */
int check_bendchange(int p)
{
	static int last_bend = -1;

	if(p == -1){ /* 新たなトラックのコンパイル、または、sコマンド発効 */
		last_bend = -1;
		return -1;
	}

	if(p == last_bend) return -1;
	last_bend = p;
	return 0;
}

/* bend を書く関数。書くかどうかチェックを入れる。 */
/* 書いた後にステップタイムも書く */
static void put_bend(int p, long l)
{
	if(psw == 0){
		if(check_bendchange(p) == -1) return;
		put_bend0(p);
		write_length(l, fp2);
	}
}

/* bend を書く関数。チェック無しで書く。 */
static void put_bend0(int p)
{
	if(run != 0xe){
		putc2(0xe0 + cur_ch, fp2);
		run = 0xe;
	}
	putc2(p % 128, fp2);
	putc2(p / 128, fp2);
}

/* RPN書き込み */
static void write_rpn(int tmsb, int tlsb, int tdat)
{
	int i;

	if(psw == 0){
		i = tmsb * 256 + tlsb;
		if(i != rpn_para) {
			put_cntchange0(101, tmsb);
			write_length(0, fp2);
			put_cntchange0(100, tlsb);
			write_length(0, fp2);
			rpn_para = i;
		}
		nrpn_para = -1;
		put_cntchange0(6, tdat);
		write_length(0, fp2);
	}
}

/* NRPN書き込み */
static void write_nrpn(int tmsb, int tlsb, int tdat)
{
	if(psw == 0){
		write_nrpn0(tmsb, tlsb, tdat);
		write_length(0, fp2);
	}
}

/* lengthを書かないNRPN書き込み(psw処理無し) */
static void write_nrpn0(int tmsb, int tlsb, int tdat)
{
	int i;

	i = tmsb * 256 + tlsb;
	if(i != nrpn_para){
		put_cntchange0(99, tmsb);
		write_length(0, fp2);
		put_cntchange0(98, tlsb);
		write_length(0, fp2);
		nrpn_para = i;
	}
	rpn_para = -1;
	put_cntchange0(6, tdat);
}

/*
  tnum, talfに対応するトラックを変換する．
  トラックに１文字もない場合、-1を返す。
  */
int converttrk(void)
{
	int i;
	int code;

	/* グローバル変数の初期化 */
	head = 0; /* (従属トラックで?)まだヘッダを書いていない */
	master_count_temp = master_count_temp_prev = -1;
	 /* 従属トラックの場合、対応主トラックを見つけたらその先頭の
	    ステップ情報を格納したmaster_step[]の添字をmaster_count_tempへ */
	master_count = (!tnum ? 0 : -1);
	 /* 主トラックの場合はgettrack()内でmaster_step[master_count]にステップ
	    情報を書いてはmaster_countを進めるためmaster_countは0から開始
	    従属トラックでは対応する主トラックを見つけた時master_countを進める
	    のでmaster_countの初期値は-1 */
	last_oddlen.pos = -1; /* 従属track位置合わせ失敗チェック用変数初期化 */

	cur_line = 1; /* 行番号 = 1 から読み始める */
	kloop_ptr = 0; /* ループネスト数 = 0 */
	cur_ch = 0; /* 現在処理中のMIDIチャンネル */
	run = 0; /* ランニングステータス初期化 */
	octave = 4; /* オクターブ */
	under = 0; /* 転調 */
	transcale = 0; /* 調号 */
	clength = timebase; /* lコマンドのクロック数 */
	 /* converttrk()が呼ばれるのはanalyze()より後であることと timebaseを
	    設定するのはanalyze()の中だけであることを前提とする 従ってここでは
	    timebaseはもう設定済み */
	velocity = 100; /* ベロシティー */
	tstep = 0; /* トラックの現在のステップタイム */
	kakko_def = 4;
	/*
	volume = def_volume;
	expression = def_expression;
	bend = def_bend;
	panpot = def_panpot;
	*/
	cutoff = -1;
	resonance = -1;
	volume = -1;
	expression = -1;
	bend = -1;
	panpot = -1;
	mod_on = -1;

	cpres = -1;

	rpan1 = rpan2 = 0;
	rvel1 = rvel2 = 0;
	randvel1 = randvel2 = 0;
	mod_delay = -1;
	koff = 0; /* キーオフベロシティー */
	rpn_para = -1;
	nrpn_para = -1;
	psw = 0; /* '='スイッチＯＦＦ */
	prog = -1;
	bs1 = bs2 = 0;
	for(i = 0; i < KIND_MAX; i++){
		codef[i][0] = 0;
		codeu[i][0] = 0;
		codei[i][0] = 0;
		first_F[i] = 0; /* 以下４個は念のため入れておいた */
		last_F[i] = 0;
		first_I[i] = 0;
		last_I[i] = 0;
	}
	setcode_I(0,0,-1); /* static変数の初期化 */
	setcode_F(0,0,-1);
	base = 0; /* トラック毎に指定する */
	for(i = 0; i < 256; i++){ /* 変数の初期化 */
		var[i] = 0;
	}

	ktencho = 0;	/* 強制転調＝オフ */
	gatetimeQ = 8;	/* Qコマンドのゲートタイム */
	gatetime = 1;	/* 第1ゲートタイム */
	gatetime2 = 0;	/* 第2ゲートタイム */
	andflag = 0;	/* & があったかどうかのフラグ */
	koff = 0;	/* キーオフ・ベロシティ */
	keyproc_ptr = 0; /* キーオフ時に処理すべきイベントの個数 */
	mmlproc_ptr = 0;
	keyproc2_ptr = 0; /* ゲートタイムがマイナスだった音の個数 */
	check_cntchange(-1, 0); /* static 変数の初期化 */
	check_bendchange(-1); /* static 変数の初期化 */

	trackname[0] = '\0'; /* should not be `NULL' */

	code = getbyte(1);
	if(head == 0){ /* 従属パートでまだヘッダを書いていない */
		if(code != -1 && code != '!'){
			/* トラックに最低1文字あった時点でヘッダを書く */
			write_header();
	 		head = 1;
		}else return -1;
	}
	for(;;){
		code = getbyte(0);
		if(code == -1 || code == '!'){
			getbyte(-1);
			msg_printf(" %6ld step%c \"%s\"\n",
				tstep, tstep > 1 ? 's' : ' ', trackname);
			text_cat(Msg);
			msg_flush();
			if(kloop_ptr != 0)
				mml_err(1); /* ループが終了してない */
			if(last_oddlen.pos >= 0){ /* 従属トラック合わせ失敗 */
				cur_line = last_oddlen.lineno;
				mml_err(70);
			}
			write_keyproc2();
			write_restofmmlproc();
			return 0; /* 1個のトラックの変換終了 */
		}
		do_command(code);
	}
}
static void do_command(int code)
{
	int i;

	switch(code){
	case 'C':
		cur_ch = (xget(&i) - 1) % 16;
			/* cur_ch にはチャンネル-1 (0〜15) が入る */
		get_trackname();
		check_cntchange(-1, 0);
		check_bendchange(-1);
		run = 0;
		break;
	case '_':
		code__();
		break;
	case '*':
		if(!backcompati) goto syn_err;
		code___();
		break;
	case 'q':
		getgatetime();
		break;
	case 'Q':
		getgatetimeQ();
		break;
	case 'J':
		getktencho();
		break;
	case 'k':
		switch(getbyte(1)){
		case 'r':
			get_randvel();
			break;
		default:
			getvelocity();
			break;
		}
		break;
	case 'w':
		keyoffvelocity();
		break;
	case 's':
		check_cntchange(-1, 0);
		check_bendchange(-1);
		break;
	case 'v':
		getvolume();
		break;
	case '@':
		progchange();
		break;
	case 'D':
		detune();
		break;
	case 'E':
		code_E();
		break;
	case 'F':
		code_F();
		break;
	case 'U':
		code_U();
		break;
	case 'H':
		bankselect();
		break;
	case 'I':
		code_I();
		break;
	case 'N':
		getnrpn();
		break;
	case '~':
		bendset('S');
		break;
	case 'V':
		kakko_def = xget(&i);
		break;
	case ')':
		kakko1();
		break;
	case '(':
		kakko2();
		break;
	case 'p':
		getpanpot();
		break;
	case 'R':
		setRT();
		break;
	case 'M':
		code_M();
		break;
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'g':
	case 'r':
	case '\\':
	case 'A': /* pori press */
		note(code);
		break;
	case 'K': /* ノートオンか調号 */
		switch(getbyte(1)){
		case 'I': case 'J':
			set_transcale();
			break;
		default:
			note(code);
			break;
		}
		break;
	case 'h':
		if(!german_scale) goto syn_err;
		note(code);
		break;
	case '`': /* 発音中の和音の操作(音長0の拡張。doc/CHANGES.md参照) */
		i = getbyte(0);
		switch(i){
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
			note_off_specific(i);
			break;
		case 'h':
			if(!german_scale){ code = i; goto syn_err; }
			note_off_specific(i);
			break;
		case 'r':
			note_r_noflush();
			break;
		default:
			code = i; goto syn_err; /* 実際に問題の文字を表示するため */
		}
		break;
	case '^':
		mml_warn(3);
		note('r');
		break;
	case 'l':
		clength = length();
		break;
	case 'o':
		oct();
		break;
	case '>':
		if(x68k == 0) octave++;
		else octave--;
		break;
	case '<':
		if(x68k == 0) octave--;
		else octave++;
		break;
	case 'y':
		code_y();
		break;
	case 'Y':
		code_Y();
		break;
	case 't':
		tempo();
		break;
	case 'B':
		code_B();
		break;
	case '=':
		setpsw();
		break;
	case 'P':
		pedal(127);
		break;
	case 'X':
		pedal(0);
		break;
	case 'G':
		cpres0();
		break;
	case 'S':
		msg_printf("\n --- H%d,%d @%d o%d v%d k%d E%d p%d BS%d(BW%d) "
			 "q%d,%d Q%d l%s%d (%ld step%s)\n        ",
			bs1, bs2, prog, octave, volume, velocity, expression,
			panpot, bend/128, bend,
			gatetime, gatetime2, gatetimeQ,
			(timebase * 4 % clength) ? "%" : "",
			(timebase * 4 % clength) ? clength : timebase * 4 / clength,
			tstep, tstep > 1 ? "s" : "");
		text_cat(Msg);
		break;
	case 'T':
		tvf();
		break;
	case 'Z':
		variables();
		break;
	case 'W':
		texts();
		break;
	default:
	syn_err:
		msg_printf("%c", code);
		warnmode ? mml_warn(1) : mml_err(64);
	}
	 /* 「[」「]」はgetbyte()から呼ばれるtrack()が展開しているから
	    ここでの処理は不要 */
}

int ichi; /* =1 があったかどうかのフラグ */

/*
 * '='スイッチの処理
 */
static void setpsw(void)
{
	int i, num;

	num = xget(&i);
	if(i == -2){
		if(psw == 0) psw = 1;
		else psw = 0;
	}else{
		if(num != 0 && num != 1) mml_err(35);
		else psw = num;
	}
	if(psw == 1){
		ichi = 1;
		prog = -1;
		write_keyproc2();
		write_restofmmlproc();
		return;
	}else if(ichi == 1){
		ichi = 0;
		if(prog != -1){ /* 途中でプログラムチェンジが１個以上あった場合 */
			put_cntchange0(0, bs1);
			write_length(0, fp2);
			put_cntchange0(32, bs2);
			write_length(0, fp2);
			/* if(run != 0xc){} ランニングステータスは無視する */
			putc2(0xc0 + cur_ch, fp2);
			run= 0xc;
			putc2(prog, fp2);
			write_length(0, fp2);
		}
		if(mod_on != -1) put_mod(mod_on);
		if(volume != -1) put_vol(volume);
		if(panpot != -1) put_panpot(panpot);
		if(expression != -1) put_expr(expression);
		if(cpres != -1) put_cpres(cpres);
	}
}

/* mmlを1トラック分変換し終えた直後に，
   その時に発音中の残りの音をキーオフする．

   なぜこういう処理が必要かと言うと，
   ゲートタイムがマイナスの場合に，やむを得ずキーオフされない音が
   残る場合があるから．
   */
static void write_keyproc2(void)
{
	int i;

	for(i = 0; i < keyproc2_ptr; i++){
		noteoff(keyproc2[i].kp_onkai, keyproc2[i].kp_velo);
		write_length(0, fp2);
	}
}

/* mmlを1トラック分変換し終えた直後に，
   その時に発音中の残りの音をキーオフする．

   ゲートタイムがマイナスで、しかもその値が巨大な時に
   keyproc2ではなくてmmlprocに発音中の音が残っている場合がある。
   */
static void write_restofmmlproc(void)
{
	int i;

	for(i = 0; i < mmlproc_ptr; i++){
		if(mmlproc[i].kind == -1){ /* 種別＝キーオフ */
			noteoff(mmlproc[i].param_onkai, mmlproc[i].param_velo);
			write_length(0, fp2);
		}
	}
}

/* ) の処理 */
static void kakko1(void)
{
	int num, i;

	num = xget(&i);
	if(x68k2 == 0){
		if(i == -2) velocity += kakko_def;
		else velocity += num;
	}else{
		if(i == -2) velocity -= kakko_def;
		else velocity -= num;
	}
	ALIGN(velocity);
}

/* ( の処理 */
static void kakko2(void)
{
	int num, i;

	num = xget(&i);
	if(x68k2 == 0){
		if(i == -2) velocity -= kakko_def;
		else velocity -= num;
	}else{
		if(i == -2) velocity += kakko_def;
		else velocity += num;
	}
	ALIGN(velocity);
}

static int tmap_timing_comp(const void *p1, const void *p2)
{ /* tmapソート用比較関数 */
	const struct tmap *t1 = p1, *t2 = p2;

	if(t1->st < t2->st) return -1;
	if(t1->st > t2->st) return 1;
	if(t1->map == TMAP_DIFF){
		if(t2->map != TMAP_DIFF) return 1;
	} else {
		if(t2->map == TMAP_DIFF) return -1;
	} /* 差分指定はそれ以外より後 */
	return t1->index < t2->index ? -1 : 1;
	 /* 同じ値同士での順番は保存 */
}
static unsigned long tempo_conv(int tempo)
{
	unsigned long i4, i_mod, t;

	if(tempo <= 0) tempo = 1;

	/* i4 = 60000000UL * 100 / (tempo_master * tempo);
	   なのだが、その通りにやるとオーバーフローするので */
	t = (unsigned long)tempo * (unsigned long)tempo_master;
	if(t == 0) t = 1; /* `#xtempo 0' would divide by zero otherwise */
	i4 = 3000000000UL / t * 2;
	i_mod = 3000000000UL % t * 4;
	if(i_mod >= t) i4++;
	if(i_mod >= 3*t) i4++;

	if(i4 >= 1UL<<24) i4 = (1UL<<24)-1;
	return i4;
}

/* テンポマップへの書き込み */
void write_tmap(void)
{
	int i, cur_tempo = 0, cur_base_tempo = 0;
	long last_st = 0;
	struct {
		int *area, amount, ptr;
	} tempo_stack;

	tempo_stack.area = malloc(sizeof(int) * (tempo_stack.amount = 30));
	if(tempo_stack.area == NULL) mml_err(61);
	tempo_stack.ptr = 0;

	qsort(tmap, (size_t)tmap_ptr, sizeof(*tmap), tmap_timing_comp);
	for(i = 0; i < tmap_ptr; i++){
		unsigned long i4;
		
		write_length(tmap[i].st - last_st, fp3);
		last_st = tmap[i].st;
		switch(tmap[i].map){
		case TMAP_BEAT: /* beat これだけ他のと全く別処理 */
			putc2(0xff, fp3);
			putc2(0x58, fp3);
			putc2(4, fp3);
			putc2(tmap[i].p1 / 100, fp3);
			putc2(log2i(tmap[i].p1 % 100), fp3);
			putc2(0x18, fp3);
			putc2(8, fp3);
			continue;

		case TMAP_DIFF: /* temporally add tempo */
			cur_tempo = cur_base_tempo + tmap[i].p1;
			break;
		case TMAP_POP: /* pop tempo */
			if(!tempo_stack.ptr){
				/* includeファイルの措置などを考慮すると
				   エラーの行番号を出すのは若干面倒なので
				   とりあえず行番号なしエラーメッセージ出す */
				fprintf(STDERR,	"ERROR!  't)' without 't('\n");
				remove_file_and_owari();
			} else {
				cur_tempo = cur_base_tempo =
					tempo_stack.area[--tempo_stack.ptr];
			}
			break;
		default:
			if(tmap[i].map & TMAP_PUSH){ /* push tempo */
				if(tempo_stack.ptr>=tempo_stack.amount){
					tempo_stack.amount = tempo_stack.ptr + 30;
					tempo_stack.area = realloc(tempo_stack.area,
						sizeof(int) * tempo_stack.amount);
					if(tempo_stack.area == NULL) mml_err(61);
				}
				tempo_stack.area[tempo_stack.ptr++] = cur_tempo;
			}
			if(tmap[i].map & TMAP_RELATIVE){ /* relative */
				cur_tempo = (cur_base_tempo += tmap[i].p1);
			} else {
				cur_tempo = cur_base_tempo = tmap[i].p1;
			}
			break;
		}

		/* beatの場合以外の共通処理 */
		i4 = tempo_conv(cur_tempo);
		putc2(0xff, fp3);
		putc2(0x51, fp3);
		putc2(0x03, fp3);
		putc2((char)(i4>>16),fp3);
		putc2((char)(i4>>8), fp3);
		putc2((char)i4,	fp3);
	}
	write_length(0, fp3);
	if(tempo_stack.ptr != 0){
		/* 警告メッセージ。上と同じ理由で行番号出さない */
		text_cat("Warning: 't(' not closed by 't)'\n");
		msg_flush();
	}
	free(tempo_stack.area);
	tmap_ptr = 0; /* 不要かも? */
}

static void tmap_realloc(void) /* Add Nide */
{
	if(tmap_ptr >= tmap_amount){
		tmap_amount = tmap_ptr + 512;
		if((tmap = realloc(tmap, tmap_amount * sizeof(*tmap))) == NULL){
			mml_err(61);
		}
	}
}

static void add_tmap(int map, int num, long step)
{
	tmap[tmap_ptr].index = tmap_ptr;
	tmap[tmap_ptr].map = map;
	tmap[tmap_ptr].p1 = num;
	tmap[tmap_ptr].st = step;
	tmap_ptr++;
	tmap_realloc();
}

/*
  テンポ・イベントの処理。
  テンポ・マップに書く。
  最初にステップタイムを書いてからテンポイベントを書く。
  */
static void tempo(void)
{
	int i, num, map;

	switch(getbyte(1)){
	case '(':
		(void)getbyte(2);
		num = xget(&i);
		map = TMAP_PUSH; /* push tempo */
		break;
	case ')':
		(void)getbyte(2);
		num = 0; i = 2;
		map = TMAP_POP; /* pop tempo */
		break;
	default:
		num = xget(&i);
		if(i == -2) mml_err(10); /* 値がない */
		map = TMAP_NORMAL; /* normally specify tempo */
		break;
	}

	switch(i){
	case 2: /* pop */
		break;
	case 0: /* 直接指定 */
		if(!num) mml_err(10); /* 値が0 */
		break;
	default: /* 相対指定 */
		map |= TMAP_RELATIVE;
		break;
	}
	add_tmap(map, num, tstep);
}

/*
  Bが来たときの処理。
  BS, BB, BR, BT, BW のコマンドがある。
  */
static void code_B(void)
{
	int kind;

	switch(kind = getbyte(0)){
	case 'T':
		beat();
		break;
	case 'R':
		bendrange();
		break;
	case 'S':
	case 'B':
	case 'W':
		bendset(kind);
		break;
	default:
		mml_err(13);
	}
}

/* log(2,i), modified by Nide */
static int log2i(int i)
{
	int j = 0;

	while(i /= 2) j++;
	return j;
}

/*
  BTコマンドの処理
  テンポ・マップに書く。
  最初にステップタイムを書いてからテンポイベントを書く。
  */
static void beat(void)
{
	int i, num, num2;

	num = xget(&i);
	if(i != 0) mml_err(8);
	if(getbyte(1) != ',') mml_err(8); /* BT?の後にコンマがない */
	(void)getbyte(2);
	num2 = xget(&i);
	if(i != 0) mml_err(8);

	add_tmap(TMAP_BEAT, num * 100 + num2, tstep);
}

/* channel press コマンドの処理 */
static void cpres0(void)
{
	int old, first = 0;

	if(cpres == -1) first = 0, cpres = def_cpres; /* Add Nide */
	old = cpres;
	if((cpres = soutai0(cpres)) == -1) mml_err(40);
	first ? (x_cpres = 0) : (x_cpres += cpres - old); /* 他と揃えた */
	if(psw == 0){
		if(run != 0xd){
			putc2(0xd0 + cur_ch, fp2);
			run = 0xd;
		}
		putc2(cpres, fp2);
		write_length(0, fp2);
	}
}

/* channel press コマンドの処理パート２ */
static void put_cpres(int x)
{
	if(psw == 0){
		put_cpres0(x);
		write_length(0, fp2);
	}
}

/* channel press コマンドの処理パート３ */
static void put_cpres0(int x)
{
	if(run != 0xd){
		putc2(0xd0 + cur_ch, fp2);
		run = 0xd;
	}
	putc2(x, fp2);
}

/* BRコマンドの処理 */
static void bendrange(void)
{
	int i, num;

	num = xget(&i);
	if(i != 0) mml_err(11);
	write_rpn (0,0,num);
}

/* BS, BW, BBコマンドの処理 */
static void bendset(int kind)
{
	int old, first = 0, bend_h, bend_l, num, i;

	if(bend == -1) first = 1, bend = def_bend; /* Add Nide */
	old = bend;

	num = xget(&i);
	switch(kind){
	case 'S': /* BS */
		bend_h = bend / 128;
		bend_l = bend % 128;

		switch(i){
		case -2:
			mml_err(17); /* never returns */
			break;
		case 0:
			bend_h = num;
			break;
		default: /* 相対指定の場合 */
			bend_h += num;
			break;
		}
		if(getbyte(1) != ','){
			bend_l = 0;
		} else {
			(void)getbyte(2);
			num = xget(&i);
			switch(i){
			case -2:
				mml_err(17); /* never returns */
				break;
			case 0:
				bend_l = num;
				break;
			default: /* 相対指定の場合 */
				bend_l += num;
				break;
			}
		}
		bend = bend_h * 128 + bend_l;
		break;
	case 'W': /* BW */
		switch(i){
		case -2:
			mml_err(34); /* never returns */
			break;
		case 0:
			bend = num;
			break;
		default: /* 相対指定の場合 */
			bend += num;
			break;
		}
		break;
	default: /* case 'B': */	/* BBコマンド; obsolete */
		switch(i){
		case -2:
			mml_err(18); /* never returns */
			break;
		default: /* 0でも相対指定と見なす */
			bend += num * 128;
			break;
		}
	}
	ALIGN2(bend);

	first ? (x_bend = 0) : (x_bend += bend - old);
	if(check_bendx() == 0) put_bend(bend, 0);
}

/* yコマンドの処理 */
static void code_y(void)
{
	int num, num2, i;

	num = xget(&i);
	if(i != 0) mml_err(9); /* 値が変 */
	if(getbyte(0) != ',') mml_err(9); /* y?の後にコンマがない */
	num2 = xget(&i);
	if(i != 0) mml_err(9); /* 値が変 */
	put_cntchange(num, num2, 0);
}

/* Nコマンドの処理 */
static void getnrpn(void)
{
	int num, num2, num3, i;

	num = xget(&i);
	if(i != 0) mml_err(32);
	if(getbyte(0) != ',') mml_err(32);
	num2 = xget(&i);
	if(i != 0) mml_err(32);
	if(getbyte(0) != ',') mml_err(32);
	num3 = xget(&i);
	if(i != 0) mml_err(32);
	if(getbyte(1) == ',') mml_err(33);
	write_nrpn(num, num2, num3);
}

/* Hコマンドの処理 */
static void bankselect(void)
{
	int i;

	bs1 = xget(&i);
	if(i != 0) mml_err(22); /* 値が変 */
	if(getbyte(1) != ',') mml_err(22); /* y?の後にコンマがない */
	(void)getbyte(2);
	bs2 = xget(&i);
	if(i != 0) mml_err(22); /* 値が変 */
	if(psw == 0){
		put_cntchange0(0, bs1);
		write_length(0, fp2);
		put_cntchange0(32, bs2);
		write_length(0, fp2);
	}
}

/* vコマンドの処理 */
static void getvolume(void)
{
	int old, first = 0;

	if(volume == -1) first = 1, volume = def_volume; /* Add Nide */
	old = volume;
	if((volume = soutai0(volume)) == -1) mml_err(15);
	first ? (x_volume = 0) : (x_volume += volume - old);
	if(check_vol() == 0) put_vol(volume);
}

/* pコマンドの処理 */
static void getpanpot(void)
{
	int i;
	int old, first = 0;

	if(panpot == -1) first = 1, panpot = def_panpot; /* Add Nide */
	old = panpot;
	if((panpot = soutai0(panpot)) == -1) panpot = 2;
	 /* マニュアルではパンポットの数値を略すると2を仮定することになってる*/
	first ? (x_panpot = 0) : (x_panpot += panpot - old);
	if(check_panpot() == 0)	put_panpot(panpot);
	if(getbyte(1) != ','){
		rpan1 = 0;
		return;
	}
	(void)getbyte(2);
	rpan1 = xget(&i);
	if(getbyte(1) == ','){
		(void)getbyte(2);
		rpan2 = xget(&i);
	}
}

/* プログラムチェンジそのものを書く関数（バンクセレクトは含まない） */
static void put_progchange(void)
{
	if(run != 0xc){
		putc2(0xc0 + cur_ch, fp2);
		run= 0xc;
	}
	putc2(prog, fp2);
	write_length(0, fp2);
}

/* @コマンドの処理 */
static void progchange(void)
{
	int num, msb, lsb, i;

	if(getbyte(1) == '('){ /* @(msb,lsb,prog) の場合 */
		(void)getbyte(2);
		msb = xget(&i);
		if(i != 0) mml_err(73);
		if(getbyte(1) != ',') mml_err(73);
		(void)getbyte(2);
		lsb = xget(&i);
		if(i != 0) mml_err(73);
		if(getbyte(1) != ',') mml_err(73);
		(void)getbyte(2);
		num = xget(&i);
		if(i != 0) mml_err(73);
		if(getbyte(1) != ')') mml_err(73);
		(void)getbyte(2);
		bs1 = msb;
		bs2 = lsb;
		prog = num + base;
		if(psw == 0){
			put_cntchange0(0, bs1);
			write_length(0, fp2);
			put_cntchange0(32, bs2);
			write_length(0, fp2);
			put_progchange();
		}
		return;
	}

	num = xget(&i);
	if(i == -1 || i == 1){
		base = num;
		return;
	} else if(i == 0){
		prog = num + base;
	} else mml_err(23);
	/* prog &= ~0x80; …不要? */
	if(psw == 0) put_progchange();
}

/* Eコマンドの処理 */
static void getexpression(void)
{
	int old, first = 0;

	if(expression == -1) first = 1, expression = def_expression;
	 /* Add Nide */
	old = expression;
	if((expression = soutai0(expression)) == -1) mml_err(16);
	first ? (x_expression = 0) : (x_expression += expression - old);
	if(check_expr() == 0) put_expr(expression);
}

/* qコマンドの処理 */
/*
  qn0,n1
	n0 == gatetime  マイナスでも良い。
	n1 == gatetime2
  q-4 a4b4 だと、bをキーオン後4step経ってからaがキーオフされる。
  今までのようなqのマイナスコマンドは無くなった。
  今までの q-4 のようにやりたければ、q100,4 のようにすれば良い。つまり、
  第1パラメータの値を非常に大きくする。
*/
static void getgatetime(void)
{
	int num, i;

	num = xget(&i);
	if(i != -2) gatetime = num;
	if(getbyte(1) != ',') return; /* q?の後にコンマがない */
	(void)getbyte(2);
	num = xget(&i);
	if(i == 0) gatetime2 = num;
}

/* Qコマンドの処理 */
static void getgatetimeQ(void)
{
	int num, i;

	num = xget(&i);
	if(num < 0 || num > 8) mml_err(41);
	if(i != -2) gatetimeQ = num;
}

/* Jコマンドの処理 */
static void getktencho(void)
{
	int num, i;

	num = xget(&i);
	if(num < 0) mml_err(42);
	if(i != -2) ktencho = num; /* num!=0ならば強制転調が有効になる */
}

/* kコマンドの処理 */
static void getvelocity(void)
{
	int i;

	if((velocity = soutai0(velocity)) == -1) mml_err(14);
	if(getbyte(1) != ','){
		rvel1 = 0;
		return;
	}
	(void)getbyte(2);
	rvel1 = xget(&i);
	if(getbyte(1) == ','){
		(void)getbyte(2);
		rvel2 = xget(&i);
	}
}

/* krコマンドの処理 */
static void get_randvel(void)
{
	int i;

	(void)getbyte(2);
	randvel1 = xget(&i);
	if(getbyte(1) == ','){
		(void)getbyte(2);
		randvel2 = xget(&i);
	}else{
		randvel2 = 1; /* 第2パラメータ省略時のdefault値 */
	}
}

/*
  Tが来たときの処理。
  TC, TRコマンドがある。
  */
static void tvf(void)
{
	switch(getbyte(0)){
	case 'C':
		tvf_c();
		break;
	case 'R':
		tvf_r();
		break;
	default:
		mml_err(68);
	}
}

static void tvf_c(void)
{
	int old, first = 0;

	if(cutoff == -1) first = 1, cutoff = def_cutoff; /* Add Nide */
	old = cutoff;
	if((cutoff = soutai0(cutoff)) == -1) mml_err(54);
	first ? (x_cutoff = 0) : (x_cutoff += cutoff - old);
	if(check_cutoff() == 0)	put_cutoff(cutoff);
}

static void tvf_r(void)
{
	int old, first = 0;

	if(resonance == -1) first = 1, resonance = def_resonance;
	 /* Add Nide */
	old = resonance;
	if((resonance = soutai0(resonance)) == -1) mml_err(55);
	first ? (x_resonance = 0) : (x_resonance += resonance - old);
	if(check_reso() == 0) put_reso(resonance);
}

static void set_transcale(void) /* KJ, KIコマンド。KJa+のように指定 */
{
	int code, tr = 0, i, no_sharpflat = 0;

	if(getbyte(0) == 'I') tr = -3; /* else getbyte(1)=='J' */
	if(!('a' <= (code = getbyte(0)) && code <= 'h')) mml_err(69);
	if(german_scale){
		switch(code){
		case 'b':
			tr -= 7; no_sharpflat = 1; break;
		case 'h':
			code = 'b'; break;
		}
	} else {
		if(code == 'h') mml_err(69);
	}
	tr += onpu2chogo(code);
	if(!no_sharpflat){
		switch(code = getbyte(1)){
		case '+':
		case '-':
			i = 1;
			(void)getbyte(2);
			if(code == getbyte(1)){
				i++;
				(void)getbyte(2);
			}
			tr += (code == '+' ? 7 : -7) * i;
			break;
		}
	}
	transcale = tr;
}

/*
  Eが来たときの処理。
  E, EX, EE のコマンドがある。
  */
static void code_E(void)
{
	switch(getbyte(1)){
	case 'X':
		(void)getbyte(2);
		getexclusive(0);
		break;
	case 'E':
		(void)getbyte(2);
		getexclusive(1);
		break;
	default:
		getexpression();
	}
}

/* _コマンドの処理 */
static void code__(void)
{
	int i, num;

	if(getbyte(1) == '_'){
		(void)getbyte(2);
		code___();
	}else{
		num = xget(&i);
		if(i == -2) mml_err(21);
		under = num;
	}
}

/* __コマンド，および，*コマンドの処理 */
static void code___(void)
{
	int i, num;
	num = xget(&i);
	if(i == -2) mml_err(20);
	under += num;
}

/* EXコマンドは多分これでOkと思う。 */
/* 例えば EXx41,x10,x42,x12,{x40,x00,x7f,x00},xf7 */
/* とすれば{}内のSUMが}部分に書かれる（はず）*/
/* EX,EE コマンドの処理 */
/* x == 0ならばEX． x == 1ならばEE */

 /* The exclusive buffer used to be written without any bounds check, so a
    long enough EX command overran the stack. */
#define EX_MAX 1024
#define ex_room(n) do{ if(ex_pnt + (n) > EX_MAX) mml_err(71); }while(0)
#define ex_add(v)  do{ ex_room(1); exclusive[ex_pnt++] = (v); }while(0)

static void getexclusive(int x)
{
	int i,j,k,bulk_code;
	int exclusive[EX_MAX];
	int ex_pnt = 0;
	int sum_pnt,bulk_pnt,pntd,pnts;
	int ci,cj;
	int bit_size,bit_ptr;
	unsigned short bit_temp,b2;

	sum_pnt=-1;
	bulk_pnt=-1;
	bulk_code=0;
	bit_size=8;
	bit_ptr=0;
	bit_temp=0;

	for(;;){
		j=xget(&i);
		if (bulk_pnt==-1) {
			if (i!=0) mml_err(19);
		} else {
			if (i==-2) mml_err(19);		/*Bulk中は符号可*/
		}
		if (bit_size==8&&bit_ptr==0) {
			ex_add(j);
		} else {
			if (j>=(1<<bit_size)||j<-(1<<(bit_size-1))) mml_err(19);
			b2=(unsigned short)j;
			b2=b2<<(16-bit_size);	/*左端に詰める*/
			b2=b2>>bit_ptr;		/*使用済分右へ*/
			bit_temp|=b2;
			bit_ptr+=bit_size;
			if (bit_ptr>=8) {
				ex_add(bit_temp>>8);
				bit_temp&=0xff;
				bit_temp<<=8;
				bit_ptr-=8;
			}
			bit_size=8;	/*宣言は１回のみ有効なので戻す*/
		}
	aft:
		i = getbyte(1);
		if (i=='}'){
			if (bit_ptr!=0) {	/*端数ビットは埋めてしまう*/
				ex_add(bit_temp>>8);
				bit_ptr=0;
				bit_temp=0;
			}
			if (bulk_pnt!=-1) {	/*まずバルクから処理*/
				switch (bulk_code) {
				case 1:	/*TR-Rack*/
				  	j=ex_pnt-bulk_pnt;	/*変換前サイズ*/
					k=((j-1)/7)+1;		/*残処理数(=あとバイト挿入されるか)*/
					ex_room(k);
					pntd=ex_pnt+k-1;	/*転送先ポインタ*/
					pnts=ex_pnt-1;		/*転送元ポインタ*/
					i=j%7;
					j=j/7;
					ci=0;
					if (i) {	/*初回転送（端数分）*/
						while (i>0) {
							cj=exclusive[pnts];
							ci*=2;
							ci+=cj/128;
							exclusive[pntd]=cj&0x7f;
							pnts--;
							pntd--;
							i--;
						}
						exclusive[pntd]=ci;
						pntd--;
					}
					while (j>0) {	/*２回目以降転送*/
						ci=0;
						for (i=0;i<7;i++) {
							cj=exclusive[pnts];
							ci*=2;
							ci+=cj/128;
							exclusive[pntd]=cj&0x7f;
							pnts--;
							pntd--;
						}
						exclusive[pntd]=ci;
						pntd--;
						j--;
					}
					ex_pnt+=k;	/*挿入された分加算*/
					break;

				case 2:	/* GS Niblize */
				  	j=ex_pnt-bulk_pnt;	/*変換前サイズ*/
					ex_room(j);
					pnts=ex_pnt-1;
					pntd=ex_pnt+j-1;
					ex_pnt+=j;
					while (j>0) {
						ci=exclusive[pnts--];
						exclusive[pntd--]=ci%16;
						exclusive[pntd--]=ci/16;
						j--;
					}
					break;

				default:
					mml_err(19);
				}
				bulk_code=0;
				bulk_pnt=-1;
			} else {	/*バルク中でない場合、チェックサム*/
				if (sum_pnt!=-1) {	/*｛を見つけている場合のみ処理をする*/
					i=ex_pnt-sum_pnt;
					j=0;
					while (i>0) {
						j=(j+exclusive[sum_pnt++])&0x7f;
						i--;
					}
					ex_add((0x80 - j) & 0x7f);
					sum_pnt=-1;
				}
			}
			(void)getbyte(2);
			i = getbyte(1);
		}
		if(i != ',') break;
		(void)getbyte(2);

		i = getbyte(1);
		if (i=='{') {
			(void)getbyte(2);
			i=getbyte(1);
			if (i=='T'||i=='t') {
				bulk_pnt=ex_pnt;
				bulk_code=1;
				(void)getbyte(2);
			} else if (i=='N'||i=='n') {
				bulk_pnt=ex_pnt;
				bulk_code=2;
				(void)getbyte(2);
			} else {
				sum_pnt=ex_pnt;
				if ((sum_pnt>bulk_pnt)&&bulk_pnt!=-1) mml_err(19);
			}
		}
		i = getbyte(1);
		if(i == '"'){ /* Mod Nide */
			 /* named `text' originally, which shadowed the global
			    message buffer of the same name */
			unsigned char strbuf[1024];
			int got, j;

			got = mml_getstring((char *)strbuf, sizeof(strbuf));
			for(j = 0; j < got; j++) ex_add(strbuf[j]);
			goto aft;
		} else if (i=='B'||i=='b') {
			if (bulk_pnt==-1) mml_err(19);	/*バルク内でのみ有効*/
			(void)getbyte(2);
			bit_size=getbyte(0)-'0';
			if (bit_size<1||bit_size>8) mml_err(19);
			i=getbyte(0);
			if (i!='_') mml_err(19);
		}
	}
	if (bulk_pnt!=-1||sum_pnt!=-1) mml_err(19);	/*｛を閉じていない*/
	if (bit_ptr!=0) {	/*端数ビットは埋めてしまう*/
		ex_add(bit_temp>>8);
		bit_ptr=0;
		bit_temp=0;
	}
	if(x == 0){
		putc2(0xf0, fp2);
		if (ex_pnt>127) {
			putc2((ex_pnt/128)+128,fp2);
			putc2((ex_pnt%128) ,fp2);
		} else {
			putc2(ex_pnt, fp2);
		}
	}
	for(i = 0; i < ex_pnt; i++){
		putc2(exclusive[i], fp2);
	}
	write_length(0, fp2);
}

/* 変数をセットするコマンド。例： Z{z0 = 120, z1 = x3a} */
/* Z{z4 = z3 + 55, Z2 = C - 30} などもＯＫ！ */
static void variables(void)
{
	int code, i;
	int index;

	code = getbyte(1);
	if(code != '{') mml_err(50);
	(void)getbyte(2);

	for(;;){
		code = getbyte(1);
		if(code == '}'){
			(void)getbyte(2);
			break;
		}

		code = getbyte(1);
		if(code != 'z') mml_err(50); /* 'z'がない場合 */
		(void)getbyte(2);

		index = 0;
		for(;;){
			code = getbyte(1);
			if(is_digit(code)){
				index = index * 10 + dtoi(code);
				(void)getbyte(2);
			}else break;
		}
		if(index < 0 || 255 < index)
			mml_err(50); /* z0 〜 z255 の範囲にない */

		code = getbyte(1);
		if(code != '=') mml_err(50); /* =がない場合 */
		(void)getbyte(2);

		var[index] = xget(&i);
		/* if(i == -2) mml_err(50); */
		code = getbyte(1);

		if (code == ','){
			(void)getbyte(2);
			continue;
		}else if(code == '}'){
			(void)getbyte(2);
			break;
		}
		mml_err(50);
	}
}

/* Dコマンド処理（Detune）= RPN#1:FineTuneを使用 */
/* Dn .... nは -64〜63 */
static void detune(void)
{
	int i, num;

	num = xget(&i);
	if(i == -2) mml_err(31);
	num += 0x40;
	ALIGN(num);
	write_rpn(0, 1, num);
}

/* P, Xコマンドの処理（ペダル類のオン・オフ）
   Pn でオン、Xn でオフ。nは省略時0。
     n=0 … ダンパー・ペダル	(コントロールチェンジ64番)
     n=1 … ポルタメント		(		 65番)
     n=2 … ソステヌート		(		 66番)
     n=3 … ソフト・ペダル	(		 67番)
     n=4 … レガート		(		 68番)
   引数onは 127(オン) または 0(オフ) */
static void pedal(int on)
{
	int i, num;

	num = xget(&i);
	if(i == -2) num = 0; /* 省略時は0（ダンパー・ペダル） */
	if(num < 0 || 4 < num) mml_err(72);
	put_cntchange(64 + num, on, 0);
}

/* #ampasandtie on の下で、&によるレガート(タイ/スラー)のためにコントロール
   チェンジ68番(P4/X4のレガート・ペダルと同じ)を送出する。note.cから呼ばれる。
   引数onは真偽値(0以外でオン)。 */
void legato_switch(int on)
{
	put_cntchange(68, on ? 127 : 0, 0);
}

/* '~'による自動ポルタメント連結(doc/CHANGES.md参照)のためにコントロール
   チェンジ65番(P1/X1のポルタメント・ペダルと同じ)を送出する。note.cから
   呼ばれる。引数onは真偽値(0以外でオン)。 */
void portamento_switch(int on)
{
	put_cntchange(65, on ? 127 : 0, 0);
}

/* `<音名> がポルタメントソースノート(keyprocが空の場合。doc/CHANGES.md
   参照)として使われたときに、コントロールチェンジ84番(Portamento
   Control)でソースとなる音階を送出する。note.cから呼ばれる。 */
void portamento_source_note(int onkai)
{
	put_cntchange(84, onkai, 0);
}

/* note.cのnote_tie_entry()から呼ばれる。'~'をノート連結の一部として
   解釈できなかった場合(直後が音符として解釈できる文字でなかった場合)に、
   従来どおり'~n'(BSnの省略形)として処理するためのラッパー。
   do_command()のcase '~'と全く同じ処理を、同じ読み取り位置から行う。 */
void bend_shorthand_tilde(void)
{
	bendset('S');
}

/* Yコマンドの処理（モノモード・オン／ポリモード・オン）
   Yn でコントロールチェンジ126番（Mono Mode On）nを送出。nは省略時1。
     n=0 の場合は例外的にコントロールチェンジ127番（Poly Mode On）0を送出する。
   引数nの範囲は0〜127。 */
static void code_Y(void)
{
	int i, num;

	num = xget(&i);
	if(i == -2) num = 1; /* 省略時は1 */
	if(num < 0 || 127 < num) mml_err(74);
	if(num == 0) put_cntchange(127, 0, 0);
	else put_cntchange(126, num, 0);
}

/* RTコマンドの処理 */
static void setRT(void)
{
	if(getbyte(0) != 'T') mml_err(24);
	gatetime2 = -1;
	/* gatetime = 32767;*/ /* 苦肉の策 */
	/*gatetime2 = 1;*/
}

/* oコマンド(オクターブ)の処理 */
static void oct(void)
{
	int num, i;

	num = xget(&i);
	if(i == -2) mml_err(7);
	if(i != 0){
		octave += num;
	}else{
		octave = num;
	}
}

static int FUIcmd2intl(int code, int *factor)
{
	int retval;

	switch(code){
	case 'E':
		retval = MMLPROC_EXPR; break;
	case 'P':
		retval = MMLPROC_PANPOT; break;
	case 'V':
		retval = MMLPROC_VOL; break;
	case 'W': case 'B':
		retval = MMLPROC_BEND; break;
	case 'C':
		retval = MMLPROC_CUTOFF; break;
	case 'R':
		retval = MMLPROC_RESO; break;
	case 'M':
		retval = MMLPROC_MOD_ON; break;
	case 'A':
		retval = MMLPROC_CPRES; break;
	case 'T':
		retval = MMLPROC_DTEMPO; break;
	 /* これの最大+1がmmlproc.hのKIND_MAX */
	default:
		 /* Set on every path, so a caller cannot observe *factor
		    uninitialised even though it bails out when -1 is returned. */
		*factor = 1;
		return -1;
	}

	 /* code{f,u,i}に入れる振幅を引数の何倍にするか */
	*factor = (code == 'B' ? 128 : each_factor[retval]);
	 /* Bの場合だけはcode{f,u,i}の値はbendの1倍だが引数の128倍 */

	return retval;
}

static void code_F(void)
{
	int y, i, num, j, factor;
	
	y = FUIcmd2intl(getbyte(0), &factor);
	if(y < 0) mml_err(29);

	num = xget(&i);
	if(i == -2 || num < 0) mml_err(25);
	codef[y][0] = num; /* 第1引数 */
	if(num == 0){
		if(psw == 0 && check_proc(y) == 0)
			do_mmlproc(y, ctl_values[y]);
		return;
	}

	if(getbyte(0) != ',' || (num = xget(&i), i == -2) ||
	   getbyte(0) != ',') mml_err(25);
	codef[y][1] = num * factor; /* 第2引数 */
	for(j = 3;;){
		codef[y][j++] = length(); /* 第3,5,…引数 */
		if(getbyte(0) != ',' || (num = xget(&i), i == -2)) mml_err(25);
		codef[y][j++] = num * factor; /* 第4,6,…引数 */
		if(getbyte(1) != ',') break;
		if(j == MAX_F_ARG+1) mml_err(25); /* too long */
		(void)getbyte(2);
	}
	codef[y][2] = (j - 3) / 2;
}

static void code_U(void)
{
	int y, i, num, j, factor;
	long tl = 0;

	y = FUIcmd2intl(getbyte(0), &factor);
	if(y < 0) mml_err(52);

	num = xget(&i);
	if(i == -2 || num <= 0) mml_err(53);
	codeu[y][0] = num; /* 第1引数 */

	if(getbyte(0) != ',' || (num = xget(&i), i == -2) ||
	   getbyte(0) != ',') mml_err(53);
	codeu[y][1] = num * factor; /* 第2引数 */
	for(j = 3;;){
		tl += (codeu[y][j++] = length()); /* 第3,5,…引数 */
		if(getbyte(0) != ',' || (num = xget(&i), i == -2)) mml_err(53);
		codeu[y][j++] = num * factor; /* 第4,6,…引数 */
		if(getbyte(1) != ',') break;
		if(j == MAX_U_ARG+1) mml_err(53); /* too long */
		(void)getbyte(2);
	}
	codeu[y][2] = (j - 3) / 2;
	if(psw == 0){
		setcode_U(tl, y); /* いきなりmmlprocに値を書き込む */
	}
}

static void code_I(void)
{
	int y, i, num, j, factor;

	y = FUIcmd2intl(getbyte(0), &factor);
	if(y < 0) mml_err(30);

	for(j = 0;;){
		num = xget(&i);
		if(i == -2) mml_err(30);
		codei[y][j++] = num;
		if(',' != getbyte(1)) break;
		if(j == 5) mml_err(30);
		(void)getbyte(2);
	} /* この時点で j:引数個数 num:最後の引数 */

	if(j == 1 && num == 0){ /* パラメータが1つだけでかつそれが0の時 */
		if(psw == 0 && check_proc(y) == 0)
			do_mmlproc(y, ctl_values[y]);
		return;
	}

	if(j < 4) mml_err(30); /* パラメータが4個より少ないのはだめ */
	codei[y][0] *= factor;
	if(j == 4) codei[y][4] = 0; /* 5個目を略すると0 */
	if(codei[y][0] == 0 || codei[y][1] <= 0 || codei[y][2] < 0 ||
	   codei[y][3] <= 0 || codei[y][4] < 0) mml_err(59);
}

/* wコマンド(キーオフベロシティー)の処理 */
static void keyoffvelocity(void)
{
	int num, i;

	num = xget(&i);
	if(i == -2) mml_err(37);
	if(i != 0){
		koff += num;
	}else{
		koff = num;
	}
}

/* MON, MOF, M の処理 */
static void code_M(void)
{
	int old, first = 0;
	int i;
	int code;

	if(mod_on == -1) first = 1, mod_on = def_mod_on; /* Add Nide */
	old = mod_on;	
	code = getbyte(1);
	if(code == 'O'){
		(void)getbyte(2);
		code = getbyte(0);
		if(code == 'N'){
			mod_on = xget(&i);
			/* mod_on &= ~0x80; …不要? */
			if(i != 0) mml_err(26);
		} else if(code == 'F'){
			mod_on = 0;
		} else mml_err(27);

		first ? (x_mod_on = 0) : (x_mod_on += mod_on - old);
		mod_delay = -1;

		if(check_mod() == 0) put_mod(mod_on);
	}else{
		mod_on = 0;
		mod_delay = xget(&i);
		if(i != 0) mml_err(28);
		if(getbyte(0) != ',') mml_err(28); /* y?の後にコンマがない */
		mod_after = xget(&i);
		/* x_mod_on = 0; 入れなくてよい */
		if(i != 0) mml_err(9); /* 値が変な場合 */
	}
}

/* 音長の取得 */
int length(void)
{
	int i, jq = 0, num = 0, futen_pow2;

	i = getbyte(1);
	if(i == '%'){
		jq = 1;
		(void)getbyte(2);
		i = getbyte(1);
	}
	if(!is_digit(i)){
		num = clength; /* lの値を使う */
		jq = 1;
	} else {
		do {
			num = num * 10 + dtoi(i);
			(void)getbyte(2);
			i = getbyte(1);
		} while(is_digit(i));
	}
	for(futen_pow2 = 1; i == '.'; futen_pow2 <<= 1){
		(void)getbyte(2);
		i = getbyte(1);
	}

	if(jq){
		num = (int)((long)num * (2*futen_pow2-1) / futen_pow2);
	} else {
		if(num) num = (int)((long)timebase * 4 *
				(2*futen_pow2-1) / futen_pow2 / num);
	}

	if(i == '^'){
		(void)getbyte(2);
		num += length();
	}else if(i == '-'){
		(void)getbyte(2);
		num -= length();
	}
	if(num < 0) mml_err(2);
	return num;
}

/*
  数値の取得．

  符号付きの数値を戻り値で返す。
  符号は、*i にも入れる。つまり、符号が付いている場合には、
  *i = -1 または *i = 1 となる。（相対指定に対する配慮です）
  数値が省略されていて無い場合、つまり、符号も数値もない
  場合、*i = -2 となる。（この時の戻り値は0）
  それ以外では *i = 0 となる。
  符号だけしかない場合、つまり、-および+は、-1および+1とみなす。
  x??で16進数の指定ができる．ただし，xの後は必ず2バイトで指定する。
  xの後の3バイト目以降は無視する。符号付きの16進数も指定できる。-x2fなど
*/
int xget(int *i)
{
	int index;
	int num = 0;
	int j;
	int code = getbyte(1);

	*i = 0;
	if(code == '-'){
		*i = -1;
		(void)getbyte(2);
		code = getbyte(1);
	}else if(code == '+'){
		*i = 1;
		(void)getbyte(2);
		code = getbyte(1);
	}

	switch(code){
	case '&': /* n分音符指定の場合 */
		(void)getbyte(2);
		num = length();
		break;
	case 'z': /* 変数の場合 */
		(void)getbyte(2);

		index = 0;
		for(;;){
			code = getbyte(1);
			if(is_digit(code)){
				index = index * 10 + dtoi(code);
				(void)getbyte(2);
			}else break;
		}
		if(index < 0 || 255 < index) mml_err(51);
		num = var[index];
		break;
	case 'C': /* チャンネル番号の場合 */
		num = cur_ch + 1;
		(void)getbyte(2);
		break;
	case 'R': /* ローランド風チャンネル番号の場合 */
		if(cur_ch == 9) num = 0;
		else if(cur_ch < 9) num = cur_ch + 1;
		else num = cur_ch;
		(void)getbyte(2);
		break;
	case 'x': /*「x」で始まる16進数 */
		goto get_hex;
	case '0':
		(void)getbyte(2);
		code = getbyte(1);
		if(code == 'x'){ /*「0x」で始まる16進 */
		get_hex:
			(void)getbyte(2);
			if((code = getbyte(1)) == '{'){
				(void)getbyte(2);
				while((code = getbyte(0)) != '}'){
					if(!is_xdigit(code)) mml_err(12);
					num = num * 16 + xtoi(code);
				}
			} else {
				for(j = 0; j < 2; j++){
					code = getbyte(0);
					if(!is_xdigit(code)) mml_err(12);
					num = num * 16 + xtoi(code);
				}
			}
			break;
		} else if(!is_digit(code)){ /* 0だけ */
			break;
		}
		/* 普通の10進数字なので case '1' 以下の処理へ落ちる */
		/* FALLTHROUGH */
	case '1':
	case '2':
	case '3':
	case '4': /* 通常の数値 */
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		for(;;){
			(void)getbyte(2);
			num = num * 10 + dtoi(code);
			code = getbyte(1);
			if(!is_digit(code)) break;
		}
		break;
	default:
		if(*i == 0){ /* 符号も数値も無い場合 */
			*i = -2;
			return 0;
		}else{ /* 符号だけの場合 */
			return *i;
		}
	}
	if(*i != 0) num = *i * num;
	code = getbyte(1);
	if(code == '+'){
		(void)getbyte(2);
		num += xget(&j); /* 再帰呼び出し */
	}else if(code == '-'){
		(void)getbyte(2);
		num -= xget(&j); /* 再帰呼び出し */
	}
	return num;
}

void write_length(long i, fileptr fp) /* 絶対長 -> 可変長 */
{
#define j 0x80L
	tstep += i;

	fgetpos2(fp, &lastlenpos);
	 /* 直前の従属トラックが主トラックより長い場合 write_length()が負の
	    長さを書き出そうとすることがある そうなると正常なSMFにならないので
	    ここでそれをチェック
	    但し 本当はその条件が成り立てば単純にエラーにしたいがそうはいかない
	    直前の従属トラックが主トラックより長くても 現在の従属トラックの
	    先頭部が無音なら 先にwrite_length()が書いた負の長さを上書きして
	    SMFとしては正常になる場合がある しかもそれを積極的に利用した
	    sampleファイルができてしまっているので 過去互換のためにはそういう
	    ケースは救済しなくてはならない
	    そこで 負の長さを書いた場所をlast_oddlen.pos(普段-1)に記録しておき
	    それが-1にならないうちにlastlenposが進んでしまったりトラックの
	    終わりに来たりした場合に初めてエラーとする */
	if(last_oddlen.pos >= 0 && last_oddlen.pos < lastlenpos){
		cur_line = last_oddlen.lineno;
		mml_err(70);
	}
	if(i < 0){ /* 長さが負 */
		if(last_oddlen.pos < 0){
			 /* 同一箇所に負の長さを初めて書こうとした行番号
			    だけ記録 */
			last_oddlen.pos = lastlenpos;
			last_oddlen.lineno = cur_line;
		}
	} else { /* 長さが正常 last_oddlen.posの記録をクリア */
		last_oddlen.pos = -1;
	}

	lastlen = i;

	if(i < j){
		putc2((char)i, fp);
	}else if(i / j < j){
		putc2((char)(i / j + j), fp);
		putc2((char)(i % j), fp);
	}else if(i / j / j < j){
		putc2((char)(i / j / j + j), fp);
		putc2((char)((i % (j * j)) / j + j), fp);
		putc2((char)((i % (j * j)) % j), fp);
	}else{
		putc2((char)(i / (j * j * j) + j), fp);
		putc2((char)((i % (j * j * j)) / (j * j) + j), fp);
		putc2((char)(((i % (j * j * j)) % (j * j)) / j + j), fp);
		putc2((char)(((i % (j * j * j)) % (j * j)) % j), fp);
	}
#undef j
}

static char *err_msgs[] = {
	NULL,	/* 0 */

	"loop end ']' is missing",
	"length parameter is negative",
	"loop start '[' is missing",
	"loop out ':' is wrong",
	"macro name '$?' is wrong",

	"macro call '$?' is wrong",
	"octave 'o?' is wrong",
	"beat 'BT?,?' is wrong",
	"control change 'y?,?' is wrong",
	"tempo 't?' is wrong",		/* 10 */

	"bend range 'BR?' is wrong",
	"parameter 'x??\047 or '0x??\047 is wrong",
	  /* to avoid being treated ??' as a trigraph, we use \047 */
	"no such a command 'B?'",
	"velocity 'k?' is wrong",
	"volume 'v?' is wrong",

	"expression 'E?' is wrong",
	"bend set 'BS?' is wrong",
	"relative bend set 'BB?' is wrong",
	"exclusive 'EX?' is wrong",
	"relative transfer '__?' or '*?' is wrong",	/* 20 */

	"transfer '_?' s wrong",
	"bank select 'H?,?' is wrong",
	"program change '@?' is wrong",
	"no such a command 'R?'",
	"illegal (or too long) arg of command 'F?'",

	"modulation 'MON?' is wrong",
	"no such a command 'MO?'",
	"modulation 'M?' is wrong",
	"command 'F?' is wrong",
	"command 'I?' is wrong",	/* 30 */

	"detune 'D?' is wrong",
	"NRPN 'N?' is wrong",
	"Too many parameters in NRPN 'Nn1,n2,n3'",
	"command 'BW?' is wrong",
	"command '=?' is wrong",

	"Master Part not found",
	"key off velocity 'w?' is wrong",
	"command 'K?' is wrong",
	"poli press 'A?' is wrong",
	"channel press 'G?' is wrong",		/* 40 */

	"command 'Q?' is wrong",
	"command 'J?' is wrong",
	"loop nest too deep",
	"macro call '$?' is wrong (in macro)",
	"undefined macro call '$?' (in macro)",

	"loop nest too deep (in macro)",
	"loop start '[' is missing (in macro)",
	"loop end ']' is missing (in macro)",
	"illegal string concatenation",
	"command 'Z{....}' is wrong",		/* 50 */

	"variable z? is wrong",
	"command 'U?' is wrong",
	"illegal (or too long) arg of command 'U?'",
	"command 'TC?' is wrong",
	"command 'TR?' is wrong",

	"track name is too long",
	"text too long",
	"command W?\"....\" is wrong",
	"Illegal arg of command 'I?'",
	"too many tracks",		/* 60 */

	"memory insufficient",
	"string end '\"' is missing (in macro)",
	"recursive macro call",
	"cannot interpret '", /* + Msg + "'" */
	"no such macro '", /* + Msg + "'" */

	"':' not within loop",
	"track name illegal",
	"command 'T?' is wrong",
	"command 'KJ?' or 'KI?' is wrong",
	"can't align sub-track (previous sub-track was longer than master track)", /* 70 */

	"exclusive data too long",
	"pedal 'P?' or 'X?' is wrong",
	"program change '@(?,?,?)' is wrong",
	"mono mode 'Y?' is wrong",
	"backquote note off '`?' has no matching sounding note",	/* 75 */
	"'&'/'~' chain cannot contain 'K' or 'A'",
	"'&'/'~' chain boundary cannot use a negative local gatetime",
	"backquote portamento source note '`?' cannot take a length or"
	  " comma argument",
};

static char *warn_msgs[] = {
	NULL,
	"cannot interpret '", /* + Msg + "'; ignored" */
	"loop out '/' is obsolete (please use ':' instead)",
	"using '^' instead of 'r' is obsolete (use 'r')"
};

void mml_err(int i)
 /* iが負の場合はエラーでなく警告。
    iの値によっては呼び出し時のMsgの内容がメッセージに入る */
{
	int err_line;
	char *err_file;

	if(i > 0){
		text_cat("ERROR!  ");
		text_cat(err_msgs[i]);
	} else {
		text_cat("Warning: ");
		text_cat(warn_msgs[-i]);
	}
	switch(i){
	case 64:
	case 65:
		text_cat(Msg);
		text_cat("'");
		break;
	case -1:
		text_cat(Msg);
		text_cat("'; ignored");
		break;
	}

	err_line = cur_line, err_file = NULL;
	if(ppinfo . fname != NULL){
		err_line += ppinfo.line - ppinfo.actual_line - 1;
		if(*ppinfo . fname) err_file = ppinfo . fname;
	}
	if(err_file == NULL){
		msg_printf(" in line %d.\n", err_line);
	} else {
		msg_printf(" in line %d. <%s>\n", err_line, err_file);
	}
	text_cat(Msg);
	if(i < 0){
		msg_flush();
	} else {
		remove_file_and_owari();
	}
}

void write_header(void)
{
	msg_printf("trk %d%s: ", tnum, trkname_str(talf));
	text_cat(Msg);
	if(++trknum >= MAXTRKNUM) mml_err(60);
	smftrkheader(fp2, &trksize);
}

static void get_trackname(void)
{
	switch(mml_getstring(trackname, (int)sizeof(trackname))){
	case -1:
		return;
	case (int)sizeof(trackname):
		mml_err(56); /* never returns */
		break;
	}
	write_trackname();
}

static void write_trackname(void)
{
	size_t i, len = strlen(trackname);

	putc2(0xff, fp2);
	putc2(3, fp2);
	putc2((int)len, fp2);
	for(i = 0; i < len; i++){
		putc2(trackname[i], fp2);
	}
	write_length(0, fp2);
}

static void texts(void)
{

	switch(getbyte(0)){
	case 'T': /* ff 01 テキスト */
		put_midistring(1);
		break;
	case 'I': /* ff 04 楽器名 */
		put_midistring(4);
		break;
	case 'L': /* ff 05 歌詞 */
		put_midistring(5);
		break;
	case 'M': /* ff 06 Marker */
		put_midistring(6);
		break;
	case 'C': /* ff 07 Cue point */
		put_midistring(7);
		break;
	default:
		mml_err(58);
	}
}

static void put_midistring(int x)
{
	size_t i, len;
	char t[MAX_TEXT_STR + 1];

	switch(mml_getstring(t, (int)sizeof(t))){
	case -1:
		mml_err(58); /* never returns */
		break;
	case (int)sizeof(t):
		mml_err(57);
		break;
	}
	len = strlen(t);

	putc2(0xff, fp2);
	putc2(x, fp2);
	putc2((int)len, fp2);
	for(i = 0; i < len; i++) putc2(t[i], fp2);
	write_length(0, fp2);
}

static int mml_getstring(char *buf, int length)
 /* 戻り値 -1:最初が「"」でなかった 0以上:正常取得した文字列の長さ
    文字列末には'\0'を書く。戻り値はその'\0'を含まない長さ。文字列が長過ぎれば
    lengthバイトだけ取得し(最後の'\0'マークを書かずに)lengthを返す */
{
	int i, j, c, d;

	d = getbyte(1);
	if(d != '"') return -1;
	(void)getbyte(2);
	for(i = 0; ; i++){
		if(i == length) return i;
		switch(d = Getbyte(0, 1)){
		case '"':
			if((d = getbyte(1)) == '+'){ /* 文字列連接 */
				(void)getbyte(2);
				if((d = getbyte(0)) != '"') mml_err(49);
				i--; continue;
			}
			 /* FALLTHROUGH */
		case -1:
			buf[i] = 0;
			return i;
		case '\\':
			d = Getbyte(0, 1);
			if(d == 'x'){
				d = 0;
				for(j = 0; j < 2; j++){
					c = Getbyte(1, 1);
					if(!is_xdigit(c)) break;
					(void)getbyte(2);
					d <<= 4, d |= xtoi(c);
				}
			} else
			if(is_octal(d)){
				d = dtoi(d);
				for(j = 0; j < 2; j++){
					c = Getbyte(1, 1);
					if(!is_octal(c)) break;
					(void)getbyte(2);
					d <<= 3, d |= dtoi(c);
				}
			} else {
				d = escchr(d);
			}
			buf[i] = d;
			break;
		default:
			buf[i] = d;
			if(mskanji && ismskanji1(d)){
				if(++i == length) return i;
				buf[i] = Getbyte(0, 1);
			}
		}
	}
}

int check_proc(int kind) /* kind: 0〜MAX_KIND-1 */
{
	int i;

	for(i = 0; i < mmlproc_ptr; i++){
		if(mmlproc[i].kind == kind) return 1;
	}
	return 0;
}
