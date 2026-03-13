#pragma once
// audio_print_hal.h - オーディオHAL (SDL2_mixer使用)

#include "msxgl_types.h"

// ============================================================
// AKG互換API (actpazle.cのAKG_*呼び出しをそのまま使える)
// 内部でSDL2_mixerに変換する
// ============================================================

// BGM再生 (ram_addrはSDL2版では無視)
void AKG_Play(u8 bgm_id, u16 ram_addr);

// SFX初期化 (SDL2版では不要、Audio_Init()で済み)
void AKG_InitSFX(u16 ram_addr);

// SFX再生 (ch/priorityはSDL2版では無視)
void AKG_PlaySFX(u8 sfx_id, u8 ch, u8 priority);

// BGMデコード処理 (SDL2_mixer使用時は不要=空実装)
void AKG_Decode(void);

// BGM/SEのID定数は actpazle.c の enum BGM_ID / SE_ID で定義済み

// ============================================================
// 初期化/終了 (main.cから呼ぶ)
// ============================================================
void Audio_Init(void);
void Audio_Quit(void);
