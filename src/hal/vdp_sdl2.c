// vdp_sdl2.c
// MSXgl VDP/Tile/SW_Sprite/Math の SDL2実装
//
// picdata.bmp (256x80px, 16色) のレイアウト:
//   y=  0〜 15: タイルチップ (ID 0〜15)
//   y= 16〜 47: MUGIアニメーションスプライト
//   y= 48〜 63: 敵スプライト
//   y= 64〜 79: フォント (8x8px)

#include "hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mugi_title_logo.h"

#define TILE_W          16
#define TILE_H          16
#define MAP_W           16
#define MAP_H           13
#define FONT_W           8
#define FONT_H           8
#define FONT_Y          64
#define FONT_FIRST    0x20
#define TILE_BANK_COUNT  7

// --- グローバル (main.cから参照) ---
SDL_Renderer* g_renderer  = NULL;
SDL_Texture*  g_screen[2] = {NULL, NULL};

// --- モジュール内部 ---
static SDL_Texture* s_bank_tex[TILE_BANK_COUNT] = {NULL};
static u8  s_draw_page  = 0;
static u8  s_tile_bank  = 0;
static u8  s_print_bank = 1;
static u16 s_print_font_y = FONT_Y;

typedef struct { u8 visible; i16 x, y; u8 pid; } SWSprite;
typedef struct { u8 bank; u16 src_x, src_y; u8 w, h; } SWSpritePT;
typedef struct { SDL_Texture* tex; int x,y,w,h; u8 valid; } SpriteCache;

static SWSprite   s_sprites[SW_SPRITE_MAX];
static SDL_Texture* s_title_logo_tex = NULL;
static SWSpritePT s_sprite_pts[64];
static SpriteCache s_cache[SW_SPRITE_MAX];

// ============================================================
// 初期化/終了
// ============================================================
int VDP_LoadPicdata(const char* bmp_path) {
    SDL_Surface* surf = SDL_LoadBMP(bmp_path);
    if (!surf) { fprintf(stderr, "VDP_LoadPicdata: %s\n", SDL_GetError()); return -1; }

    // バンク1: タイル用 (カラー0=透明, カラー1=不透明黒)
    SDL_SetColorKey(surf, SDL_TRUE, 0); // index=0 を透明色に
    SDL_Surface* tile_surf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    if (!tile_surf) { SDL_FreeSurface(surf); return -1; }
    if (s_bank_tex[1]) SDL_DestroyTexture(s_bank_tex[1]);
    s_bank_tex[1] = SDL_CreateTextureFromSurface(g_renderer, tile_surf);
    SDL_FreeSurface(tile_surf);
    if (!s_bank_tex[1]) { fprintf(stderr, "VDP_LoadPicdata: bank1 failed\n"); SDL_FreeSurface(surf); return -1; }
    SDL_SetTextureBlendMode(s_bank_tex[1], SDL_BLENDMODE_BLEND);

    // バンク2: スプライト用 (カラー0を透明色にする、bank1と同じ設定)
    // SDL_SetColorKey は既に設定済み
    SDL_Surface* spr_surf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!spr_surf) { fprintf(stderr, "VDP_LoadPicdata: spr_surf failed\n"); return -1; }
    if (s_bank_tex[2]) SDL_DestroyTexture(s_bank_tex[2]);
    s_bank_tex[2] = SDL_CreateTextureFromSurface(g_renderer, spr_surf);
    SDL_FreeSurface(spr_surf);
    if (!s_bank_tex[2]) { fprintf(stderr, "VDP_LoadPicdata: bank2 failed\n"); return -1; }
    SDL_SetTextureBlendMode(s_bank_tex[2], SDL_BLENDMODE_BLEND);

    // バンク3: フォント用 (完全不透明、カラーキーなし)
    SDL_Surface* font_surf_orig = SDL_LoadBMP(bmp_path);
    if (font_surf_orig) {
        SDL_Surface* font_surf = SDL_ConvertSurfaceFormat(font_surf_orig, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(font_surf_orig);
        if (font_surf) {
            if (s_bank_tex[4]) SDL_DestroyTexture(s_bank_tex[4]);
            s_bank_tex[4] = SDL_CreateTextureFromSurface(g_renderer, font_surf);
            SDL_FreeSurface(font_surf);
            if (s_bank_tex[4])
                SDL_SetTextureBlendMode(s_bank_tex[4], SDL_BLENDMODE_NONE);
        }
    }

    fprintf(stderr, "VDP_LoadPicdata OK: %s (bank1=tile, bank2=sprite, bank4=font_opaque)\n", bmp_path);
    return 0;
}

