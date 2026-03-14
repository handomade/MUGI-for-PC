#pragma once
// hal.h - MSXgl互換 HAL定義 (SDL2版)

#include "msxgl_types.h"
#include <SDL2/SDL.h>

// ============================================================
// ゲーム解像度
// ============================================================
#define GAME_W  256
#define GAME_H  212

// ============================================================
// カラー定数 (MSX16色)
// ============================================================
#define COLOR_BLACK         0
#define COLOR_MEDIUM_GREEN  1
#define COLOR_LIGHT_GREEN   2
#define COLOR_DARK_BLUE     3
#define COLOR_LIGHT_BLUE    4
#define COLOR_DARK_RED      5
#define COLOR_CYAN          6
#define COLOR_MEDIUM_RED    7
#define COLOR_LIGHT_RED     8
#define COLOR_DARK_YELLOW   9
#define COLOR_LIGHT_YELLOW  10
#define COLOR_DARK_GREEN    11
#define COLOR_MAGENTA       12
#define COLOR_GRAY          13
#define COLOR_WHITE         14

// ============================================================
// VDP定数
// ============================================================
#define VDP_MODE_SCREEN5    5
#define VDP_FREQ_50HZ       0
#define VDP_FREQ_60HZ       1
#define TILE_CELL_BYTES     128  // 16x16 x 4bpp (MSX互換, Tile_LoadBankで使用)

// ============================================================
// SW_Sprite定数
// ============================================================
#define SW_SPRITE_MAX  32

// ============================================================
// グローバル (vdp_sdl2.c で定義, main.c からも使う)
// ============================================================
extern SDL_Renderer* g_renderer;
extern SDL_Window*   g_window_handle;
extern SDL_Texture*  g_screen[2];  // [0]=表示バッファ, [1]=作業バッファ

// ============================================================
// VDP (vdp_sdl2.c)
// ============================================================
void VDP_SetMode(u8 mode);
void VDP_SetColor(u8 color);
void VDP_EnableDisplay(u8 enable);
void VDP_EnableVBlank(u8 enable);
void VDP_DisableSprite(void);
void VDP_ClearVRAM(void);
u8   VDP_GetFrequency(void);
void VDP_CommandHMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 w, u16 h);
void VDP_CommandPSET(u16 x, u16 y, u8 color, u8 op);

// 初期化/終了 (main.c から呼ぶ)
int  VDP_LoadPicdata(const char* bmp_path);
int  VDP_LoadMysteryBg(const char* bmp_path);
int  VDP_LoadRankingIllust(const char* bmp_path);
void Draw_MysteryBg(void);
void Copy_MysteryBgTile(u8 tx, u8 ty);
void Draw_RankingIllust(void);  // assets/picdata.bmp をバンク1に読み込む
int  VDP_CreateScreenTextures(void);
void VDP_Cleanup(void);
void Draw_TitleLogo(u8 page, int dst_x, int dst_y);  // タイトルロゴ描画
void Draw_CursorSprite(u8 page, int dst_x, int dst_y); // カーソル△描画

// Setting (actpazle_setting.c)
extern u8 g_setting_window_scale;
extern u8 g_setting_fullscreen;
extern u8 g_setting_filter;
extern u8 g_setting_bgm_type;
extern u8 g_setting_bgm_vol;
extern u8 g_setting_sfx_vol;
void Setting_Load(void);
void Setting_SaveOnExit(void);
void Setting_ApplyWindowScale(u8 scale);
void Setting_ApplyFullscreen(u8 on);
void Setting_ApplyBGMVolume(u8 vol);
void Setting_ApplySFXVolume(u8 vol);
void Game_Setting(void);

// ============================================================
// Tile (vdp_sdl2.c)
// ============================================================
void  Tile_SetDrawPage(u8 page);
void  Tile_SelectBank(u8 bank);
void  Tile_SetBankPage(u8 page);
void  Tile_FillScreen(u8 color);
void  Tile_FillTile(u8 tx, u8 ty, u8 color);
void  Tile_DrawTile(u8 tx, u8 ty, u8 tile_id);
void  Tile_DrawScreen(const u8* tilemap);
void  Tile_LoadBank(u8 bank, const u8* data, u16 tile_count);  // SDL2版ではno-op
void* Tile_GetBankAddress(u8 bank);

// ============================================================
// S_Print (vdp_sdl2.c)
// ============================================================
void S_Print_Init(u8 bank, u8 unused, u8 font_y);
void S_Print_Text(u8 page, u8 tx, u8 ty, const char* str);
void S_Print_Int(u8 page, u8 tx, u8 ty, u16 val);
void S_Print_Int_Padded(u8 page, u8 tx, u8 ty, u16 val, u8 width, char pad);
void S_Print_Int_Padded32(u8 page, u8 tx, u8 ty, u32 val, u8 width, char pad);
void S_Print_Char(u8 page, u8 tx, u8 ty, char c);

// ============================================================
// SW_Sprite (vdp_sdl2.c)
// ============================================================
void SW_Sprite_Init(void);
void SW_SpritePT_Init(void);
void SW_SetSpritePT(u8 pid, u8 bank, u16 sx, u16 sy, u8 w, u8 h);
void SW_Sprite_Set(u8 id, u8 x, u8 y, u8 pid);
void SW_Sprite_SetVisible(u8 id, u8 v);
void SW_Sprite_SetPattern(u8 id, u8 pid);
u8   SW_Sprite_IsVisible(u8 id);
void SW_Sprite_SaveCache(u8 id);
void SW_Sprite_RestoreCache(i8 id);
void SW_Sprite_Clear(u8 id);
void SW_Sprite_DrawAll(void);
void SW_Sprite_COPY(void);

// ============================================================
// Pletter (SDL2版では実質不使用)
// ============================================================
void Pletter_UnpackToRAM(const u8* src, u8* dst);

// ============================================================
// Math (vdp_sdl2.c)
// ============================================================
void Math_SetRandomSeed8(u8 seed);
void Math_SetRandomSeed16(u16 seed);
u8   Math_GetRandomMax8(u8 max);
