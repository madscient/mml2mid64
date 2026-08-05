# mml2mid

MML（Music Macro Language）で書かれたテキストファイルを読み込み、標準MIDIファイル
（SMF）を出力するコンパイラです。

## 本リポジトリについて（フォーク元）

**本リポジトリは、オリジナルの mml2mid Version 5.30b からのフォークです。**

オリジナルは 門田暁人・藤井秀樹・黒田久泰・新出尚之 の各氏による作品（1993〜2011年）
で、本リポジトリはそのソースを現代の64bit環境（Windows / Linux / macOS）で
ビルドできるようにモダナイズしたものです。
**生成されるMIDIの内容はオリジナルと変わりません。**

| 項目 | 内容 |
| --- | --- |
| フォーク元 | mml2mid Version 5.30b |
| オリジナル作者 | 門田暁人、藤井秀樹（MKR）、黒田久泰、新出尚之 |
| 入手元 | <https://www.vector.co.jp/soft/dl/unix/art/se102432.html> |
| オリジナルのWebページ | `http://hpc.jp/~mml2mid/`（現在はアクセス不可） |

オリジナル配布物に同梱されていたドキュメント（`org-doc/`）、サンプル曲（`sample/`）、
プリプロセッサ（`mmlpp/`）、GUIフロントエンド（`tk/`）、および配布物の説明
（[readme.txt](readme.txt)）は、そのまま同梱してあります。

本フォークでの変更点は、次の3つに分けて記載しています。

| ドキュメント | 内容 |
| --- | --- |
| [doc/CHANGES.md](doc/CHANGES.md) | **オリジナル版との差分**（エンドユーザー向け。MMLの拡張など） |
| [doc/STATUS.md](doc/STATUS.md) | 変更履歴（開発側の作業記録） |
| [BUILD.md](BUILD.md) | ビルド方法とモダナイズの技術的詳細 |