// mystery_bg: 256x208px を bank5 として読み込む
// 将来ステージごとの別画像に対応できるよう path を引数にしている
int VDP_LoadMysteryBg(const char* bmp_path) {
    SDL_Surface* surf = SDL_LoadBMP(bmp_path);
    if (!surf) { fprintf(stderr, "VDP_LoadMysteryBg: %s\n", SDL_GetError()); return -1; }
    if (s_bank_tex[5]) SDL_DestroyTexture(s_bank_tex[5]);
    s_bank_tex[5] = SDL_CreateTextureFromSurface(g_renderer, surf);
    SDL_FreeSurface(surf);
    if (!s_bank_tex[5]) { fprintf(stderr, "VDP_LoadMysteryBg: bank5 failed\n"); return -1; }
    SDL_SetTextureBlendMode(s_bank_tex[5], SDL_BLENDMODE_NONE);
    fprintf(stderr, "VDP_LoadMysteryBg OK: %s (bank5)\n", bmp_path);
    return 0;
}

// mystery_bg を page2(作業バッファ) に中央寄せで描画 (y=2px, 256x208px)
// GAME_H=212, 画像=208px → 上下2pxずつ黒
void Draw_MysteryBg(void) {
    if (!s_bank_tex[5]) return;
    SDL_SetRenderTarget(g_renderer, g_screen[1]); // page2=g_screen[1]
    // 上下の黒帯
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_Rect top = {0, 0, GAME_W, 2};
    SDL_Rect bot = {0, 210, GAME_W, 2};
    SDL_RenderFillRect(g_renderer, &top);
    SDL_RenderFillRect(g_renderer, &bot);
    // 画像を中央(y=2)に描画
    SDL_Rect dst = {0, 2, 256, 208};
    SDL_RenderCopy(g_renderer, s_bank_tex[5], NULL, &dst);
}

// ブロックが消えた跡に mystery_bg の同座標を切り出してコピー (page2 に描画)
// tx,ty: タイルグリッド座標 (各16px)
void Copy_MysteryBgTile(u8 tx, u8 ty) {
    if (!s_bank_tex[5]) return;
    int px = tx * 16;
    int py = ty * 16 + 2; // y=2px オフセット(中央寄せ分)
    SDL_Rect src = {px, py - 4, 16, 16}; // 画像内座標(2px上)
    SDL_Rect dst = {px, py - 2, 16, 16}; // スクリーン座標(2px上)
    SDL_SetRenderTarget(g_renderer, g_screen[1]);
    SDL_RenderCopy(g_renderer, s_bank_tex[5], &src, &dst);
}

// ranking_illust: ランキング画面用イラスト (bank6)
int VDP_LoadRankingIllust(const char* bmp_path) {
    SDL_Surface* surf = SDL_LoadBMP(bmp_path);
    if (!surf) { fprintf(stderr, "VDP_LoadRankingIllust: %s\n", SDL_GetError()); return -1; }
    if (s_bank_tex[6]) SDL_DestroyTexture(s_bank_tex[6]);
    s_bank_tex[6] = SDL_CreateTextureFromSurface(g_renderer, surf);
    SDL_FreeSurface(surf);
    if (!s_bank_tex[6]) { fprintf(stderr, "VDP_LoadRankingIllust: bank6 failed\n"); return -1; }
    SDL_SetTextureBlendMode(s_bank_tex[6], SDL_BLENDMODE_NONE);
    fprintf(stderr, "VDP_LoadRankingIllust OK: %s (bank6)\n", bmp_path);
    return 0;
}

