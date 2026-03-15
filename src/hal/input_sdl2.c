// input_sdl2.c
// MSXgl互換入力HAL: Joystick / Keyboard / Gamepad

#include "input_hal.h"
#include <SDL2/SDL.h>
#include <string.h>

static const u8*    s_keys      = NULL;
static u8           s_button    = 0;
static u8           s_quit      = 0;
static u8           s_button_blocked = 0;  // ボタン入力を一時的にブロック
static SDL_GameController* s_pad = NULL;

// MSX KeyCode → SDL_Scancode マッピング
static const SDL_Scancode s_key_map[KEY_MAX] = {
    [KEY_0] = SDL_SCANCODE_0, [KEY_1] = SDL_SCANCODE_1,
    [KEY_2] = SDL_SCANCODE_2, [KEY_3] = SDL_SCANCODE_3,
    [KEY_4] = SDL_SCANCODE_4, [KEY_5] = SDL_SCANCODE_5,
    [KEY_6] = SDL_SCANCODE_6, [KEY_7] = SDL_SCANCODE_7,
    [KEY_8] = SDL_SCANCODE_8, [KEY_9] = SDL_SCANCODE_9,
    [KEY_A] = SDL_SCANCODE_A, [KEY_B] = SDL_SCANCODE_B,
    [KEY_C] = SDL_SCANCODE_C, [KEY_D] = SDL_SCANCODE_D,
    [KEY_E] = SDL_SCANCODE_E, [KEY_F] = SDL_SCANCODE_F,
    [KEY_G] = SDL_SCANCODE_G, [KEY_H] = SDL_SCANCODE_H,
    [KEY_I] = SDL_SCANCODE_I, [KEY_J] = SDL_SCANCODE_J,
    [KEY_K] = SDL_SCANCODE_K, [KEY_L] = SDL_SCANCODE_L,
    [KEY_M] = SDL_SCANCODE_M, [KEY_N] = SDL_SCANCODE_N,
    [KEY_O] = SDL_SCANCODE_O, [KEY_P] = SDL_SCANCODE_P,
    [KEY_Q] = SDL_SCANCODE_Q, [KEY_R] = SDL_SCANCODE_R,
    [KEY_S] = SDL_SCANCODE_S, [KEY_T] = SDL_SCANCODE_T,
    [KEY_U] = SDL_SCANCODE_U, [KEY_V] = SDL_SCANCODE_V,
    [KEY_W] = SDL_SCANCODE_W, [KEY_X] = SDL_SCANCODE_X,
    [KEY_Y] = SDL_SCANCODE_Y, [KEY_Z] = SDL_SCANCODE_Z,
    [KEY_F1] = SDL_SCANCODE_F1, [KEY_F2] = SDL_SCANCODE_F2,
    [KEY_F3] = SDL_SCANCODE_F3, [KEY_F4] = SDL_SCANCODE_F4,
    [KEY_F5] = SDL_SCANCODE_F5,
    [KEY_ESC]   = SDL_SCANCODE_ESCAPE,
    [KEY_ENTER] = SDL_SCANCODE_RETURN,
    [KEY_SPACE] = SDL_SCANCODE_SPACE,
    [KEY_DEL]    = SDL_SCANCODE_DELETE,
    [KEY_BS]     = SDL_SCANCODE_BACKSPACE,
    [KEY_PERIOD] = SDL_SCANCODE_PERIOD,
    [KEY_UP]     = SDL_SCANCODE_UP,
    [KEY_DOWN]  = SDL_SCANCODE_DOWN,
    [KEY_LEFT]  = SDL_SCANCODE_LEFT,
    [KEY_RIGHT] = SDL_SCANCODE_RIGHT,
};

void Input_Init(void) {
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            s_pad = SDL_GameControllerOpen(i);
            if (s_pad) {
                fprintf(stderr, "Gamepad connected: %s\n", SDL_GameControllerName(s_pad));
                break;
            }
        }
    }
}

void Input_Update(void) {
    SDL_Event ev;
    s_button_blocked = 0;  // 毎フレーム自動解除
    s_button = 0;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            s_quit = 1;
        }
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_SPACE)
            s_button = 1;
        // ENTERはKeyboard_IsKeyPressed(KEY_ENTER)で個別に判定するためs_buttonに含めない
        // ゲームパッド接続/切断
        if (ev.type == SDL_CONTROLLERDEVICEADDED) {
            if (!s_pad) {
                s_pad = SDL_GameControllerOpen(ev.cdevice.which);
                if (s_pad)
                    fprintf(stderr, "Gamepad connected: %s\n", SDL_GameControllerName(s_pad));
            }
        }
        if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (s_pad && !SDL_GameControllerGetAttached(s_pad)) {
                SDL_GameControllerClose(s_pad);
                s_pad = NULL;
                fprintf(stderr, "Gamepad disconnected\n");
            }
        }
        // ゲームパッドボタン (A/B/スタート → MSXのスペースキー相当)
        if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
            if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
                ev.cbutton.button == SDL_CONTROLLER_BUTTON_B ||
                ev.cbutton.button == SDL_CONTROLLER_BUTTON_START)
                s_button = 1;
        }
    }
    s_keys = SDL_GetKeyboardState(NULL);
    if (s_keys[SDL_SCANCODE_SPACE]) s_button = 1;  // ENTERはs_buttonに含めない
    // ゲームパッドボタン連続押し対応
    if (s_pad) {
        if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_A) ||
            SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_B) ||
            SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_START))
            s_button = 1;
    }
}

u8 Input_IsQuitRequested(void) { return s_quit; }

u8 Keyboard_IsKeyPressed(KeyCode key) {
    if (!s_keys || key >= KEY_MAX) return 0;
    return (u8)(s_keys[s_key_map[key]] ? 1 : 0);
}

u8 Input_IsButtonPressed(void) {
    if (s_button_blocked) return 0;
    return s_button;
}

// ボタン入力を指定フレーム数ブロック（パスワード入力のENTER誤検知防止）
void Input_BlockButton(void) { s_button_blocked = 1; }
void Input_UnblockButton(void) { s_button_blocked = 0; }

// WaitVBlank後に毎フレーム呼ぶ（ブロック解除管理）
// ブロックは次のInput_Updateで自動解除


// ジョイスティック読み取り (カーソルキー / WASD / ゲームパッド十字キー・スティック)
u8 Joystick_Read(u8 port) {
    (void)port;
    u8 state = 0;
    if (s_keys) {
        if (s_keys[SDL_SCANCODE_UP]    || s_keys[SDL_SCANCODE_W]) state |= JOY_UP;
        if (s_keys[SDL_SCANCODE_DOWN]  || s_keys[SDL_SCANCODE_S]) state |= JOY_DOWN;
        if (s_keys[SDL_SCANCODE_LEFT]  || s_keys[SDL_SCANCODE_A]) state |= JOY_LEFT;
        if (s_keys[SDL_SCANCODE_RIGHT] || s_keys[SDL_SCANCODE_D]) state |= JOY_RIGHT;
    }
    if (s_pad) {
        // 十字キー
        if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    state |= JOY_UP;
        if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  state |= JOY_DOWN;
        if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  state |= JOY_LEFT;
        if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) state |= JOY_RIGHT;
        // 左スティック (閾値: 8000)
        Sint16 lx = SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ly = SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (ly < -8000) state |= JOY_UP;
        if (ly >  8000) state |= JOY_DOWN;
        if (lx < -8000) state |= JOY_LEFT;
        if (lx >  8000) state |= JOY_RIGHT;
    }
    return state;
}
