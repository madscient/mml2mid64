# 変更履歴（STATUS）

本フォークの作業記録です。新しいものが上に来ます。
エンドユーザー向けの「オリジナル版との差分」は [CHANGES.md](CHANGES.md) にあります。

## 現状

| 項目 | 内容 |
| --- | --- |
| ベース | mml2mid Version 5.30b（門田暁人、藤井秀樹（MKR）、黒田久泰、新出尚之） |
| 入手元 | <https://www.vector.co.jp/soft/dl/unix/art/se102432.html> |
| 対応環境 | Windows / Linux / macOS（64bit） |
| ビルド | CMake（推奨）または `src/makefile` |
| 回帰テスト | サンプル曲が非同梱のため既定では無効。持ち込めば 33/33 合格（`ctest`） |
| バージョン表示 | `5.30`（`src/mml2mid.c` の `VER`、`CMakeLists.txt` の `VERSION`） |

検証済みツールチェーン:

| ツールチェーン | プラットフォーム | 結果 |
| --- | --- | --- |
| MSVC 19.51（VS 18） | Windows x64 | `/W3` で警告ゼロ、テスト 33/33 合格 |
| GCC 10 | Debian x86-64 | `-O2 -Wall -Wextra` で警告ゼロ、テスト 33/33 合格 |
| clang 21（clang-cl） | Windows x64 | エラー・実質的な警告なし |

生成MIDIは、決定的な32サンプル全てで上記ビルド間がバイト単位で一致します。


## サンプル曲の削除と非公式フォークであることの明示

`sample/` を削除。各曲の著作権はそれぞれの作曲者に帰属する第三者の著作物であり、
自由利用が明示されているのは門田氏作の曲のみのため、再配布しないことにした。

オリジナルのライセンス（`org-doc/copyrigh.txt` と `header.txt`）で明示されている
のは「転載可」「フリーソフト＝無料」「無保証」のみで、**改変版の再配布は許諾されて
いない**。このためMIT等での再ライセンスもできない（サブライセンス権がないため）。
README にこの点と、本フォークが非公式・無認可であることを明記した。

付随する対応:

- サンプル曲が無い場合、CMake はその旨を表示してテストを無効化する（ビルドは正常）。
- `test/baseline.sha256` と `test/run-baseline.sh` は残置。オリジナル配布物の
  `sample/` をツリー直下に置けばそのまま検証できる。
- `test/README.md` に入手方法を記載。

**注意:** git履歴には削除前の `sample/` が残っているため、公開リポジトリから完全に
除去するには履歴の書き換え（`git filter-repo` 等）とforce pushが必要。

## 未リリース

### ドキュメントの再編（2026-08-06）

- オリジナル配布物の `doc/` を **`org-doc/`** にリネーム。内容は一切変更せず、
  オリジナルのドキュメントの置き場とする。
- 本フォークで書いたドキュメントを新しい `doc/` 配下に Markdown で置く方針とし、
  [`doc/STATUS.md`](STATUS.md)（このファイル）と [`doc/CHANGES.md`](CHANGES.md) を新設。
- `README.md` / `BUILD.md` / `.gitattributes` の `doc/` への参照を更新。

`org-doc/` は原則として編集しない。仕様変更の記述は `doc/CHANGES.md` 側に書く。
このため `P` / `X` の拡張（下記）も `org-doc/mml2mid.txt` と `org-doc/command.txt`
には反映していない。

### `P` / `X` コマンドの拡張（2026-08-06）