// ランキングイラストをpage0に描画 (行17〜22, 中央寄せ, 99×56px)
void Draw_RankingIllust(void) {
    if (!s_bank_tex[6]) return;
    SDL_SetRenderTarget(g_renderer, g_screen[0]);
    SDL_Rect dst = { 78, 136, 99, 56 };
    SDL_RenderCopy(g_renderer, s_bank_tex[6], NULL, &dst);
}

int VDP_CreateScreenTextures(void) {
    // g_setting_filter==2(BLUR)時はリニアフィルタリングを使用
    extern u8 g_setting_filter;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                (g_setting_filter >= 2) ? "1" : "0");
    for (int i = 0; i < 2; i++) {
        if (g_screen[i]) SDL_DestroyTexture(g_screen[i]);
        g_screen[i] = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_TARGET, GAME_W, GAME_H);
        if (!g_screen[i]) { fprintf(stderr, "CreateScreenTextures[%d]: %s\n", i, SDL_GetError()); return -1; }
        SDL_SetTextureBlendMode(g_screen[i], SDL_BLENDMODE_BLEND);
    }
    for (int i = 0; i < SW_SPRITE_MAX; i++) {
        if (s_cache[i].tex) { SDL_DestroyTexture(s_cache[i].tex); s_cache[i].tex = NULL; }
        s_cache[i].valid = 0;
    }
    return 0;
}

void VDP_Cleanup(void) {
    if (s_title_logo_tex) { SDL_DestroyTexture(s_title_logo_tex); s_title_logo_tex = NULL; }
    for (int i = 0; i < TILE_BANK_COUNT; i++) {
        if (s_bank_tex[i]) { SDL_DestroyTexture(s_bank_tex[i]); s_bank_tex[i] = NULL; }
    }
    for (int i = 0; i < 2; i++) {
        if (g_screen[i]) { SDL_DestroyTexture(g_screen[i]); g_screen[i] = NULL; }
    }
    for (int i = 0; i < SW_SPRITE_MAX; i++) {
        if (s_cache[i].tex) { SDL_DestroyTexture(s_cache[i].tex); s_cache[i].tex = NULL; }
    }
}

// ============================================================
// VDP
// ============================================================
void VDP_SetMode(u8 m)         { (void)m; }
void VDP_EnableDisplay(u8 e)   { (void)e; }
void VDP_EnableVBlank(u8 e)    { (void)e; }
void VDP_DisableSprite(void)   {}
u8   VDP_GetFrequency(void)    { return VDP_FREQ_60HZ; }
void VDP_SetColor(u8 c)        { (void)c; }

void VDP_ClearVRAM(void) {
    for (int i = 0; i < 2; i++) {
        if (!g_screen[i]) continue;
        SDL_SetRenderTarget(g_renderer, g_screen[i]);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);
    }
    SDL_SetRenderTarget(g_renderer, NULL);
}

