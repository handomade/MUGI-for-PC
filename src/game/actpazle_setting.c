// actpazle_setting.c
// SETTING画面

#include "hal.h"
#include "input_hal.h"
#include "audio_print_hal.h"
#include <stdio.h>
#include <string.h>

// ────────────────────────────────────────
// 設定値（グローバル）
// ────────────────────────────────────────
u8 g_setting_window_scale = 2;   // 1〜3
u8 g_setting_fullscreen   = 0;   // 0=OFF, 1=ON
u8 g_setting_filter       = 0;   // 0=NONE, 1=CRT, 2=BLUR, 3=CRT+BLUR
u8 g_setting_bgm_type     = 0;   // 0=ORIGINAL, 1=ARRANGE
u8 g_setting_bgm_vol      = 80;  // 0〜100
u8 g_setting_sfx_vol      = 80;  // 0〜100

extern void Setting_ApplyWindowScale(u8 scale);
extern void Setting_ApplyFullscreen(u8 on);
extern void Setting_ApplyBGMVolume(u8 vol);
extern void Setting_ApplySFXVolume(u8 vol);

// ────────────────────────────────────────
// INIファイル保存・読み込み
// ────────────────────────────────────────
static void Setting_Save(void) {
    FILE* f = fopen("mugi_config.ini", "w");
    if (!f) return;
    fprintf(f,
        "[Window]\nwidth_scale=%d\nfullscreen=%d\n\n"
        "[Sound]\nbgm_type=%s\nbgm_volume=%d\nsfx_volume=%d\n\n"
        "[Display]\nfilter=%s\n",
        g_setting_window_scale,
        g_setting_fullscreen,
        g_setting_bgm_type ? "arrange" : "original",
        g_setting_bgm_vol,
        g_setting_sfx_vol,
        g_setting_filter == 1 ? "crt" : g_setting_filter == 2 ? "blur" : g_setting_filter == 3 ? "crt+blur" : "none"
    );
    fclose(f);
}

// 終了時保存（ウィンドウ位置も含む）
void Setting_SaveOnExit(void) {
    // フルスクリーン時はウィンドウ位置を保存しない
    if (!g_setting_fullscreen) {
        extern SDL_Window* g_window_handle;
        int x, y;
        SDL_GetWindowPosition(g_window_handle, &x, &y);
        FILE* f = fopen("mugi_config.ini", "w");
        if (!f) return;
        fprintf(f,
            "[Window]\nwidth_scale=%d\nfullscreen=%d\npos_x=%d\npos_y=%d\n\n"
            "[Sound]\nbgm_type=%s\nbgm_volume=%d\nsfx_volume=%d\n\n"
            "[Display]\nfilter=%s\n",
            g_setting_window_scale,
            g_setting_fullscreen,
            x, y,
            g_setting_bgm_type ? "arrange" : "original",
            g_setting_bgm_vol,
            g_setting_sfx_vol,
            g_setting_filter == 1 ? "crt" : g_setting_filter == 2 ? "blur" : g_setting_filter == 3 ? "crt+blur" : "none"
        );
        fclose(f);
    } else {
        Setting_Save();
    }
}

void Setting_Load(void) {
    FILE* f = fopen("mugi_config.ini", "r");
    if (!f) return;
    char line[64];
    int pos_x = -1, pos_y = -1;
    while (fgets(line, sizeof(line), f)) {
        int v;
        char s[32];
        if (sscanf(line, "width_scale=%d",  &v) == 1) g_setting_window_scale = (u8)v;
        if (sscanf(line, "fullscreen=%d",   &v) == 1) g_setting_fullscreen   = (u8)(v ? 1 : 0);
        if (sscanf(line, "bgm_volume=%d",   &v) == 1) g_setting_bgm_vol      = (u8)v;
        if (sscanf(line, "sfx_volume=%d",   &v) == 1) g_setting_sfx_vol      = (u8)v;
        if (sscanf(line, "bgm_type=%31s",    s) == 1) g_setting_bgm_type     = (strcmp(s,"arrange")==0) ? 1 : 0;
        if (sscanf(line, "filter=%31s",      s) == 1) g_setting_filter       = (strcmp(s,"crt+blur")==0) ? 3 : (strcmp(s,"crt")==0) ? 1 : (strcmp(s,"blur")==0) ? 2 : 0;
        if (sscanf(line, "pos_x=%d",        &v) == 1) pos_x = v;
        if (sscanf(line, "pos_y=%d",        &v) == 1) pos_y = v;
    }
    fclose(f);
    // 範囲クランプ
    if (g_setting_window_scale < 1 || g_setting_window_scale > 5) g_setting_window_scale = 2;
    if (g_setting_bgm_vol  > 100) g_setting_bgm_vol  = 80;
    if (g_setting_sfx_vol  > 100) g_setting_sfx_vol  = 80;
    // ウィンドウ位置を復元
    if (!g_setting_fullscreen && pos_x >= 0 && pos_y >= 0) {
        SDL_SetWindowPosition(g_window_handle, pos_x, pos_y);
    }
}

