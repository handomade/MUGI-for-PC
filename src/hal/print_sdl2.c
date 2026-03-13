// print_sdl2.c
// S_Print互換: ビットマップフォントによるテキスト描画
// フォントはassets/font.bmpを使用（8x8ピクセル, ASCII 0x20-0x7F, 横16文字配置）

#include "audio_print_hal.h"
#include "hal.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#define FONT_CHAR_W  8
#define FONT_CHAR_H  8
#define FONT_COLS    16  // フォントシート横の文字数

static SDL_Texture* s_font_tex = NULL;

void S_Print_LoadFont(const char* path) {
    SDL_Surface* surf = SDL_LoadBMP(path);
    if (!surf) {
        fprintf(stderr, "Font load failed: %s\n", SDL_GetError());
        return;
    }
    // 色0（黒）を透明にする
    SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format, 0, 0, 0));
    s_font_tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    SDL_FreeSurface(surf);
    if (s_font_tex) {
        SDL_SetTextureBlendMode(s_font_tex, SDL_BLENDMODE_BLEND);
    }
}

void S_Print_Init(u8 bank, u8 r1, u8 r2) {
    (void)bank; (void)r1; (void)r2;
    // フォントロード
    S_Print_LoadFont("assets/font.bmp");
}

// 1文字をVRAMページに描画
// tile_x, tile_y は8px単位のグリッド座標
static void draw_char(u8 page, u8 tx, u8 ty, char c) {
    if (!s_font_tex) return;
    if (!g_page[page]) return;

    u8 idx = (u8)c - 0x20;
    if (idx >= 96) idx = 0;

    int sx = (idx % FONT_COLS) * FONT_CHAR_W;
    int sy = (idx / FONT_COLS) * FONT_CHAR_H;

    SDL_Rect src = { sx, sy, FONT_CHAR_W, FONT_CHAR_H };
    SDL_Rect dst = { tx * FONT_CHAR_W, ty * FONT_CHAR_H, FONT_CHAR_W, FONT_CHAR_H };

    SDL_SetRenderTarget(g_renderer, g_page[page]);
    SDL_RenderCopy(g_renderer, s_font_tex, &src, &dst);
    SDL_SetRenderTarget(g_renderer, NULL);
}

void S_Print_Text(u8 page, u8 tx, u8 ty, const char* str) {
    while (*str) {
        draw_char(page, tx++, ty, *str++);
    }
}

void S_Print_Char(u8 page, u8 tx, u8 ty, char c) {
    draw_char(page, tx, ty, c);
}

static void print_uint(u8 page, u8 tx, u8 ty, u32 val, u8 digits, char pad) {
    char buf[12];
    char out[12];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else { while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; } }
    // 逆順+パディング
    int len = i;
    int total = (digits > len) ? digits : len;
    for (int j = 0; j < total; j++) {
        int src = total - 1 - j;
        if (src >= len) out[j] = pad;
        else out[j] = buf[len - 1 - (src - (total - len))];
    }
    // 手直し: 普通に書く
    // 桁数分バッファ作成
    char tbuf[16] = {0};
    snprintf(tbuf, sizeof(tbuf), "%u", (unsigned)val);  // val=0のとき対応
    // valがprint前に0になっているので再計算
    // → シンプルに実装し直す
    (void)buf; (void)out; (void)len; (void)total;
    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%%c%dd", pad, digits);  // 例: "%06d"
    char result[16];
    // 元のvalが必要なので引数を使う（上の計算は捨てる）
    (void)fmt; (void)result;
    // 最終的なシンプル実装
    char tmp[12];
    snprintf(tmp, sizeof(tmp), "%u", (unsigned int)(
        // この関数内でvalが0になっているため、上位関数で値を渡す
        0  // ← 後述の実装に移譲
    ));
    (void)tmp;
}

void S_Print_Int(u8 page, u8 tx, u8 ty, u32 val) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val);
    S_Print_Text(page, tx, ty, buf);
}

void S_Print_Int_Padded(u8 page, u8 tx, u8 ty, u32 val, u8 digits, char pad) {
    char fmt[16], buf[16];
    snprintf(fmt, sizeof(fmt), "%%%c%du", pad, (int)digits);
    snprintf(buf, sizeof(buf), fmt, (unsigned)val);
    S_Print_Text(page, tx, ty, buf);
}

void S_Print_Int_Padded32(u8 page, u8 tx, u8 ty, u32 val, u8 digits, char pad) {
    S_Print_Int_Padded(page, tx, ty, val, digits, pad);
}