void VDP_CommandHMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 w, u16 h) {
    // MSX2 VRAM: sy/dy >= 256 はpage1を意味する
    // SDL2版: srcがpage1の場合はg_screen[1]ではなくpicdata(bank1テクスチャ)から読む
    //         MSX2のVRAMpage1にはLoad_Picdataで画像が入っているため
    int dp = (dy >= 256) ? 1 : 0;
    u16 sly = sy % 256, dly = dy % 256;
    if (!g_screen[dp]) return;
    SDL_SetRenderTarget(g_renderer, g_screen[dp]);
    if (sy >= 256) {
        // srcはpicdata(bank1) ※透過なしで転送
        SDL_Texture* src_tex = s_bank_tex[1];
        if (!src_tex) { SDL_SetRenderTarget(g_renderer, NULL); return; }
        SDL_BlendMode prev;
        SDL_GetTextureBlendMode(src_tex, &prev);
        SDL_SetTextureBlendMode(src_tex, SDL_BLENDMODE_NONE);
        SDL_Rect sr = {(int)sx, (int)sly, (int)w, (int)h};
        SDL_Rect dr = {(int)dx, (int)dly, (int)w, (int)h};
        SDL_RenderCopy(g_renderer, src_tex, &sr, &dr);
        SDL_SetTextureBlendMode(src_tex, prev);
    } else {
        // srcはg_screen[0]
        SDL_Rect sr = {(int)sx, (int)sly, (int)w, (int)h};
        SDL_Rect dr = {(int)dx, (int)dly, (int)w, (int)h};
        SDL_RenderCopy(g_renderer, g_screen[0], &sr, &dr);
    }
    SDL_SetRenderTarget(g_renderer, NULL);
}

void VDP_CommandPSET(u16 x, u16 y, u8 color, u8 op) {
    (void)op;
    static const SDL_Color pal[16] = {
        {0,0,0,255},{0,1,1,255},{62,184,73,255},{116,208,125,255},
        {89,85,224,255},{128,118,241,255},{185,94,81,255},{101,219,239,255},
        {219,101,89,255},{255,137,125,255},{204,195,94,255},{222,208,135,255},
        {58,162,65,255},{183,102,181,255},{204,204,204,255},{255,255,255,255},
    };
    int pg = (y >= 256) ? 1 : 0;
    SDL_Color c = pal[color & 0x0F];
    SDL_SetRenderTarget(g_renderer, g_screen[pg]);
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, 255);
    SDL_RenderDrawPoint(g_renderer, (int)x, (int)(y%256));
    SDL_SetRenderTarget(g_renderer, NULL);
}

// ============================================================
// Tile
// ============================================================
void Tile_SetDrawPage(u8 p)    { s_draw_page = (p>=2)?1:(p<2?p:0); }  // page2→1に変換
void Tile_SelectBank(u8 b)     { s_tile_bank = (b==0)?1:((b<TILE_BANK_COUNT)?b:1); }  // bank0→1に変換
void Tile_SetBankPage(u8 p)    { (void)p; }
void* Tile_GetBankAddress(u8 b){ (void)b; return NULL; }

void Tile_FillScreen(u8 color) {
    if (!g_screen[s_draw_page]) return;
    SDL_SetRenderTarget(g_renderer, g_screen[s_draw_page]);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    if (color != 0) SDL_SetRenderDrawColor(g_renderer, 32, 32, 32, 255);
    SDL_RenderClear(g_renderer);
    SDL_SetRenderTarget(g_renderer, NULL);
}


