# mml2mid

MML（Music Macro Language）で書かれたテキストファイルを読み込み、標準MIDIファイル
（SMF）を出力するコンパイラです。

オリジナルは 門田暁人・藤井秀樹・黒田久泰・新出尚之 の各氏による Version 5.30b
（1993〜2011年）です。本リポジトリは、そのソースを現代の64bit環境
（Windows / Linux / macOS）でビルドできるようにモダナイズしたものです。
**生成されるMIDIの内容はオリジナルと変わりません。**

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
MMLの詳しい書き方は [doc/mml2mid.txt](doc/mml2mid.txt) を参照してください
（doc以下のドキュメントはShift-JISです）。

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
| `doc/` | オリジナルのドキュメント類（MMLリファレンス、テクニック集など） |
| `sample/` | サンプル曲のMMLデータと標準MIDIファイル |
| `test/` | 回帰テスト用のベースラインとスクリプト |
| `cmake/` | ctest から呼ばれるテスト実行スクリプト |
| `mmlpp/` | mml2mid用プリプロセッサ（Perlスクリプト） |
| `tk/` | Tcl/Tk製のGUIフロントエンド tkmml2mid |

## モダナイズの概要

詳細および修正した不具合の一覧は [BUILD.md](BUILD.md) にあります。主な点は次の通りです。

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

## 文字コードについて

- `src/` 以下のソースは **UTF-8** です。元はEUC-JPでしたが、非ASCII文字は
  すべて日本語コメント内にあり、プログラムの文字列は変わっていません。
  ビルドファイルはMSVCに `/utf-8`、GCC/Clangに `-finput-charset=UTF-8` を渡します。
  `cl` を手動で実行する場合は `/utf-8` を付けてください（C4819警告が出ます）。
- `doc/` 以下と `sample/*.txt` は **Shift-JIS**、`sample/*.mml` は **EUC-JP** のまま
  変更していません。

## 著作権

本ソフトウェアの著作権は、オリジナルの作者である 門田暁人・藤井秀樹・黒田久泰・
新出尚之 の各氏に帰属します。サンプル曲（MMLデータ）の著作権は、それぞれのデータ
作成者に帰属します。詳細および免責事項は [doc/copyrigh.txt](doc/copyrigh.txt) を
参照してください。

オリジナルの配布物の説明は [readme.txt](readme.txt) にあります。
