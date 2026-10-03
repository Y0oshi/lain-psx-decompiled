<p align="center">
  <img src="docs/assets/banner.svg" alt="Serial Experiments Lain PSX: decompilation and native port" width="880">
</p>

<p align="center">
  <a href="https://github.com/Y0oshi/lain-psx-decompiled/actions/workflows/port.yml"><img src="https://github.com/Y0oshi/lain-psx-decompiled/actions/workflows/port.yml/badge.svg?branch=main" alt="Build"></a>
  <a href="https://github.com/Y0oshi/lain-psx-decompiled/releases"><img src="https://img.shields.io/badge/download-releases-e0435c" alt="Download"></a>
  <img src="https://img.shields.io/badge/matching-321%2F321%20functions%20(100%25)-2ea44f" alt="Matching progress">
  <img src="https://img.shields.io/badge/EXE-byte--identical-2ea44f" alt="Byte-identical EXE">
  <img src="https://img.shields.io/badge/language-C-555555?logo=c" alt="C">
  <img src="https://img.shields.io/badge/platforms-Windows%20%7C%20macOS%20%7C%20Linux-6d6780" alt="Platforms">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT License"></a>
</p>

<p align="center"><a href="README.md">English</a> | 日本語</p>

PlayStation版『serial experiments lain』（パイオニアLDC、1998年）の**デコンパイル**と、そのコードから作った Windows・macOS・Linux 向けの**ネイティブ版**です。

デコンパイルではゲームのソースコードを C 言語で再現しています。当時の PsyQ 開発環境でコンパイルすると、市販版とバイト単位で同一の実行ファイルができあがります。ネイティブ版は同じコードを PlayStation SDK の再実装の上で動かすもので、エミュレーターは使いません。

| ディスク | 品番 | 実行ファイル | データトラックの SHA-1 (Redump) |
|---|---|---|---|
| ディスク1 | SLPS-01603 | `SLPS_016.03` | `6426fbdb45f27e089af650cddb8c41929c7890de` |
| ディスク2 | SLPS-01604 | `SLPS_016.04` | `409668af454a0e0a33a71c1d22918d765e718608` |

再ビルドした `SLPS_016.03` の SHA-1 は `a0012634f82dd7fcc4ee5a3f97ddc052573b4815` です。ディスク2の実行ファイルも同一です。

> [!IMPORTANT]
> このリポジトリと配布物には**ゲームのデータは一切含まれていません**。ムービー、音声、画像、効果音、テキスト、データテーブルは、お手持ちの日本版ディスク2枚から吸い出したイメージから読み込みます。

## 遊び方