void Tile_FillTile(u8 tx, u8 ty, u8 color) {
    // 指定タイル座標を単色で塗りつぶす (Tile_DrawTileの単色版)
    // color!=0 なら黒以外の色 (壁タイルID=1 相当の色)
    SDL_Texture* tgt = g_screen[s_draw_page];
    if (!tgt) return;
    SDL_Rect dst = { tx*TILE_W, ty*TILE_H, TILE_W, TILE_H };
    SDL_SetRenderTarget(g_renderer, tgt);
    if (color == 0)
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    else
        SDL_SetRenderDrawColor(g_renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(g_renderer, &dst);
    SDL_SetRenderTarget(g_renderer, NULL);
}
void Tile_DrawTile(u8 tx, u8 ty, u8 tile_id) {
    SDL_Texture* tgt  = g_screen[s_draw_page];
    SDL_Texture* bank = s_bank_tex[s_tile_bank];
    if (!tgt || !bank) return;
    SDL_Rect src = { (tile_id%16)*TILE_W, (tile_id/16)*TILE_H, TILE_W, TILE_H };
    SDL_Rect dst = { tx*TILE_W, ty*TILE_H, TILE_W, TILE_H };
    SDL_SetRenderTarget(g_renderer, tgt);
    SDL_RenderCopy(g_renderer, bank, &src, &dst);
    SDL_SetRenderTarget(g_renderer, NULL);
}

void Tile_DrawScreen(const u8* tilemap) {
    for (u8 y=0; y<MAP_H; y++)
        for (u8 x=0; x<MAP_W; x++)
            Tile_DrawTile(x, y, tilemap[x + y*MAP_W]);
}

void Tile_LoadBank(u8 bank, const u8* data, u16 tile_count) {
    (void)bank; (void)data; (void)tile_count;
    // SDL2版ではVDP_LoadPicdata()でバンク1を設定済みのため不要
}

// ============================================================
// S_Print
// ============================================================
void S_Print_Init(u8 bank, u8 u, u8 font_y) {
    (void)u; (void)bank;
    s_print_bank   = 4; // フォントは常に不透明のbank4を使用
    s_print_font_y = font_y;
}

// ASCII→フォントBMPインデックス 逆引きテーブル
// picdata.bmpのフォントレイアウト(y=64〜79, 8x8px, 32文字×2行):
//   y=64行 [0〜31]:  A B C D E F G H I J K L M N O P Q R S T U V W X Y Z + - × / : '
//   y=72行 [32〜46]: 0 1 2 3 4 5 6 7 8 9   ! ? , .
static int ascii_to_font_idx(u8 ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';      // 0〜25
    if (ch == '+') return 26;
    if (ch == '-') return 27;
    if (ch == '*') return 28;
    if (ch == '/') return 29;
    if (ch == ':') return 30;
    if (ch == ';') return 31;  // セミコロン = picdata(248,64)
    if (ch >= '0' && ch <= '9') return ch - '0' + 32; // 32〜41
    if (ch == ' ') return 42;
    if (ch == '!') return 43;
    if (ch == '?') return 44;
    if (ch == ',') return 45;
    if (ch == '.') return 46;
    if (ch == 0x01) return 63; // カーソル△ = picdata(248,72)
    return -1; // 未定義
}

static void draw_char(SDL_Texture* tgt, u8 ch, int px, int py) {
    SDL_Texture* ft = s_bank_tex[s_print_bank];
    if (!ft || !tgt) return;
    int idx = ascii_to_font_idx(ch);
    if (idx < 0) return;
    int fx = (idx % 32) * FONT_W;
    int fy = (int)s_print_font_y + (idx / 32) * FONT_H;
    SDL_Rect src = {fx, fy, FONT_W, FONT_H};
    SDL_Rect dst = {px, py, FONT_W, FONT_H};
    SDL_SetRenderTarget(g_renderer, tgt);
    SDL_RenderCopy(g_renderer, ft, &src, &dst);
    SDL_SetRenderTarget(g_renderer, NULL);
}

void S_Print_Text(u8 page, u8 tx, u8 ty, const char* str) {
    // page2→1 に変換 (MSX版はpage2をワークバッファとして使用)
    SDL_Texture* tgt = g_screen[(page >= 2) ? 1 : page];
    // MSX版: 1文字=8px, tx/tyはタイル座標(8px単位)
    int px = tx * FONT_W;
    int py = ty * FONT_H;
    while (*str) {
        draw_char(tgt, (u8)*str, px, py);
        px += FONT_W;
        str++;
    }
}

void S_Print_Int(u8 page, u8 tx, u8 ty, u16 val) {
    char buf[8]; snprintf(buf,sizeof(buf),"%u",val);
    S_Print_Text(page,tx,ty,buf);
}

void S_Print_Int_Padded(u8 page, u8 tx, u8 ty, u16 val, u8 width, char pad) {
    char fmt[16], buf[16];
    snprintf(fmt,sizeof(fmt),"%%%c%du", pad=='0'?'0':' ', width);
    snprintf(buf,sizeof(buf),fmt,val);
    S_Print_Text(page,tx,ty,buf);
}

void S_Print_Int_Padded32(u8 page, u8 tx, u8 ty, u32 val, u8 width, char pad) {
    char fmt[16], buf[16];
    snprintf(fmt,sizeof(fmt),"%%%c%du", pad=='0'?'0':' ', width);
    snprintf(buf,sizeof(buf),fmt,val);
    S_Print_Text(page,tx,ty,buf);
}

// ============================================================
// SW_Sprite
// ============================================================
void SW_Sprite_Init(void)   { memset(s_sprites,   0, sizeof(s_sprites));   }
void SW_SpritePT_Init(void) { memset(s_sprite_pts, 0, sizeof(s_sprite_pts)); }

void SW_SetSpritePT(u8 pid, u8 bank, u16 sx, u16 sy, u8 w, u8 h) {
    if (pid >= 64) return;
    // スプライトはカラー0透明のbank2を使用 (bank1はタイル用不透明)
    u8 spr_bank = (bank == 1) ? 2 : bank;
    s_sprite_pts[pid] = (SWSpritePT){spr_bank, sx, sy, w, h};

}

void SW_Sprite_Set(u8 id, u8 x, u8 y, u8 pid) {
    if (id >= SW_SPRITE_MAX) return;
    s_sprites[id].x = x; s_sprites[id].y = y; s_sprites[id].pid = pid;
}

void SW_Sprite_SetVisible(u8 id, u8 v) {
    if (id < SW_SPRITE_MAX) s_sprites[id].visible = v;
}

void SW_Sprite_SetPattern(u8 id, u8 pid) {
    if (id < SW_SPRITE_MAX) s_sprites[id].pid = pid;
}

u8 SW_Sprite_IsVisible(u8 id) {
    return (id < SW_SPRITE_MAX) ? s_sprites[id].visible : 0;
}

void SW_Sprite_SaveCache(u8 id) {
    if (id >= SW_SPRITE_MAX) return;
    u8 pid = s_sprites[id].pid;
    if (pid >= 64) return;
    int w = s_sprite_pts[pid].w, h = s_sprite_pts[pid].h;
    int x = s_sprites[id].x, y = s_sprites[id].y;
    if (!s_cache[id].tex || s_cache[id].w != w || s_cache[id].h != h) {
        if (s_cache[id].tex) SDL_DestroyTexture(s_cache[id].tex);
        s_cache[id].tex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_TARGET, w, h);
        s_cache[id].w = w; s_cache[id].h = h;
    }
    if (!s_cache[id].tex) return;
    SDL_Rect src = {x, y, w, h};
    SDL_SetRenderTarget(g_renderer, s_cache[id].tex);
    SDL_RenderCopy(g_renderer, g_screen[s_draw_page], &src, NULL);
    SDL_SetRenderTarget(g_renderer, NULL);
    s_cache[id].x = x; s_cache[id].y = y; s_cache[id].valid = 1;
}