`P` / `X` が 0〜4 の引数を取れるようにし、ダンパー・ペダル以外のペダル系
コントロールチェンジ（65〜68番）にも対応した。仕様は
[CHANGES.md](CHANGES.md#mmlの拡張) を参照。

実装（[`src/mmlproc.c`](../src/mmlproc.c)）:

- `pedal(int on)` を新設。引数を `xget()` で取り（省略時は0）、範囲外なら新エラー
  72番でエラーとし、`put_cntchange(64 + n, on, 0)` を出力する。
  引数の解析を `xget()` に任せているので、変数・16進・加減算の記法がそのまま使える。
- `mmlcmd()` の `case 'P'` / `case 'X'` を `pedal(127)` / `pedal(0)` に置き換え。
- `err_msgs[]` に 72番 `"pedal 'P?' or 'X?' is wrong"` を追加。

確認したこと:

- `P`, `X`, `P0`〜`P4`, `X0`〜`X4` の12通りをコンパイルし、出力が
  CC64=127 / CC64=0 / …… / CC68=127 / CC68=0 になることをバイト列で確認。
- `P5` と `X-1` が新エラーで停止すること（終了コード1）。
- `ctest -C Release` が 33/33 合格すること。引数なしの `P` / `X` は従来どおり
  CC64 なので、既存サンプルの生成MIDIは不変。

## 2026-08-06 — 初期モダナイズ

オリジナルの mml2mid 5.30b を64bit環境でビルドできるようにした一連の作業。
生成MIDIの内容は変えていない（根拠は [test/README.md](../test/README.md)）。
技術的な詳細は [BUILD.md](../BUILD.md) にある。

### `a46ca58` — ドキュメントをEUC-JPからUTF-8に変換

ドキュメント（`*.txt`、`src/mml2mid.1`）を文字列を1文字も変えずにUTF-8へ変換。
`sample/*.mml` と `org-doc/tr-rack.mml` はコンパイラへの入力データなので
EUC-JPのまま据え置き（変換すると生成MIDIのバイト列が変わるため）。

### `3385905` — フォークであることの明示

`README.md` と `BUILD.md` に、本ツリーが mml2mid 5.30b のフォークである旨、
オリジナル作者、入手元を記載。

### `2841323` — 初期コミット（64bit化）

ビルドシステム:

- `CMakeLists.txt`（MSVC / GCC / Clang対応）を追加。`src/makefile` を
  `install` / `check` ターゲットを持つ移植性のあるMakefileに書き直し。
- `-DUNIX` を廃止し、`_WIN32` の有無でプラットフォームを判別。
- 旧来の `makefile.bcc` / `makefile.egc` / `makefile.lcc` および `file.asm`
  （16bit DOS用アセンブラ）はどのビルドからも参照されない。

64bit対応:

- `Fpos_t` が `long` だった。ファイル位置はポインタ同士の差から計算されるため、
  `long` が32bitであるWin64では算出された位置が毎回切り詰められていた。
  `ptrdiff_t` に変更し、`fseek2` / `ftell2` / `smftrkend` も追随。
- `qsort()` にプロトタイプなし関数ポインタ型へキャストした比較関数を渡していた
  （未定義動作）。正しい `(const void *, const void *)` 署名に修正。

移植性レイヤ:

- `win.h` を `compat.h` に置き換え、`UNIX` / `WINDOWS` / `MSDOS` / `BCC` /
  `LSI_C` の手動切り替えをWin32とPOSIXの実際の分岐に整理。feature-testマクロを
  設定するため、各 `.c` の最初の `#include` である必要がある。
- DOS用ファーポインタ（LSI-C）版とBorland C版のメモリ上ファイル層、Win16 GUI用の
  エントリポイント（`mml_smf`、`setjmp`/`longjmp` によるエラー処理、`hWnd3`、
  `InvalidateRect`/`UpdateWindow` のダミー）を削除。
- `isascii()`（標準Cではなく、MSVCには存在しない）を、`EOF` を正しく偽と判定する
  明示的な範囲チェックに置き換え。
- `read()` / `write()` が短い転送と `EINTR` をループするように修正
  （`<errno.h>` 未インクルードのため `EINTR` の再試行が無効化されていた）。
- パイプからの読み込みに対応（`fdopen2` が `fstat` のサイズ報告に依存しなくなった）。

修正した不具合:

- **`EX` / `EE` でのスタックバッファオーバーフロー** — `getexclusive()` が
  `int exclusive[1024]` に境界チェックなしで書き込んでいた。新エラー
  71番 `"exclusive data too long"` を追加。
- **乱数ベロシティでのゼロ除算** — `kr1`（または `kr n,0`）で除数が0になった。
- **`text[]` のオーバーフロー** — 8KB固定バッファへ `strcat` で追記しており、
  `S` コマンドの多いトラックで溢れた。`text_cat()` に集約し、`Msg[]` への
  `sprintf` / `wsprintf` はすべて `snprintf` に変更。
- **タイトル/著作権表示のoff-by-one** — 長さは1バイトで書き出されるのに256を許して
  おり、長さ0として書き出されていた。上限を255に修正。
- **早期エラー時のヌル参照** — `free_all_macros()` がエラー経路から呼ばれ、
  `init_all_macros()` の実行前に `mcrstr` を参照していた。
- **`realloc` のリーク** — `reallocmacro()` と `getLine()` で、結果をブロックへの
  唯一のポインタへ代入していた。
- `put_cpres` の宣言が `static` で定義が非staticだった。
- `setcode_I()` の死んだ状態 `dir[]`（書くだけで読まれない）を削除。

テスト:

- `sample/*.mml` をすべてコンパイルしてSHA-256を照合する回帰テスト
  （`test/baseline.sha256`、`ctest`、`make check`）を追加。
- ベースラインは同梱の `.mid`（オリジナル版が生成したもの）と照合して検証済み。
  32件中28件がバイト単位で完全一致。残り4件の原因は [test/README.md](../test/README.md)。