1. ディスク2枚を Redump 形式（`.cue` + `.bin`）で吸い出します。
2. [Releases](https://github.com/Y0oshi/lain-psx-decompiled/releases) からお使いの OS 用のファイルをダウンロードします。

   | OS | ファイル | 初回起動 |
   |---|---|---|
   | Windows 10/11 (x64) | `Lain-windows.zip` | `lain.exe` を実行。SmartScreen の警告が出たら「詳細情報」から「実行」。 |
   | macOS（Apple シリコン） | `Lain-macos.zip` | `Lain.app` を右クリックして「開く」。 |
   | Linux（x86-64、glibc 2.35 以降） | `Lain-linux-x86_64.tar.gz` | `./lain` を実行。OpenGL 3.2 が必要です。 |

   ビルドにはコード署名をしていません。
3. ランチャーで2枚のディスクを追加します（ファイル選択またはドラッグ&ドロップ）。ランチャーがディスクを確認し、データフォルダーにコピーします。
4. ウィンドウサイズ、字幕などを選んで **Play** を押します。

ゲーム中に **F1** を押すとメニューが開きます。画面、音量、言語、キー設定、チート、MOD を変更できます。

| PlayStation | キーボード | ゲーム内 |
|---|---|---|
| 方向キー | 矢印キー | カーソル移動、ノードの選択 |
| ○ | V | 決定、ノードを開く |
| × | C | 戻る |
| △ | Z | メニュー（ロード、セーブなど） |
| スタート | Return | 進む、スキップ |

ゲームパッドはコントローラー1として使えます。操作と設定の詳細は[port/PORTING.md](port/PORTING.md#playing)（英語）を参照してください。

### 主な機能

| 機能 | 内容 |
|---|---|
| オリジナルどおりの動作 | ゲームの処理、タイミング、テンポは PlayStation 実機での計測どおりです。 |
| 画面 | 解像度の拡大、フルスクリーン、元のゲーム速度のままの 60fps 補間（任意）。 |
| 字幕・吹き替え | `.srt`、`.ass`、`.ogg` のファイルをまとめたフォルダー形式のパック。laingame.net の英語ファン字幕は、ランチャーから配布元よりダウンロードできます（同梱はしていません）。 |
| セーブ | 標準的な 128 KiB のメモリーカードイメージ（`.mcd`）。エミュレーターと共用できます。 |
| 未使用データ | ディスクに入っているのにゲームで使われていない素材を、お手持ちのイメージから読み出して表示するランチャーのタブ。[docs/findings](docs/findings/README.md)（英語）を参照。 |
| MOD | 画像、テキスト、音声、ムービー、ゲームデータの差し替え、HD テクスチャーパック、あらゆるゲーム関数をフックできる Lua スクリプト、ネイティブプラグイン。ランチャーの Mods タブで管理します。[MODDING.md](port/MODDING.md)（英語）を参照。 |

HD テクスチャーは MOD として作って読み込む仕組みです。高解像度化した画像そのものは同梱していません。

## 進捗

| 部分 | 状況 |
|---|---|
| デコンパイル（`src/`） | ゲームの全321関数が、元と同じバイト列にコンパイルされる C になっています。実行ファイルはバイト単位で一致します。ソニーの PsyQ ライブラリは元のコードのままリンクします。 |
| ネイティブ版（`port/`） | ゲームを最後まで遊べます。両サイト、全音声セッションとムービー、ディスク交換、4つのエンディングすべてに対応しています。 |
| 命名 | 関数、グローバル変数、構造体のフィールドの大半に名前が付いています。 |

残りの作業は [docs/ROADMAP.md](docs/ROADMAP.md)（英語）にまとめています。

## ネイティブ版のビルド

必要なもの: CMake、C/C++ コンパイラー、SDL2、OpenAL (openal-soft)。

```sh
# macOS:  brew install cmake ninja sdl2 openal-soft
# Linux:  apt install cmake ninja-build libsdl2-dev libopenal-dev libgl-dev
cmake -S port -B build/port -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/port
build/port/lain
```

Windows 版は macOS または Linux から、Docker 上の MinGW-w64 でクロスコンパイルします。配布用パッケージはすべての依存ライブラリを静的リンクします。

```sh
port/tools/package_macos.sh     # dist/Lain-macos.zip
port/tools/package_windows.sh   # dist/Lain-windows.zip (Docker 上の MinGW-w64)
port/tools/package_linux.sh     # dist/Lain-linux-x86_64.tar.gz (Docker 上の Ubuntu 22.04)
```

## デコンパイルのビルド

必要なもの: Python 3、Docker（GCC 2.8.1-psx と maspsx を含みます）、ディスク2枚のイメージ。

```sh
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
tools/disc.py extract path/to/disc1.cue --disc 1
tools/disc.py extract path/to/disc2.cue --disc 2
tools/docker.sh make split   # 実行ファイルを asm/ に逆アセンブル
tools/docker.sh make         # コンパイル、リンク、照合
```

ビルドに成功すると最後に `build/SLPS_016.03: OK` と表示されます。関数の作業については[docs/MATCHING.md](docs/MATCHING.md)（英語）を参照してください。

## ドキュメント（英語）

| ドキュメント | 内容 |
|---|---|
| [DECOMPILATION.md](docs/DECOMPILATION.md) | デコンパイルの構成 |
| [MATCHING.md](docs/MATCHING.md) | マッチングの手順とツール |
| [PORTING.md](port/PORTING.md) | ネイティブ版の内部構造、パック形式、設定 |
| [MODDING.md](port/MODDING.md) | MOD の作り方と使い方 |
| [findings](docs/findings/README.md) | 未使用・隠し要素 |
| [ROADMAP.md](docs/ROADMAP.md) | 残りの作業 |

## ライセンス

このリポジトリのコードは [MIT ライセンス](LICENSE)で公開しています（copyright Y0oshi）。サードパーティー製のコンポーネントはそれぞれのライセンスに従います。[thirdparty/](thirdparty/README.md) を参照してください。

『serial experiments lain』の権利は各権利者に帰属します。本プロジェクトは権利者とは関係がなく、承認も受けていません。アイコンはゲームのメモリーカードアイコンをもとにしています。
