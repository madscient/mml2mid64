# WebAssembly ビルド

統合環境（[mml2mid64ide](../../mml2mid64ide)）に同梱するための WASM ビルド。
インストール不要で全 OS 同一の挙動になり、VS Code 拡張機能ホスト（Node）から
`NODEFS` で実ワークスペースをマウントできるので `#include` もそのまま解決できる。

検証済み: Emscripten 6.0.6 / Node 24.19.0 / CMake 4.3.3 + Ninja（Windows）。
警告ゼロでビルドでき、**生成される SMF もデバッグマップもネイティブ版と
バイト単位で一致する**（`test/` 配下の全24 MML で確認）。ソースの変更は不要だった。

## ビルド

```sh
# emsdk を入れて有効化しておく（source /path/to/emsdk/emsdk_env.sh）
emcmake cmake -S . -B build-wasm -G Ninja
cmake --build build-wasm
```

出力は `build-wasm/mml2mid.mjs` と `build-wasm/mml2mid.wasm`。
`mml2mid.mjs` は ES モジュールで、ファクトリ `createMml2mid()` を default export する。

ジェネレータの指定（`-G Ninja`）は Windows では必須。既定の Visual Studio
ジェネレータは Emscripten では使えない。

### うまくいかないとき

- **`No CMAKE_C_COMPILER could be found`** … 環境変数 `CMAKE_TOOLCHAIN_FILE` が
  既に設定されていると、`emcmake` は Emscripten のツールチェーンファイルを
  渡さない（`emcmake.py` がそう書かれている）。vcpkg を入れていると
  `c:/vcpkg/scripts/buildsystems/vcpkg.cmake` が入っていることが多い。
  `unset CMAKE_TOOLCHAIN_FILE` してから叩くか、明示的に渡す:

  ```sh
  cmake -S . -B build-wasm -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/c/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
  ```

- **Git Bash で `emsdk_env.sh` が効かない** … PATH を直接足せばよい:

  ```sh
  export PATH="/c/emsdk:/c/emsdk/upstream/emscripten:/c/emsdk/node/<版>:/c/emsdk/python/<版>:$PATH"
  export EM_CONFIG=/c/emsdk/.emscripten
  ```

- **ninja が無い** … Visual Studio に同梱されている
  `.../Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja` を PATH に足す。

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
