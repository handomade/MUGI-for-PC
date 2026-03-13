#pragma once
// MSXgl 統合互換ヘッダー（SDL2移植版）
// actpazle.c の #include "msxgl.h" がこのファイルに向くように
// CMakeのinclude pathを設定する

#include "msxgl_types.h"
#include "hal.h"
#include "input_hal.h"
#include "audio_print_hal.h"

// actpazle.cが直接includeしているサブヘッダーのダミー
// (MSXgl側のヘッダーがあれば中身は不要、宣言だけ通す)
// math.h, tile.h, vdp.h, input.h はhal.hに統合済み
// sw_sprite.h, s_print.h はhal.hに統合済み

// COMPRESS_USE_PLETTER は SDL2版では常に有効
#define COMPRESS_USE_PLETTER 1