概要は「[モダナイズの概要](#モダナイズの概要)」にもあります。

## クイックスタート

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

`sample.mml` として下記の1行だけのファイルを用意し、

```
A C1 cdefg
```

コンパイルすると `sample.mid` ができ、「ドレミファソ」と鳴ります。

```sh
mml2mid sample.mml
```

先頭の `A` がトラック名、`C1` がMIDIチャンネル1の指定、続く `cdefg` が音符です。
MMLの詳しい書き方は [org-doc/mml2mid.txt](org-doc/mml2mid.txt)、コマンド一覧は
[org-doc/command.txt](org-doc/command.txt) を参照してください。これらはオリジナル版
時点の内容なので、本フォークで拡張した箇所は [doc/CHANGES.md](doc/CHANGES.md) で
読み替えてください。

## ビルド

プラットフォームは自動判別されるため、設定用のスイッチは不要です。

### CMake（全プラットフォーム、推奨）

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Windows の Visual Studio ではマルチコンフィグ構成になります。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

インストールは `cmake --install build --prefix /usr/local` です。

### Makefile（Linux / macOS / *BSD / MinGW）

```sh
cd src
make
make check          # 全サンプルをコンパイルしてハッシュを照合
sudo make install   # 既定の PREFIX は /usr/local
```

### 必要なもの

- C11コンパイラ。既定では `gnu11` を選択します。本プログラムはPOSIX
  （`open` / `read` / `isatty` / `strdup`）を使用しており、厳密な `-std=c11`
  ではglibcがこれらの宣言を隠してしまうためです。`src/compat.h` 自身も
  feature-test マクロを設定しているので、厳密なC11でもビルドできます。
- 外部ライブラリへの依存はありません。

### 動作確認済みのツールチェーン

| ツールチェーン | プラットフォーム | 結果 |
| --- | --- | --- |
| MSVC 19.51（VS 18） | Windows x64 | `/W3` で警告ゼロ、テスト 33/33 合格 |
| GCC 10 | Debian x86-64 | `-O2 -Wall -Wextra` で警告ゼロ、テスト 33/33 合格 |
| clang 21（clang-cl） | Windows x64 | エラー・実質的な警告なし |

上記2つのフルビルドは、決定的な32サンプル全てでバイト単位で同一のMIDIを出力します。

## テスト

`sample/*.mml` をすべてコンパイルし、`test/baseline.sha256` に記録された
SHA-256と照合します。生成MIDIが変化する変更は即座に検出されます。

```sh
cd build && ctest            # CMake 3.20 未満
ctest --test-dir build       # CMake 3.20 以降
```

```powershell
ctest --test-dir build -C Release   # Windows / Visual Studio
```

このベースラインは自己参照ではなく、同梱の `.mid`（オリジナル版が生成したもの）
と照合して検証済みです。**32件中28件がバイト単位で完全一致**します。
一致しない4件の原因と根拠は [test/README.md](test/README.md) に記載しています
（うち3件は同梱`.mid`生成時のテンポ値の丸め方の違い、1件は乱数ベロシティによる
非決定性で、いずれも本リポジトリでの退行ではありません）。

## ディレクトリ構成

| パス | 内容 |
| --- | --- |
| `src/` | ソースプログラムと Makefile |
| `org-doc/` | **オリジナルのドキュメント類**（MMLリファレンス、テクニック集など。内容は変更していません） |
| `doc/` | 本フォークのドキュメント（[CHANGES.md](doc/CHANGES.md)、[STATUS.md](doc/STATUS.md)） |
| `sample/` | サンプル曲のMMLデータと標準MIDIファイル |
| `test/` | 回帰テスト用のベースラインとスクリプト |
| `cmake/` | ctest から呼ばれるテスト実行スクリプト |
| `mmlpp/` | mml2mid用プリプロセッサ（Perlスクリプト） |
| `tk/` | Tcl/Tk製のGUIフロントエンド tkmml2mid |

## モダナイズの概要

詳細および修正した不具合の一覧は [BUILD.md](BUILD.md)、時系列の記録は
[doc/STATUS.md](doc/STATUS.md) にあります。主な点は次の通りです。

### ビルドシステム

- `CMakeLists.txt`（MSVC / GCC / Clang対応）を追加し、`src/makefile` を
  `install` / `check` ターゲットを持つ移植性のあるMakefileに書き直しました。
- `-DUNIX` を廃止し、`_WIN32` の有無でプラットフォームを判別します。
- 旧来の `makefile.bcc` / `makefile.egc` / `makefile.lcc` および `file.asm`
  （16bit DOS用アセンブラ）は、どのビルドからも参照されなくなりました。
  歴史的資料として残してありますが、削除して構いません。

### 64bit対応

- `Fpos_t` が `long` でした。ファイル位置はポインタ同士の差から計算されるため、
  `long` が32bitであるWin64では、算出された位置が毎回切り詰められていました。
  現在は `ptrdiff_t` です。
- `qsort()` にプロトタイプなし関数ポインタ型へキャストした比較関数を渡していました
  （未定義動作であり、新しい規格では受け付けられません）。

### 移植性レイヤ

- `win.h` を `compat.h` に置き換え、旧来の `UNIX` / `WINDOWS` / `MSDOS` /
  `BCC` / `LSI_C` を手動で切り替える構成から、Win32とPOSIXの実際の分岐に整理しました。
  feature-testマクロを設定するため、各 `.c` ファイルの**最初の** `#include`
  である必要があります。
- DOS用ファーポインタ（LSI-C）版とBorland C版のメモリ上ファイル層、および
  Win16 GUI用のエントリポイント（`mml_smf`、`setjmp`/`longjmp` によるエラー処理、
  `hWnd3`、`InvalidateRect`/`UpdateWindow` のダミー）を削除しました。
- `isascii()`（標準Cではなく、MSVCには存在しない）を、`EOF` を正しく偽と判定する
  明示的な範囲チェックに置き換えました。

### 修正した不具合

モダナイズの過程で見つかった実際のバグです（詳細は [BUILD.md](BUILD.md)）。

- **`EX` / `EE` でのスタックバッファオーバーフロー** — `getexclusive()` が
  `int exclusive[1024]` に境界チェックなしで書き込んでいました。
- **乱数ベロシティでのゼロ除算** — `kr1`（または `kr n,0`）で除数が0になりました。
- **`text[]` のオーバーフロー** — 8KB固定バッファへ `strcat` で追記しており、
  `S` コマンドの多いトラックで溢れました。
- **タイトル/著作権表示のoff-by-one** — 長さは1バイトで書き出されるのに256を許して
  おり、長さ0として書き出されていました。
- `<errno.h>` が未インクルードのため `EINTR` の再試行が無効化されていた問題、
  パイプからの読み込みが空になる問題なども修正しました。

### MMLの拡張

生成MIDIを変えないことを原則としていますが、1点だけ機能を追加しています。

- **`P` / `X` コマンドが 0〜4 の引数を取れるようになりました。** ダンパー・ペダル
  （CC64）に加え、ポルタメント（65）、ソステヌート（66）、ソフト・ペダル（67）、
  レガート（68）のオン・オフができます。引数を省略すると従来どおりダンパー・ペダル
  なので、既存のMMLへの影響はありません。詳細は
  [doc/CHANGES.md](doc/CHANGES.md#mmlの拡張) を参照してください。

## 文字コードについて

オリジナルの配布物は全体が **EUC-JP** でした。本フォークでは、ソースとドキュメントを
**UTF-8** に変換し、コンパイラの入力データは元のまま残しています。

| 対象 | 文字コード | 備考 |
| --- | --- | --- |
| `src/` のソース（`.c` / `.h`） | UTF-8 | 非ASCII文字はすべて日本語コメント内。プログラムの文字列は不変 |
| ドキュメント（`org-doc/*.txt`、`doc/*.md`、`src/mml2mid.1`） | UTF-8 | 内容は1文字も変えずに変換済み |
| `sample/*.mml`、`org-doc/tr-rack.mml` | **EUC-JP のまま** | 下記の理由により変換しません |
| `org-doc/mml2mid.def` | 変更なし | VZエディタ用のバイナリを含む定義ファイル |
| `mmlpp/mmlpp.pl`、`tk/tkmml2mid.tcl` | EUC-JP のまま | ドキュメントではなくプログラム |

### `.mml` を変換しない理由

`.mml` はドキュメントではなく**コンパイラへの入力データ**です。mml2mid は文字列中の
バイトをそのままSMFのメタイベント（曲名・トラック名など）へ書き出すため、`.mml` の
文字コードを変えると**生成されるMIDIファイルの中身が変わります**。同梱の `.mid` との
一致も、回帰テストのベースラインも崩れます。

`.mml` 内の日本語を扱う場合は、ソースの文字コードに応じて `-m` オプション
（文字列中をShift-JISとみなす）を使い分けてください。

### ビルド時の注意

ビルドファイルはMSVCに `/utf-8`、GCC/Clangに `-finput-charset=UTF-8` を渡します。
`cl` を手動で実行する場合は `/utf-8` を付けてください（付けないとC4819警告が出ます）。

## 著作権

本ソフトウェアの著作権は、オリジナルの作者である 門田暁人・藤井秀樹・黒田久泰・
新出尚之 の各氏に帰属します。サンプル曲（MMLデータ）の著作権は、それぞれのデータ
作成者に帰属します。詳細および免責事項は [org-doc/copyrigh.txt](org-doc/copyrigh.txt) を
参照してください。

本リポジトリは上記オリジナル（mml2mid Version 5.30b、入手元:
<https://www.vector.co.jp/soft/dl/unix/art/se102432.html>）のフォークであり、
著作権の帰属はオリジナルのまま変わりません。オリジナルの配布物の説明は
[readme.txt](readme.txt) にあります。