void SW_Sprite_RestoreCache(i8 id) {
    if (id < 0 || id >= (i8)SW_SPRITE_MAX) return;
    if (!s_cache[id].valid || !s_cache[id].tex) return;
    SDL_Rect dst = {s_cache[id].x, s_cache[id].y, s_cache[id].w, s_cache[id].h};
    SDL_SetRenderTarget(g_renderer, g_screen[s_draw_page]);
    SDL_RenderCopy(g_renderer, s_cache[id].tex, NULL, &dst);
    SDL_SetRenderTarget(g_renderer, NULL);
    s_cache[id].valid = 0;
}

void SW_Sprite_Clear(u8 id) {
    if (id >= SW_SPRITE_MAX) return;
    if (s_cache[id].valid) SW_Sprite_RestoreCache((i8)id);
    s_sprites[id].visible = 0;
}

void SW_Sprite_DrawAll(void) {
    SDL_Texture* tgt = g_screen[s_draw_page];
    if (!tgt) return;
    SDL_SetRenderTarget(g_renderer, tgt);
    for (u8 id = 0; id < SW_SPRITE_MAX; id++) {
        if (!s_sprites[id].visible) continue;
        u8 pid = s_sprites[id].pid;
        if (pid >= 64) continue;
        SDL_Texture* bank = s_bank_tex[s_sprite_pts[pid].bank];
        if (!bank) continue;
        SDL_Rect src = {(int)s_sprite_pts[pid].src_x, (int)s_sprite_pts[pid].src_y,
                        (int)s_sprite_pts[pid].w,     (int)s_sprite_pts[pid].h};
        SDL_Rect dst = {(int)s_sprites[id].x, (int)s_sprites[id].y,
                        (int)s_sprite_pts[pid].w,     (int)s_sprite_pts[pid].h};
        SDL_RenderCopy(g_renderer, bank, &src, &dst);
    }
    SDL_SetRenderTarget(g_renderer, NULL);
}