// ────────────────────────────────────────
// 描画ヘルパー
// ────────────────────────────────────────
// ボリューム表示: 0〜100を10段階バーに変換
static void draw_vol_bar(u8 tx, u8 ty, u8 val) {
    u8 bars = val / 10;  // 0〜10
    for (u8 i = 0; i < 10; i++)
        S_Print_Char(0, tx + i, ty, (i < bars) ? '-' : ' ');
}

static void draw_setting_row(u8 row, u8 cursor,
    u8 ws, u8 fs, u8 flt, u8 bt, u8 bv, u8 sv)
{
    u8 y = 6 + row;
    // カーソルクリア＋描画
    S_Print_Char(0, 2, y, ' ');
    if (row == cursor) Draw_CursorSprite(0, 2 * 8, y * 8);

    const char* win_labels[] = {"", "X1 ", "X2 ", "X3 ", "X4 ", "X5 "};
    const char* flt_labels[] = {"NONE    ", "CRT     ", "BLUR    ", "CRT+BLUR"};
    const char* bgt_labels[] = {"ORIGINAL", "ARRANGE "};

    switch (row) {
        case 0:
            S_Print_Text(0, 4, y, "WINDOW    ");
            S_Print_Text(0, 15, y, win_labels[ws]);
            break;
        case 1:
            S_Print_Text(0, 4, y, "FULLSCREEN");
            S_Print_Text(0, 15, y, fs ? "ON " : "OFF");
            break;
        case 2:
            S_Print_Text(0, 4, y, "FILTER    ");
            S_Print_Text(0, 15, y, flt_labels[flt]);
            break;
        case 3:
            S_Print_Text(0, 4, y, "BGM TYPE  ");
            S_Print_Text(0, 15, y, bgt_labels[bt]);
            break;
        case 4:
            S_Print_Text(0, 4, y, "BGM VOL   ");
            draw_vol_bar(15, y, bv);
            break;
        case 5:
            S_Print_Text(0, 4, y, "SFX VOL   ");
            draw_vol_bar(15, y, sv);
            break;
        case 6:
            S_Print_Text(0, 4, y, "CANCEL    ");
            break;
        case 7:
            S_Print_Text(0, 4, y, "OK        ");
            break;
    }
}

#define SET_ITEMS 8

