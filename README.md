# MUGI SDL2移植版 ビルドガイド

## 必要なもの

- CMake 3.16以上
- GCC / Clang (Linux) または MSVC / MinGW (Windows)
- SDL2
- SDL2_mixer

### Linux (Ubuntu/Debian)
```bash
sudo apt install cmake libsdl2-dev libsdl2-mixer-dev
```

### Windows (vcpkg)
```bash
vcpkg install sdl2 sdl2-mixer
```

---

## ディレクトリ構成

```
mugi_sdl2/
├── CMakeLists.txt
├── PATCH_GUIDE.c          ← actpazle.cへの修正指示
├── include/
│   ├── msxgl.h            ← MSXgl互換統合ヘッダー（SDL2版）
│   ├── msxgl_types.h      ← 型定義 (u8, u16, i8...)
│   ├── hal.h              ← VDP/Tile/SW_Sprite/Pletter/Math API
│   ├── input_hal.h        ← Joystick/Keyboard API
│   └── audio_print_hal.h  ← Audio/S_Print API
├── src/
│   ├── main.c             ← SDL2メインループ
│   ├── game/
│   │   ├── actpazle.c     ← [要修正] MSX2版から移植
│   │   ├── actpazle_title.c    ← そのまま
│   │   ├── actpazle_password.c ← そのまま
│   │   └── (stage1.h等は assets/ またはここに配置)
│   └── hal/
│       ├── vdp_sdl2.c     ← VDP/Tile/SW_Sprite実装
│       ├── input_sdl2.c   ← 入力実装
│       ├── audio_sdl2.c   ← サウンド実装
│       └── print_sdl2.c   ← テキスト描画実装
└── assets/
    ├── bgm/
    │   ├── title.ogg
    │   ├── main.ogg
    │   ├── win.ogg
    │   ├── mystery.ogg
    │   └── ending.ogg
    ├── sfx/
    │   ├── cursor.wav
    │   ├── button.wav
    │   └── ... (11ファイル)
    └── font.bmp            ← 8x8ビットマップフォント
```

---

## actpazle.c への修正手順

PATCH_GUIDE.c の[1]〜[7]を参照。主な変更は5箇所：

1. `#include` をSDL2版 `msxgl.h` に統一
2. `void main()` → `void game_main(void)` にリネーム
3. `WaitVBlank()` をSDL2版に書き換え（SDL2描画+Input_Update呼び出し）
4. `VDP_InterruptHandler()` → `vblank_process(void)` にリネーム
5. メインループの終了条件を `Input_IsQuitRequested()` に変更

---

## Pletter圧縮データについて

ステージデータ (`stage1.h` 等) がPletter圧縮されている場合、
`vdp_sdl2.c` の `Pletter_UnpackToRAM()` に正式な展開ルーチンが必要。

MSXglの公式実装: `engine/src/compress/pletter.c`  
→ Z80アセンブリ版をCに移植したものを使用すること。

未圧縮データの場合はスタブのままで動作する。

---

## フォントについて

`assets/font.bmp` として 8x8ピクセルのビットマップフォントを用意する。

- 横16文字 × 縦6行 = 96文字（ASCII 0x20-0x7F）
- 背景色(黒 0,0,0)は透明扱い

MSXglのフォントデータ (`font_*.h`) から変換ツールを自作するか、
フリーの8x8ドットフォントを使用する。

---

## ビルド方法

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# 実行（デフォルト2倍サイズ）
./mugi

# 3倍サイズで実行
./mugi 3
```

---

## 既知の制限事項

- Pletter展開ルーチンは未実装（スタブ）→ 要実装
- タイルパレット変換の精度は近似値
- `VDP_CommandHMMM` の座標系はMSXの256px/pageモデルを踏襲
- `SW_Sprite_COPY()` はPage2→Page0の全面コピーを行う