void SW_Sprite_COPY(void) {
    if (!g_screen[0] || !g_screen[1]) return;
    SDL_SetRenderTarget(g_renderer, g_screen[0]);
    SDL_RenderCopy(g_renderer, g_screen[1], NULL, NULL);
    SDL_SetRenderTarget(g_renderer, NULL);
}

// ============================================================
// Pletter: SDL2版では不使用 (all_stage_data.h の生データを使う)
// ============================================================
void Pletter_UnpackToRAM(const u8* src, u8* dst) {
    fprintf(stderr, "ERROR: Pletter_UnpackToRAM called - replace with memcpy!\n");
    (void)src; (void)dst;
}

// ============================================================
// Math
// ============================================================
void Math_SetRandomSeed8(u8 seed)   { srand((unsigned)seed); }
void Math_SetRandomSeed16(u16 seed) { srand((unsigned)seed); }
u8   Math_GetRandomMax8(u8 max)     { return max ? (u8)(rand()%(int)max) : 0; }

void S_Print_Char(u8 page, u8 tx, u8 ty, char c) {
    char buf[2] = { c, '\0' };
    S_Print_Text(page, tx, ty, buf);
}

// ============================================================
// タイトルロゴ描画
// ============================================================
// カーソル△をbank2(スプライトテクスチャ)から直接描画
void Draw_CursorSprite(u8 page, int dst_x, int dst_y) {
    SDL_Texture* tgt = g_screen[(page >= 2) ? 1 : page];
    SDL_Texture* src = s_bank_tex[2]; // bank2=スプライト（透過あり）
    if (!tgt || !src) return;
    SDL_Rect sr = {248, 72, 8, 8};
    SDL_Rect dr = {dst_x, dst_y, 8, 8};
    SDL_SetRenderTarget(g_renderer, tgt);
    SDL_SetTextureBlendMode(src, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(g_renderer, src, &sr, &dr);
    SDL_SetRenderTarget(g_renderer, NULL);
}

void Draw_TitleLogo(u8 page, int dst_x, int dst_y) {
    // 初回のみテクスチャを生成
    if (!s_title_logo_tex) {
        SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(
            (void*)g_title_logo_rgba,
            TITLE_LOGO_W, TITLE_LOGO_H,
            32, TITLE_LOGO_W * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
        );
        if (surf) {
            s_title_logo_tex = SDL_CreateTextureFromSurface(g_renderer, surf);
            SDL_FreeSurface(surf);
            if (s_title_logo_tex)
                SDL_SetTextureBlendMode(s_title_logo_tex, SDL_BLENDMODE_BLEND);
        }
    }
    if (!s_title_logo_tex || !g_screen[page]) return;
    SDL_SetRenderTarget(g_renderer, g_screen[page]);
    SDL_Rect dst = { dst_x, dst_y, TITLE_LOGO_W, TITLE_LOGO_H };
    SDL_RenderCopy(g_renderer, s_title_logo_tex, NULL, &dst);
}
