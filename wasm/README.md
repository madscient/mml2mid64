# WebAssembly ビルド

統合環境（[mml2mid64ide](../../mml2mid64ide)）に同梱するための WASM ビルド。
インストール不要で全 OS 同一の挙動になり、VS Code 拡張機能ホスト（Node）から
`NODEFS` で実ワークスペースをマウントできるので `#include` もそのまま解決できる。

> **⚠ 未検証です。** このディレクトリと `CMakeLists.txt` の `if(EMSCRIPTEN)` 分岐は
> emsdk / Node が入っていない環境で書かれており、**一度もビルド・実行していません。**
> 最初に使うときは `smoke.mjs` を通すところまで確認し、必要ならフラグを調整して
> ください。ソース側の変更は不要のはずです（理由は `CMakeLists.txt` のコメント）。

## ビルド

```sh
# emsdk を入れて有効化しておく
emcmake cmake -S . -B build-wasm
cmake --build build-wasm
```

出力は `build-wasm/mml2mid.mjs` と `build-wasm/mml2mid.wasm`。
`mml2mid.mjs` は ES モジュールで、ファクトリ `createMml2mid()` を default export する。

## 動作確認

```sh
node wasm/smoke.mjs build-wasm/mml2mid.mjs
```

確認する内容:

1. 普通にコンパイルできて SMF とデバッグマップの両方が出る
2. `-g2` を付けても SMF が1バイトも変わらない
3. `--diag=json` が JSON の診断を出す
4. `NODEFS` で実ディレクトリをマウントできる

## 使い方（拡張機能側）

**1コンパイルにつき1インスタンスを作って捨てる。** 本体はグローバル状態が多く
（`tstep` / `cur_line` / `trknum` / マクロテーブル）、エラー時に `exit()` するため、
生きたインスタンスで `main()` に再突入するのは安全ではない。インスタンス生成の
コストは ms オーダーなので、再入可能化のリファクタより遥かに安全で速い。

```js
import createMml2mid from "./mml2mid.mjs";

const mod = await createMml2mid({
  noInitialRun: true,
  print:    (s) => {/* 通常メッセージ */},
  printErr: (s) => {/* エラー・警告・--diag=json の1行JSON */},
});

// ワークスペースを実ファイルシステムのままマウントする
mod.FS.mkdir("/work");
mod.FS.mount(mod.NODEFS, { root: "/path/to/workspace" }, "/work");

try {
  mod.callMain(["-g", "--diag=json", "/work/song.mml", "/work/song.mid"]);
} catch (e) {
  if (typeof e?.status !== "number") throw e;  // ExitStatus 以外は本物の例外
}

const map = JSON.parse(new TextDecoder().decode(
  mod.FS.readFile("/work/song.mmlmap.json")));
```

`EXIT_RUNTIME=1` を付けてあるので、`main()` が返るかエラーで `exit()` した時点で
ランタイムは片付けられ、出力はファイルシステムに反映される。以後そのインスタンスは
使えない。

デバッグマップの形式は [../doc/DEBUGMAP.md](../doc/DEBUGMAP.md) を参照。
