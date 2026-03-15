// audio_sdl2.c
// AKG Player互換スタブ → SDL_mixerで置き換え
// BGM/SFXのOGGファイルを assets/bgm/ assets/sfx/ に配置する

#include "audio_print_hal.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>

// BGMファイルパス（IDと対応）
static const char* BGM_FILES[] = {
    "assets/bgm/title.ogg",    // 0: BGM_TITLE
    "assets/bgm/main.ogg",     // 1: BGM_MAIN
    "assets/bgm/win.ogg",      // 2: BGM_WIN
    NULL,                       // 3: BGM_STOP
    "assets/bgm/mystery.ogg",  // 4: BGM_MYSTERY
    "assets/bgm/ending.ogg",   // 5: BGM_ENDING
    "assets/bgm/ranking.ogg",  // 6: BGM_RANKING
    "assets/bgm/name_entry.wav", // 7: BGM_NAME_ENTRY
    "assets/bgm/fanfare.ogg",  // 8: BGM_FANFARE
    "assets/bgm/top.ogg",       // 9: BGM_TOP
};
#define BGM_COUNT  10

// SFXファイルパス
static const char* SFX_FILES[] = {
    "assets/sfx/cursor.wav",   // 0: SE_CURSOR
    "assets/sfx/button.wav",   // 1: SE_BUTTON
    "assets/sfx/set.wav",      // 2: SE_SET
    "assets/sfx/super.wav",    // 3: SE_SUPER
    "assets/sfx/bomb.wav",     // 4: SE_BOMB
    "assets/sfx/energy.wav",   // 5: SE_ENERGY
    "assets/sfx/don.wav",      // 6: SE_DON
    "assets/sfx/battery.wav",  // 7: SE_BATTERY
    "assets/sfx/opendoor.wav", // 8: SE_OPENDOOR
    "assets/sfx/keyword.wav",  // 9: SE_KEYWORD
    "assets/sfx/pause.wav",    // 10: SE_PAUSE
};
#define SFX_COUNT  11

static Mix_Music* s_bgm[BGM_COUNT];
static Mix_Chunk* s_sfx[SFX_COUNT];
static int        s_audio_ok = 0;

void Audio_Init(void) {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "SDL_mixer init failed: %s\n", Mix_GetError());
        return;
    }
    s_audio_ok = 1;

    memset(s_bgm, 0, sizeof(s_bgm));
    memset(s_sfx, 0, sizeof(s_sfx));

    for (int i = 0; i < BGM_COUNT; i++) {
        if (BGM_FILES[i]) {
            s_bgm[i] = Mix_LoadMUS(BGM_FILES[i]);
            if (!s_bgm[i]) {
                fprintf(stderr, "BGM load failed [%d]: %s\n", i, Mix_GetError());
            }
        }
    }
    for (int i = 0; i < SFX_COUNT; i++) {
        if (SFX_FILES[i]) {
            s_sfx[i] = Mix_LoadWAV(SFX_FILES[i]);
            if (!s_sfx[i]) {
                fprintf(stderr, "SFX load failed [%d]: %s\n", i, Mix_GetError());
            }
        }
    }
}

void Audio_Quit(void) {
    if (!s_audio_ok) return;
    for (int i = 0; i < BGM_COUNT; i++) if (s_bgm[i]) Mix_FreeMusic(s_bgm[i]);
    for (int i = 0; i < SFX_COUNT; i++) if (s_sfx[i]) Mix_FreeChunk(s_sfx[i]);
    Mix_CloseAudio();
}

// AKG互換API
void AKG_Play(u8 bgm_id, u16 ram_addr) {
    (void)ram_addr;
    if (!s_audio_ok) return;
    // BGM_STOP=3
    if (bgm_id == 3) { Mix_HaltMusic(); return; }
    if (bgm_id >= BGM_COUNT || !s_bgm[bgm_id]) return;
    int loops = (bgm_id == 2 || bgm_id == 5 || bgm_id == 8) ? 0 : -1;  // win(2)・ending(5)・fanfare(8)は1回再生
    Mix_PlayMusic(s_bgm[bgm_id], loops);
}

void AKG_InitSFX(u16 ram_addr) {
    (void)ram_addr;
    // SDL_mixerではロード済みのため何もしない
}

void AKG_PlaySFX(u8 sfx_id, u8 ch, u8 priority) {
    (void)ch; (void)priority;
    if (!s_audio_ok) return;
    if (sfx_id >= SFX_COUNT || !s_sfx[sfx_id]) return;
    Mix_PlayChannel(-1, s_sfx[sfx_id], 0);
}

void AKG_Decode(void) {
    // SDL_mixer使用時は不要
}

// SETTING画面から呼ばれるボリューム設定（0〜8 → 0〜128に変換）
void Setting_ApplyBGMVolume(u8 vol) {
    if (!s_audio_ok) return;
    Mix_VolumeMusic((int)vol * 128 / 100);
}

void Setting_ApplySFXVolume(u8 vol) {
    if (!s_audio_ok) return;
    Mix_Volume(-1, (int)vol * 128 / 100);
}