void Game_Setting(void) {
    // 一時値（OKで確定、CANCELで破棄）
    u8 tmp_ws  = g_setting_window_scale;
    u8 tmp_fs  = g_setting_fullscreen;
    u8 tmp_flt = g_setting_filter;
    u8 tmp_bt  = g_setting_bgm_type;
    u8 tmp_bv  = g_setting_bgm_vol;
    u8 tmp_sv  = g_setting_sfx_vol;

    Tile_SetDrawPage(0);
    Tile_SelectBank(0);
    Tile_FillScreen(COLOR_BLACK);

    S_Print_Text(0, 9, 2, "- SETTING -");
    S_Print_Text(0, 3, 21, "UP,DOWN:SELECT");
    S_Print_Text(0, 3, 22, "LEFT,RIGHT:CHANGE");

    u8 cursor  = 0;
    u8 cooldown = 0;

    // 全行を初回描画
    for (u8 i = 0; i < SET_ITEMS; i++)
        draw_setting_row(i, cursor, tmp_ws, tmp_fs, tmp_flt, tmp_bt, tmp_bv, tmp_sv);

    while (1) {
        WaitVBlank();
        joy_state = Joystick_Read(JOY_PORT_1);
        g_Frame = 0;

        if (cooldown > 0) { cooldown--; continue; }

        u8 prev   = cursor;
        u8 dirty  = 0;

        // 上下でカーソル移動
        if (IsUpPressed()) {
            cursor = (cursor > 0) ? cursor - 1 : SET_ITEMS - 1;
            cooldown = 8; next_se = SE_CURSOR; dirty = 1;
        } else if (IsDownPressed()) {
            cursor = (cursor < SET_ITEMS - 1) ? cursor + 1 : 0;
            cooldown = 8; next_se = SE_CURSOR; dirty = 1;
        }

        // 左右で値変更（項目0〜5のみ）
        u8 changed = 0;
        if (IsLeftPressed()) {
            switch (cursor) {
                case 0: if (tmp_ws  > 1)   { tmp_ws--;        changed = 1; } break;
                case 1: tmp_fs  = 0;                            changed = 1; break;
                case 2: if (tmp_flt > 0)   { tmp_flt--;       changed = 1; } break;
                case 3: tmp_bt  = 0;                            changed = 1; break;
                case 4: if (tmp_bv  >= 10) { tmp_bv  -= 10;   changed = 1; } break;
                case 5: if (tmp_sv  >= 10) { tmp_sv  -= 10;   changed = 1; } break;
            }
            if (changed) { cooldown = 8; next_se = SE_CURSOR; dirty = 1; }
        } else if (IsRightPressed()) {
            switch (cursor) {
                case 0: if (tmp_ws  < 5)   { tmp_ws++;        changed = 1; } break;
                case 1: tmp_fs  = 1;                            changed = 1; break;
                case 2: if (tmp_flt < 3)   { tmp_flt++;       changed = 1; } break;
                case 3: tmp_bt  = 1;                            changed = 1; break;
                case 4: if (tmp_bv  <= 90) { tmp_bv  += 10;   changed = 1; } break;
                case 5: if (tmp_sv  <= 90) { tmp_sv  += 10;   changed = 1; } break;
            }
            if (changed) { cooldown = 8; next_se = SE_CURSOR; dirty = 1; }
        }

        // スペース/Aで決定
        if (IsButtonPressed()) {
            next_se = SE_BUTTON;
            if (cursor == 7) {
                // OK: 確定・保存・反映
                u8 filter_changed = (tmp_flt != g_setting_filter);
                g_setting_window_scale = tmp_ws;
                g_setting_fullscreen   = tmp_fs;
                g_setting_filter       = tmp_flt;
                g_setting_bgm_type     = tmp_bt;
                g_setting_bgm_vol      = tmp_bv;
                g_setting_sfx_vol      = tmp_sv;
                Setting_ApplyWindowScale(g_setting_window_scale);
                Setting_ApplyFullscreen(g_setting_fullscreen);
                Setting_ApplyBGMVolume(g_setting_bgm_vol);
                Setting_ApplySFXVolume(g_setting_sfx_vol);
                // BLUR切り替え時はスクリーンテクスチャを再生成
                if (filter_changed) VDP_CreateScreenTextures();
                Setting_Save();
                Input_BlockButton();
                break;
            } else if (cursor == 6) {
                // CANCEL: 破棄して戻る
                Input_BlockButton();
                break;
            }
            cooldown = 15;
        }

        // ESCもCANCEL
        if (Keyboard_IsKeyPressed(KEY_ESC)) {
            next_se = SE_BUTTON;
            break;
        }

        // 変化した行のみ再描画
        if (dirty) {
            if (prev != cursor) {
                // 旧カーソル行
                S_Print_Char(0, 2, 6 + prev, ' ');
                draw_setting_row(prev, cursor, tmp_ws, tmp_fs, tmp_flt, tmp_bt, tmp_bv, tmp_sv);
            }
            // 現在カーソル行（値変更があった場合も含む）
            draw_setting_row(cursor, cursor, tmp_ws, tmp_fs, tmp_flt, tmp_bt, tmp_bv, tmp_sv);
        }
    }

    Tile_FillScreen(COLOR_BLACK);
}
