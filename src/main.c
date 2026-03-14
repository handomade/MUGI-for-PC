// main.c - MUGI SDL2版 メインループ

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "hal.h"
#include "input_hal.h"
#include "audio_print_hal.h"

extern void game_main(void);
void Input_Init(void);

#define GAME_TITLE "MUGI"

SDL_Window* g_window_handle = NULL;  // actpazle_setting.cから参照
static int s_scale = 2;

// GAME_H(212)*scaleが縦解像度、横は4:3で計算
// x1=282x212, x2=565x424, x3=848x636, x4=1130x848, x5=1413x1060
#define WIN_H_BASE  (GAME_H)           // 212
#define WIN_W_BASE  (GAME_H * 4 / 3)  // 282

// SETTING画面から呼ばれるウィンドウサイズ変更
void Setting_ApplyWindowScale(u8 scale) {
    if (scale < 1 || scale > 5) return;
    s_scale = scale;
    if (SDL_GetWindowFlags(g_window_handle) & SDL_WINDOW_FULLSCREEN_DESKTOP) return;
    SDL_SetWindowSize(g_window_handle, WIN_W_BASE * scale, WIN_H_BASE * scale);
    SDL_SetWindowPosition(g_window_handle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void Setting_ApplyFullscreen(u8 on) {
    SDL_SetWindowFullscreen(g_window_handle, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static int sdl2_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 0;
    }
    // 縦=212*scale、横=4:3で決定（スキャンラインがMSXピクセルに正確に対応）
    g_window_handle = SDL_CreateWindow(GAME_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W_BASE * s_scale, WIN_H_BASE * s_scale,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_window_handle) { fprintf(stderr, "CreateWindow: %s\n", SDL_GetError()); return 0; }

    // VSyncはオフ: フレームレートはWaitVBlank内のSDL_GetTicks()で固定60fps制御
    g_renderer = SDL_CreateRenderer(g_window_handle, -1,
        SDL_RENDERER_ACCELERATED);
    if (!g_renderer) { fprintf(stderr, "CreateRenderer: %s\n", SDL_GetError()); return 0; }

    SDL_RenderSetLogicalSize(g_renderer, GAME_W, GAME_H);

    // picdata.bmp をバンク1として読み込む
    if (VDP_LoadPicdata("assets/picdata.bmp") < 0) return 0;
    // mystery_bg をバンク5として読み込む（失敗しても続行）
    VDP_LoadMysteryBg("assets/mystery_bg.bmp");
    // ranking_illust をバンク6として読み込む（失敗しても続行）
    VDP_LoadRankingIllust("assets/ranking_illust.bmp");

    VDP_ClearVRAM();
    Audio_Init();
    return 1;
}

static void sdl2_quit(void) {
    Audio_Quit();
    VDP_Cleanup();
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window_handle)   SDL_DestroyWindow(g_window_handle);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        int sc = atoi(argv[1]);
        if (sc >= 1 && sc <= 4) s_scale = sc;
    }
    if (!sdl2_init()) { sdl2_quit(); return 1; }
    Input_Init();
    game_main();
    sdl2_quit();
    return 0;
}
