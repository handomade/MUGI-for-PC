# actpazle.c SDL2移植パッチガイド

## 必要な変更一覧

### [1] インクルード先の変更 (先頭付近)

```c
// 削除:
#include "msxgl.h"
// 追加:
#include "hal.h"
#include "input_hal.h"
#include "audio_print_hal.h"
#include "all_stage_data.h"   // ← 新規: 全ステージデータ
```

### [2] actpazle_p0.c のインクルード行を削除

```c
// 削除:
// #include "actpazle_p0.c"
// (actpazle_p0.cはSDL2版では不要)
```

### [3] void main() → void game_main() にリネーム

```c
// 変更前:
void main()
// 変更後:
void game_main()
```

### [4] Game_Init() 内の Pletter/Tile_LoadBank を置き換え

```c
// 変更前:
void Load_Picdata(u8 num){
    u8 temp_buffer[10240];
    Tile_SetBankPage(num);
    Pletter_UnpackToRAM(g_picdata, temp_buffer);
    Tile_LoadBank(0, temp_buffer, sizeof(temp_buffer) / TILE_CELL_BYTES);
}

// 変更後:
void Load_Picdata(u8 num){
    (void)num;
    // SDL2版: VDP_LoadPicdata() で既にバンク1に読み込み済み
    Tile_SelectBank(1);
}
```

### [5] Game_Init() 内の効果音展開を削除/コメントアウト

```c
// 削除 (SDL2版はAudio_Init()で代替):
// Pletter_UnpackToRAM(g_mugi_sfx, 0xE000);
```

### [6] ステージロード部分の置き換え

Load_Stage() 内:
```c
// 変更前:
Pletter_UnpackToRAM(g_stage_data, g_TileMap);

// 変更後:
memcpy(g_TileMap, g_stage_map_table[current_stage], 208);
```

### [7] タイトル画面のロード (actpazle_title.c)

```c
// 変更前:
Pletter_UnpackToRAM(g_TITLE, g_TileMap);
Tile_DrawScreen(g_TileMap);

// 変更後:
memcpy(g_TileMap, g_TITLE, 208);
Tile_DrawScreen(g_TileMap);
```

### [8] WaitVBlank() の置き換え

SDL2版では WaitVBlank() がフレーム同期点になる。
actpazle.c の WaitVBlank() 定義を以下に置き換え:

```c
void WaitVBlank(void) {
    // screen[0] を画面に表示
    SDL_SetRenderTarget(g_renderer, NULL);
    SDL_RenderCopy(g_renderer, g_screen[0], NULL, NULL);
    SDL_RenderPresent(g_renderer);

    // 入力更新
    Input_Update();
    if (Input_IsQuitRequested()) {
        SDL_Quit();
        exit(0);
    }

    // VDP割り込み処理 (タイマー・BGM・SE)
    vblank_process();

    g_VBlank = 0;
    g_Frame++;
}
```

### [9] VDP_InterruptHandler() → vblank_process() にリネーム

```c
// 変更前:
void VDP_InterruptHandler()

// 変更後:
void vblank_process(void)
```

そして actpazle.c の先頭付近に前方宣言を追加:
```c
void vblank_process(void);
```

### [10] AKG_* サウンド関数をSDL2版に置き換え

```c
// AKG_Play(bgm_id, 0xD100)   → Audio_PlayBGM(bgm_id)
// AKG_PlaySFX(se_id, 0, 0)   → Audio_PlaySFX(se_id)
// AKG_Decode()                → Audio_Update()
// AKG_InitSFX(0xE000)         → (不要、Audio_Init()で初期化済み)
```

### [11] Tile_SetDrawPage の呼び出し確認

SDL2版では page 0/1 のみ使用:
- page 0 = 表示バッファ (g_screen[0])
- page 1 = 作業バッファ (g_screen[1])

MSX版の page 2 を使っている箇所は page 1 に変更。

---

## ビルド手順 (MSYS2 UCRT64)

```bash
cd mugi_sdl2
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=mingw32-make
mingw32-make
```

## assets/ フォルダ構成

```
assets/
  picdata.bmp      ← MSXプロジェクトから流用
  bgm/
    bgm0.ogg ... bgm5.ogg
  sfx/
    se0.wav ... se10.wav
```
