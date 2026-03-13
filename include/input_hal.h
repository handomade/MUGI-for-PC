#pragma once
// MSXgl互換 HAL: Keyboard / Joystick
// 実装: input_sdl2.c

#include "msxgl_types.h"

//=============================================================================
// ジョイスティック
//=============================================================================
#define JOY_PORT_1  0
#define JOY_PORT_2  1

// joy_stateビットマスク（MSX互換）
#define JOY_UP    0x01
#define JOY_DOWN  0x02
#define JOY_LEFT  0x04
#define JOY_RIGHT 0x08

u8 Joystick_Read(u8 port);

//=============================================================================
// 方向キー判定マクロ（joy_stateを使う）
// actpazle.cのジョイスティック読み取り後に評価される
//=============================================================================
extern u8 joy_state;  // actpazle.cで定義

#define IsUpPressed()    (joy_state & JOY_UP)
#define IsDownPressed()  (joy_state & JOY_DOWN)
#define IsLeftPressed()  (joy_state & JOY_LEFT)
#define IsRightPressed() (joy_state & JOY_RIGHT)
#define IsButtonPressed() Input_IsButtonPressed()

u8 Input_IsButtonPressed(void);

//=============================================================================
// キーボード
//=============================================================================

// MSXキーコード → SDL_Scancode にマッピング（input_sdl2.cで実装）
typedef enum {
    KEY_0 = 0, KEY_1, KEY_2, KEY_3, KEY_4,
    KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E,
    KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
    KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y,
    KEY_Z,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_ESC, KEY_ENTER, KEY_SPACE,
    KEY_DEL, KEY_BS,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_MAX
} KeyCode;

u8 Keyboard_IsKeyPressed(KeyCode key);

// 入力状態の更新（メインループ冒頭で呼ぶ）
void Input_Init(void);
void Input_BlockButton(void);
void Input_UnblockButton(void);
void Input_Update(void);
// 終了フラグ取得
u8 Input_IsQuitRequested(void);
