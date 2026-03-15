// Page 0: Title and Game Over scenes
// This file is included from actpazle_p0.c to keep heavy UI code and strings off page 1.

#include "hal.h"
#include "input_hal.h"
#include "audio_print_hal.h"
#include "all_stage_data.h"
#include <string.h>

// Scene constants (must match actpazle.c enum order)
#define SCENE_TITLE    0
#define SCENE_STAGENO  1
#define SCENE_STAGE    2
#define SCENE_GAMEOVER 3
#define SCENE_GAMECLEAR 4
#define SCENE_ENDING   5

// Externs from actpazle.c
extern u8 current_scene;  // actpazle.cでは Scene(enum) だが u8 と互換
extern u8 current_stage;
extern u8 max_stages;
extern u8 stage_display_timer;
extern u8 is_60hz_mode;
extern u8 pause;
extern u8 input_cooldown;
extern u8 next_bgm;
extern u8 next_se;
extern u8 g_Frame;
extern u8 gameover_timer;
extern i8 Mugi_Lives;
extern u8 pt;
extern u16 ending_timer;
extern u8 ending_message_index;
extern void WaitVBlank(void);
extern void Ranking_Draw(void);
extern i16  g_scroll_x;
extern u8 g_TileMap[];
extern u32 high_score;

// HAL関数はhal.hで定義済み

// BGM/SFX constants (match actpazle.c enums)
#define BGM_TITLE  0
#define BGM_MAIN   1
#define BGM_WIN    2
#define BGM_STOP   3
#define BGM_MYSTERY 4
#define BGM_ENDING 5

#define SE_CURSOR  0
#define SE_BUTTON  1
#define SE_SET     2
#define SE_SUPER   3
#define SE_BOMB    4
#define SE_ENERGY  5
#define SE_DON     6
#define SE_BATTERY 7
#define SE_OPENDOOR 8
#define SE_KEYWORD 9
#define SE_PAUSE   10

// VDP constants
#ifndef VDP_FREQ_50HZ
#define VDP_FREQ_50HZ 0
#define VDP_FREQ_60HZ 1
#endif

#ifndef COLOR_BLACK
#define COLOR_BLACK 0
#endif

#ifndef SW_SPRITE_MAX
#define SW_SPRITE_MAX 32
#endif

// Password/continue
extern u8 continue_stage;
extern char g_password[7];
extern u8 password_mode;
extern u8 password_input_pos;
extern char password_buffer[7];

// エンディングメッセージテーブル (actpazle.cで定義)
extern const char* ending_msg_0[];
extern const char* ending_msg_1[];
extern const char* ending_msg_2[];
extern const char* ending_msg_3[];
extern const char* ending_msg_4[];
extern const char* ending_msg_5[];

// Password input cooldown constants
#define PW_INPUT_COOLDOWN 14    // キー入力後の待機フレーム数（60fps対応）
#define PW_ENTER_COOLDOWN 30     // ENTER確定後の待機フレーム数
#define PW_CANCEL_COOLDOWN 10    // ESCキャンセル後の待機フレーム数
#define PW_BACK_COOLDOWN 8      // バックスペース入力後の待機フレーム数（60fps対応）

// Password result blink state
static u8 s_pw_msg_timer = 0;   // 残りフレーム（点滅表示用）
static u8 s_pw_msg_type = 0;    // 0:なし, 1:成功, 2:失敗, 3:MYSTERY, 4:MAIN, 5:TITLE, 6:CLEAR, 7:216
static const u8 PW_MSG_Y = 14;

// メッセージごとのX座標（中央寄せ）
static const u8 pw_msg_x[] = {
    0,   // 0: なし
    14,  // 1: "OK!" (3文字)
    10,  // 2: "INVALID CODE" (12文字)
    12,  // 3: "MYSTERY" (7文字)
    14,  // 4: "MAIN" (4文字)
    13,  // 5: "TITLE" (5文字)
    13,  // 6: "CLEAR" (5文字)
    14   // 7: "216" (3文字)
};

// 特殊コードテーブル（コード、長さ、BGM、タイプ、SE）
static const struct {
    const char* code;
    u8 len;
    u8 bgm;
    u8 type;
    u8 se;
} special_codes[] = {
    {"MYSTER", 6, BGM_MYSTERY, 3, 0xFF},
    {"MAIN", 4, BGM_MAIN, 4, 0xFF},
    {"TITLE", 5, BGM_TITLE, 5, 0xFF},
    {"CLEAR", 5, BGM_WIN, 6, 0xFF},
    {"216", 3, 0xFF, 7, SE_PAUSE}
};

// メッセージテーブル
static const char* pw_messages[] = {
    "",              // 0: なし
    "OK!",  // 1: 成功
    "INVALID CODE",  // 2: 失敗
    "MYSTERY",  // 3: MYSTERY
    "MAIN",  // 4: MAIN
    "TITLE",  // 5: TITLE
    "CLEAR",  // 6: CLEAR
    "216"   // 7: 216
};

// 無敵モード（特殊コード216用）
u8 invincible_mode = 0;  // 0:通常, 1:無敵（敵に触れてもミスにならない）

// 文字列比較ヘルパー（簡易版）
static u8 StrNCmp(const char* s1, const char* s2, u8 len) {
    for(u8 i = 0; i < len; i++) {
        if(s1[i] != s2[i]) return 1;
    }
    return 0;
}

// パスワード入力文字登録（共通処理）
static inline void RegisterPasswordChar(char c) {
    password_buffer[password_input_pos++] = c;
    input_cooldown = PW_INPUT_COOLDOWN;
    next_se = SE_CURSOR;
}

// Functions from actpazle.c
extern void Stage_start(void);
extern void update_enemies(void);

// Password functions (implemented in actpazle_password.c, also on page 0)
extern void EncodePassword(u8 stage, char out[7]);
extern u8 DecodePassword(const char in[6], u8* out_stage);
extern void Input_BlockButton(void);

// Title scene
// ============================================================
// タイトルメニュー定数
// ============================================================
#define MENU_GAME_START  0
#define MENU_TUTORIAL    1
#define MENU_PASSWORD    2
#define MENU_SETTING     3
#define MENU_EDIT_MODE   4
#define MENU_COUNT       5

// カーソルスプライトのPT番号（Sprite_iniで登録済みの空きを使用）
// PT63を使用（タイトル専用）
#define CURSOR_PT  63

// カーソル画像: picdata.bmp (248,72) 8x8
// Sprite_iniで登録するため、Draw_CursorSprite関数で直接描画する
// SW_SetSpritePTを呼んで登録する

// メニュー表示Y座標（タイル行）
// ロゴ高さ105px = 6.5行。ロゴy=13px → 13+105=118px = 7.4行
// メニューは行9〜13に配置
#define MENU_Y_BASE  16
#define MENU_X_LABEL 9   // "GAME START"等の左端
#define MENU_X_CURSOR 7  // カーソルX（ラベルの2文字左）

// ステージ選択表示X
#define STAGE_DISP_X 16
#define STAGE_DISP_Y 9   // GAME STARTと同じ行の右端

// パスワード結果メッセージY
#define PW_RESULT_Y 21

// タイトル画面全体を再描画（確認ダイアログ消去後など）
static void title_redraw_menu(u8 cursor, u8 stage) {
    Tile_SetDrawPage(0);
    Tile_SelectBank(0);
    Tile_FillScreen(COLOR_BLACK);
    Draw_TitleLogo(0, 0, 16);
    S_Print_Text(0, 6, 0, "MECHANICAL UNIT FOR");
    S_Print_Text(0, 4, 1, "GROWTH AND INDEPENDENCE");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 0, "GAME START");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 1, "TUTORIAL");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 2, "PASSWORD");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 3, "SETTING");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 4, "EDIT MODE");
    S_Print_Text(0, 3, 24, "2026 HANDO;S GAME CHANNEL");
    // カーソルと選択状態
    for(u8 i = 0; i < MENU_COUNT; i++)
        S_Print_Char(0, MENU_X_CURSOR, MENU_Y_BASE + i, ' ');
    S_Print_Char(0, MENU_X_CURSOR, MENU_Y_BASE + cursor, 0x01);
    Draw_CursorSprite(0, MENU_X_CURSOR * 8, (MENU_Y_BASE + cursor) * 8);
    if(cursor == MENU_GAME_START) {
        S_Print_Text(0, MENU_X_LABEL + 11, MENU_Y_BASE, "STAGE:");
        S_Print_Int_Padded(0, MENU_X_LABEL + 17, MENU_Y_BASE, stage, 2, '0');
    }
}

void Game_Title() {
    // ────────────────────────────────────────
    // 初期化
    // ────────────────────────────────────────
    Tile_SetDrawPage(0);
    Tile_SelectBank(0);
    Tile_FillScreen(COLOR_BLACK);

    // カーソルはDraw_CursorSprite()で毎フレーム描画

    // BGM
    next_bgm = BGM_TITLE;

    // パスワード関連リセット
    password_mode = 0;
    password_input_pos = 0;
    for(u8 i = 0; i < 7; i++) password_buffer[i] = 0;
    input_cooldown = 0;
    s_pw_msg_type = 0;
    s_pw_msg_timer = 0;

    // 無敵モードリセット
    invincible_mode = 0;

    // ステージ初期化
    current_stage = (continue_stage > 0) ? continue_stage : 1;

    // メニュー状態
    static u8 menu_cursor = 0;  // 現在選択中のメニュー項目
    u8 menu_input_cooldown = 0;
    u8 confirm_quit = 0;        // 0=通常 1=終了確認ダイアログ中
    u8 title_start_lock = 60;   // タイトル表示後60fはGAME START無効

    // スクロールステート
    // 0=タイトル待機, 1=左スクロール中, 2=ランキング表示, 3=右スクロール中
    u8  scroll_state  = 0;
    u16 scroll_timer  = 0;      // 待機カウンタ
    u8  scroll_clear  = 0;      // スクロール終了後にウィンドウクリアが必要
    #define SCROLL_WAIT_FRAMES  (8 * 50)   // 8秒(50fps)
    #define RANKING_SHOW_FRAMES (10 * 50)  // 10秒
    #define SCROLL_SPEED        4           // px/フレーム（64フレーム≒1.3秒で256px）

    // ────────────────────────────────────────
    // 静的UI描画（ループ外で1回だけ）
    // ────────────────────────────────────────
    // タイトルロゴ: y=0px、高さ105px → フォント行0〜13を占有
    Draw_TitleLogo(0, 0, 16);

    // サブタイトル（ロゴより上、行0〜1）
    S_Print_Text(0, 6, 0, "MECHANICAL UNIT FOR");
    S_Print_Text(0, 4, 1, "GROWTH AND INDEPENDENCE");

    // メニュー項目テキスト（行16〜20、ロゴの下）
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 0, "GAME START");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 1, "TUTORIAL");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 2, "PASSWORD");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 3, "SETTING");
    S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 4, "EDIT MODE");

    // コピーライト（行24）
    S_Print_Text(0, 3, 24, "2026 HANDO;S GAME CHANNEL");

    // ────────────────────────────────────────
    // メインループ
    // ────────────────────────────────────────
    while(1) {
        WaitVBlank();
        joy_state = Joystick_Read(JOY_PORT_1);
        g_Frame = 0;

        if(input_cooldown  > 0) input_cooldown--;
        if(menu_input_cooldown > 0) menu_input_cooldown--;
        if(title_start_lock > 0) title_start_lock--;

        // ────────────────────────
        // スクロールステート管理
        // ────────────────────────
        {
            // キー入力があればスクロール中止してタイトルへ戻す
            u8 any_key = IsUpPressed() || IsDownPressed() || IsLeftPressed() || IsRightPressed()
                      || IsButtonPressed() || Keyboard_IsKeyPressed(KEY_ENTER)
                      || Keyboard_IsKeyPressed(KEY_ESC) || Keyboard_IsKeyPressed(KEY_SPACE);

            if(scroll_state != 0 && any_key) {
                // 即タイトルに戻す
                scroll_state = 0;
                scroll_timer = 0;
                g_scroll_x   = 0;
                scroll_clear = 1;
                // BGMはそのまま継続（再起動しない）
                menu_input_cooldown = 15;
            } else {
                switch(scroll_state) {
                    case 0: // タイトル待機
                        if (!password_mode && !confirm_quit) {
                            scroll_timer++;
                        } else {
                            scroll_timer = 0;
                        }
                        if(scroll_timer >= SCROLL_WAIT_FRAMES) {
                            // ランキング画面を一時的にpage0に描画してg_screen[1]へコピー
                            Ranking_Draw();  // page0に描画
                            // g_screen[0]の内容をg_screen[1]にコピー
                            extern SDL_Renderer* g_renderer;
                            extern SDL_Texture*  g_screen[];
                            SDL_SetRenderTarget(g_renderer, g_screen[1]);
                            SDL_RenderCopy(g_renderer, g_screen[0], NULL, NULL);
                            SDL_SetRenderTarget(g_renderer, NULL);
                            // タイトル画面を再描画してpage0に戻す
                            title_redraw_menu(menu_cursor, current_stage);
                            scroll_state = 1;
                            scroll_timer = 0;
                            // BGMはタイトル曲のまま継続
                        }
                        break;
                    case 1: // 左スクロール中（タイトル→ランキング）
                        g_scroll_x += SCROLL_SPEED;
                        if(g_scroll_x >= GAME_W) {
                            g_scroll_x   = GAME_W;
                            scroll_state = 2;
                            scroll_timer = 0;
                        }
                        break;
                    case 2: // ランキング表示中
                        scroll_timer++;
                        if(scroll_timer >= RANKING_SHOW_FRAMES) {
                            scroll_state = 3;
                            scroll_timer = 0;
                            // BGMはタイトル曲のまま継続
                        }
                        break;
                    case 3: // 右スクロール中（ランキング→タイトル）
                        g_scroll_x -= SCROLL_SPEED;
                        if(g_scroll_x <= 0) {
                            g_scroll_x   = 0;
                            scroll_state = 0;
                            scroll_timer = 0;
                            scroll_clear = 1;
                        }
                        break;
                }
            }
            // スクロール終了後の1フレーム: ウィンドウ全体を黒クリアしてタイトル再描画
            if(scroll_clear) {
                scroll_clear = 0;
                extern SDL_Renderer* g_renderer;
                SDL_SetRenderTarget(g_renderer, NULL);
                SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
                SDL_RenderClear(g_renderer);
                SDL_RenderSetLogicalSize(g_renderer, GAME_W, GAME_H);
                title_redraw_menu(menu_cursor, current_stage);
            }
            // スクロール中もタイトル側(g_screen[0])のSTAGE表示を更新
            if(scroll_state != 0) {
                u8 max_sel = (continue_stage > 0) ? continue_stage : 1;
                if(current_stage < 1) current_stage = 1;
                if(current_stage > max_sel) current_stage = max_sel;
                S_Print_Text(0, MENU_X_LABEL + 11, MENU_Y_BASE + 0, "STAGE:");
                S_Print_Int_Padded(0, MENU_X_LABEL + 17, MENU_Y_BASE + 0, current_stage, 2, '0');
                goto title_loop_end;
            }
        }

        // ────────────────────────
        // パスワード入力モード
        // ────────────────────────
        if(password_mode) {
            const u8 y_label = 22;
            const u8 x_label = 5;
            const u8 x_input = 15;

            S_Print_Text(0, x_label, y_label, "PASSWORD:      ");
            // 入力バッファ表示
            for(u8 i = 0; i < 6; i++) {
                if(i < password_input_pos && password_buffer[i] != 0)
                    S_Print_Char(0, x_input + i, y_label, password_buffer[i]);
                else
                    S_Print_Char(0, x_input + i, y_label, ' ');
            }
            password_buffer[6] = 0;

            if(input_cooldown == 0) {
                // アルファベット・数字入力
                if(password_input_pos < 6) {
                    if(Keyboard_IsKeyPressed(KEY_0)) { RegisterPasswordChar('0'); }
                    else if(Keyboard_IsKeyPressed(KEY_1)) { RegisterPasswordChar('1'); }
                    else if(Keyboard_IsKeyPressed(KEY_2)) { RegisterPasswordChar('2'); }
                    else if(Keyboard_IsKeyPressed(KEY_3)) { RegisterPasswordChar('3'); }
                    else if(Keyboard_IsKeyPressed(KEY_4)) { RegisterPasswordChar('4'); }
                    else if(Keyboard_IsKeyPressed(KEY_5)) { RegisterPasswordChar('5'); }
                    else if(Keyboard_IsKeyPressed(KEY_6)) { RegisterPasswordChar('6'); }
                    else if(Keyboard_IsKeyPressed(KEY_7)) { RegisterPasswordChar('7'); }
                    else if(Keyboard_IsKeyPressed(KEY_8)) { RegisterPasswordChar('8'); }
                    else if(Keyboard_IsKeyPressed(KEY_9)) { RegisterPasswordChar('9'); }
                    else if(Keyboard_IsKeyPressed(KEY_A)) { RegisterPasswordChar('A'); }
                    else if(Keyboard_IsKeyPressed(KEY_B)) { RegisterPasswordChar('B'); }
                    else if(Keyboard_IsKeyPressed(KEY_C)) { RegisterPasswordChar('C'); }
                    else if(Keyboard_IsKeyPressed(KEY_D)) { RegisterPasswordChar('D'); }
                    else if(Keyboard_IsKeyPressed(KEY_E)) { RegisterPasswordChar('E'); }
                    else if(Keyboard_IsKeyPressed(KEY_F)) { RegisterPasswordChar('F'); }
                    else if(Keyboard_IsKeyPressed(KEY_G)) { RegisterPasswordChar('G'); }
                    else if(Keyboard_IsKeyPressed(KEY_H)) { RegisterPasswordChar('H'); }
                    else if(Keyboard_IsKeyPressed(KEY_I)) { RegisterPasswordChar('I'); }
                    else if(Keyboard_IsKeyPressed(KEY_J)) { RegisterPasswordChar('J'); }
                    else if(Keyboard_IsKeyPressed(KEY_K)) { RegisterPasswordChar('K'); }
                    else if(Keyboard_IsKeyPressed(KEY_L)) { RegisterPasswordChar('L'); }
                    else if(Keyboard_IsKeyPressed(KEY_M)) { RegisterPasswordChar('M'); }
                    else if(Keyboard_IsKeyPressed(KEY_N)) { RegisterPasswordChar('N'); }
                    else if(Keyboard_IsKeyPressed(KEY_P)) { RegisterPasswordChar('P'); }
                    else if(Keyboard_IsKeyPressed(KEY_Q)) { RegisterPasswordChar('Q'); }
                    else if(Keyboard_IsKeyPressed(KEY_R)) { RegisterPasswordChar('R'); }
                    else if(Keyboard_IsKeyPressed(KEY_S)) { RegisterPasswordChar('S'); }
                    else if(Keyboard_IsKeyPressed(KEY_T)) { RegisterPasswordChar('T'); }
                    else if(Keyboard_IsKeyPressed(KEY_U)) { RegisterPasswordChar('U'); }
                    else if(Keyboard_IsKeyPressed(KEY_V)) { RegisterPasswordChar('V'); }
                    else if(Keyboard_IsKeyPressed(KEY_W)) { RegisterPasswordChar('W'); }
                    else if(Keyboard_IsKeyPressed(KEY_X)) { RegisterPasswordChar('X'); }
                    else if(Keyboard_IsKeyPressed(KEY_Y)) { RegisterPasswordChar('Y'); }
                    else if(Keyboard_IsKeyPressed(KEY_Z)) { RegisterPasswordChar('Z'); }
                }

                // バックスペース
                if((Keyboard_IsKeyPressed(KEY_DEL) || Keyboard_IsKeyPressed(KEY_BS))
                    && password_input_pos > 0) {
                    password_input_pos--;
                    password_buffer[password_input_pos] = 0;
                    password_buffer[6] = 0;
                    S_Print_Text(0, x_input, y_label, "      ");
                    input_cooldown = PW_BACK_COOLDOWN;
                    next_se = SE_CURSOR;
                }

                // ENTER確定（文字数0はキャンセル扱い）
                if(Keyboard_IsKeyPressed(KEY_ENTER)) {
                    S_Print_Text(0, x_label, y_label, "                          ");
                    if(password_input_pos == 0) {
                        // 空ENTERはキャンセル
                        password_mode = 0;
                        input_cooldown = PW_CANCEL_COOLDOWN;
                        next_se = SE_CURSOR;
                    } else
                    if(password_input_pos == 6) {
                        // MARKUNチェック
                        if(password_buffer[0]=='M' && password_buffer[1]=='A' &&
                           password_buffer[2]=='R' && password_buffer[3]=='K' &&
                           password_buffer[4]=='U' && password_buffer[5]=='N') {
                            current_scene = SCENE_ENDING;
                            ending_timer = 0;
                            ending_message_index = 0;
                            next_bgm = BGM_ENDING;
                            password_mode = 0;
                            password_input_pos = 0;
                            for(u8 i = 0; i < 7; i++) password_buffer[i] = 0;
                            input_cooldown = 30;
                            Tile_SetDrawPage(0);
                            Tile_SelectBank(0);
                            Tile_FillScreen(COLOR_BLACK);
                            return;
                        }
                        // MYSTERYチェック
                        u8 matched = 0;
                        if(StrNCmp(password_buffer, special_codes[0].code, special_codes[0].len) == 0) {
                            next_bgm = special_codes[0].bgm;
                            s_pw_msg_type = special_codes[0].type;
                            s_pw_msg_timer = 90;
                            matched = 1;
                        }
                        // 通常パスワードデコード
                        if(!matched) {
                            u8 decoded_stage;
                            if(DecodePassword(password_buffer, &decoded_stage)) {
                                if(decoded_stage > continue_stage) continue_stage = decoded_stage;
                                current_stage = (continue_stage > 0) ? continue_stage : 1;
                                s_pw_msg_type = 1;
                                s_pw_msg_timer = 90;
                                next_se = SE_PAUSE;
                            } else {
                                s_pw_msg_type = 2;
                                s_pw_msg_timer = 90;
                                next_se = SE_BOMB;
                            }
                        }
                    } else {
                        // 短いコード（MAIN/TITLE/CLEAR/216）
                        u8 matched = 0;
                        for(u8 i = 1; i < 5; i++) {
                            if(StrNCmp(password_buffer, special_codes[i].code, special_codes[i].len) == 0) {
                                if(special_codes[i].bgm != 0xFF) next_bgm = special_codes[i].bgm;
                                if(i == 4) invincible_mode = 1;
                                s_pw_msg_type = special_codes[i].type;
                                s_pw_msg_timer = 90;
                                if(special_codes[i].se != 0xFF) next_se = special_codes[i].se;
                                matched = 1;
                                break;
                            }
                        }
                        if(!matched) {
                            s_pw_msg_type = 2;
                            s_pw_msg_timer = 90;
                            next_se = SE_BOMB;
                        }
                    }
                    password_mode = 0;
                    password_input_pos = 0;
                    for(u8 i = 0; i < 7; i++) password_buffer[i] = 0;
                    input_cooldown = PW_ENTER_COOLDOWN;
                    Input_BlockButton();
                }

                // ESCでキャンセル（menu_input_cooldownをセットして同フレーム伝播防止）
                if(Keyboard_IsKeyPressed(KEY_ESC)) {
                    password_mode = 0;
                    password_input_pos = 0;
                    for(u8 i = 0; i < 7; i++) password_buffer[i] = 0;
                    S_Print_Text(0, x_label, y_label, "                          ");
                    input_cooldown = PW_CANCEL_COOLDOWN;
                    menu_input_cooldown = PW_CANCEL_COOLDOWN; // メニューESCへの伝播を防ぐ
                    next_se = SE_CURSOR;
                }
            }

        } else {
            // ────────────────────────
            // 通常メニュー操作
            // ────────────────────────
            // 終了確認ダイアログ中
            if(confirm_quit) {
                S_Print_Text(0, 11, 9,  "QUIT GAME?");
                S_Print_Text(0, 10, 11, "Y:YES   N:NO");
                if(Keyboard_IsKeyPressed(KEY_Y)) {
                    Setting_SaveOnExit();
                    SDL_Quit();
                    exit(0);
                }
                if(Keyboard_IsKeyPressed(KEY_N)) {
                    confirm_quit = 0;
                    menu_input_cooldown = 15;
                    title_redraw_menu(menu_cursor, current_stage);
                }
            } else {
            // ESCで終了確認ダイアログ
            if(Keyboard_IsKeyPressed(KEY_ESC) && menu_input_cooldown == 0) {
                confirm_quit = 1;
                scroll_timer = 0;
            }
            if(menu_input_cooldown == 0) {
                // 上下でカーソル移動
                if(IsUpPressed()) {
                    if(menu_cursor > 0) menu_cursor--;
                    else menu_cursor = MENU_COUNT - 1;
                    menu_input_cooldown = 8;
                    next_se = SE_CURSOR;
                    scroll_timer = 0;
                } else if(IsDownPressed()) {
                    if(menu_cursor < MENU_COUNT - 1) menu_cursor++;
                    else menu_cursor = 0;
                    menu_input_cooldown = 8;
                    next_se = SE_CURSOR;
                    scroll_timer = 0;
                }

                // 左右はGAME STARTのみステージ変更
                if(menu_cursor == MENU_GAME_START) {
                    u8 max_selectable = (continue_stage > 0) ? continue_stage : 1;
                    if(IsRightPressed()) {
                        if(current_stage < max_selectable) current_stage++;
                        menu_input_cooldown = 6;
                        next_se = SE_CURSOR;
                        scroll_timer = 0;
                    } else if(IsLeftPressed()) {
                        if(current_stage > 1) current_stage--;
                        menu_input_cooldown = 6;
                        next_se = SE_CURSOR;
                        scroll_timer = 0;
                    }
                }

                // スペース/Aボタンで決定
                if(IsButtonPressed()) {
                    next_se = (menu_cursor == MENU_GAME_START && title_start_lock > 0) ? 0xFF : SE_BUTTON;
                    switch(menu_cursor) {
                        case MENU_GAME_START:
                            if (title_start_lock > 0) break;  // 60f以内はスタート不可
                            // ゲーム開始
                            next_bgm = BGM_STOP;
                            Tile_FillScreen(COLOR_BLACK);
                            current_scene = SCENE_STAGENO;
                            stage_display_timer = 45;
                            Input_BlockButton();
                            return;

                        case MENU_TUTORIAL:
                            // ダミー（後日実装）
                            S_Print_Text(0, 6, 23, "COMING SOON!        ");
                            menu_input_cooldown = 60;
                            break;

                        case MENU_PASSWORD:
                            // パスワード入力モードへ
                            password_mode = 1;
                            password_input_pos = 0;
                            for(u8 i = 0; i < 7; i++) password_buffer[i] = 0;
                            input_cooldown = PW_CANCEL_COOLDOWN;
                            next_se = SE_BUTTON;
                            break;

                        case MENU_SETTING:
                            Game_Setting();
                            // 戻ったら画面を再描画
                            Tile_SetDrawPage(0);
                            Tile_SelectBank(0);
                            Tile_FillScreen(COLOR_BLACK);
                            Draw_TitleLogo(0, 0, 16);
                            S_Print_Text(0, 6, 0, "MECHANICAL UNIT FOR");
                            S_Print_Text(0, 4, 1, "GROWTH AND INDEPENDENCE");
                            S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 0, "GAME START");
                            S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 1, "TUTORIAL");
                            S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 2, "PASSWORD");
                            S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 3, "SETTING");
                            S_Print_Text(0, MENU_X_LABEL, MENU_Y_BASE + 4, "EDIT MODE");
                            S_Print_Text(0, 3, 24, "2026 HANDO;S GAME CHANNEL");
                            menu_input_cooldown = 15;
                            break;

                        case MENU_EDIT_MODE:
                            // ダミー（後日実装）
                            S_Print_Text(0, 6, 23, "COMING SOON!        ");
                            menu_input_cooldown = 60;
                            break;
                    }
                }
            }
            } // else (confirm_quit)
        }

        // ────────────────────────
        // 画面更新（毎フレーム）
        // ────────────────────────

        // GAME STARTのステージ番号表示（右端に「ST:01」と表示）
        u8 max_selectable = (continue_stage > 0) ? continue_stage : 1;
        if(current_stage < 1) current_stage = 1;
        if(current_stage > max_selectable) current_stage = max_selectable;
        S_Print_Text(0, MENU_X_LABEL + 11, MENU_Y_BASE + 0, "STAGE:");
        S_Print_Int_Padded(0, MENU_X_LABEL + 17, MENU_Y_BASE + 0, current_stage, 2, '0');

        // カーソルスプライト表示（選択行の左に△）
        // カーソルはSW_Spriteではなくタイルフォントで代用（△=0x1Eをフォントで描画）
        // 全メニュー行のカーソル位置をクリアしてから現在行だけ表示
        // 全カーソル位置を黒でクリアしてから現在位置に描画
        for(u8 i = 0; i < MENU_COUNT; i++)
            S_Print_Char(0, MENU_X_CURSOR, MENU_Y_BASE + i, ' ');
        Draw_CursorSprite(0, MENU_X_CURSOR * 8, (MENU_Y_BASE + menu_cursor) * 8);

        // パスワード結果メッセージ点滅
        if(s_pw_msg_timer > 0) {
            u8 show = ((s_pw_msg_timer / 6) & 1);
            if(show && s_pw_msg_type < 8)
                S_Print_Text(0, pw_msg_x[s_pw_msg_type], PW_RESULT_Y, pw_messages[s_pw_msg_type]);
            else
                S_Print_Text(0, 8, PW_RESULT_Y, "              ");
            s_pw_msg_timer--;
            if(s_pw_msg_timer == 0) {
                S_Print_Text(0, 8, PW_RESULT_Y, "              ");
                s_pw_msg_type = 0;
            }
        }
        title_loop_end:; // スクロール中はここにジャンプ
    }

    next_bgm = BGM_STOP;
    next_se = SE_BUTTON;
    Tile_FillScreen(COLOR_BLACK);
}

// ENDING scene
void Game_Ending() {
    // 初回のみ画面クリアと初期化
    static u8 initialized = 0;
    const u16 MESSAGE_DURATION = 900;
    if(initialized==0) {
        ending_message_index = 0;
        initialized = 1;
        ending_timer = 0;

    }

    //S_Print_Int(0,0,0,ending_timer);
    // スタッフクレジット（メッセージを時間経過で切り替え）

    // メッセージ配列（容量節約のため簡易的に）
    switch(ending_message_index) {
        case 0:
            S_Print_Text(0, 16 - 7 , 4, ending_msg_0[0]);
            S_Print_Text(0, 5, 6, ending_msg_0[1]);
            S_Print_Text(0, 5, 8, ending_msg_0[2]);
            break;
        case 1:
            S_Print_Text(0, 16 - 9, 6, ending_msg_1[0]);
            S_Print_Text(0, 16 - 9, 8, ending_msg_1[1]);
            S_Print_Text(0, 16 - 9,13, ending_msg_1[2]);
            S_Print_Text(0, 16 - 9,15, ending_msg_1[3]);
            S_Print_Text(0, 16 - 9, 17, ending_msg_1[4]);
            break;
        case 2:
            S_Print_Text(0, 16 - 10, 4, ending_msg_2[0]);
            S_Print_Text(0, 16 - 4, 6, ending_msg_2[1]);
            S_Print_Text(0, 16 - 4, 8, ending_msg_2[2]);
            S_Print_Text(0, 16 - 9,12, ending_msg_2[3]);
            S_Print_Text(0, 16 - 7,14, ending_msg_2[4]);
            S_Print_Text(0, 16 - 8,16, ending_msg_2[5]);
            S_Print_Text(0, 16 - 7,18, ending_msg_2[6]);
            S_Print_Text(0, 16 - 8,20, ending_msg_2[7]);
            S_Print_Text(0, 16 - 7,22, ending_msg_2[8]);
            break;
        case 3:
            S_Print_Text(0, 7, 4, ending_msg_3[0]);
            S_Print_Text(0, 7, 6, ending_msg_3[1]);
            S_Print_Text(0, 7, 8, ending_msg_3[2]);
            S_Print_Text(0, 7, 10, ending_msg_3[3]);
            S_Print_Text(0, 7, 12, ending_msg_3[4]);
            S_Print_Text(0, 7, 16, ending_msg_3[5]);
            S_Print_Text(0, 7, 18, ending_msg_3[6]);
            break;
        case 4:
            S_Print_Text(0, 2 , 6, ending_msg_4[0]);
            S_Print_Text(0, 2 , 8, ending_msg_4[1]);
            S_Print_Text(0, 2 , 10, ending_msg_4[2]);
            S_Print_Text(0, 2 , 12, ending_msg_4[3]);
            S_Print_Text(0, 2 , 14, ending_msg_4[4]);
            S_Print_Text(0, 2 , 16, ending_msg_4[5]);
            S_Print_Text(0, 8 , 17, ending_msg_4[6]);
            S_Print_Text(0, 2 , 18, ending_msg_4[7]);
            S_Print_Text(0, 2 , 20, ending_msg_4[8]);
            S_Print_Text(0, 7 , 21, ending_msg_4[9]);
            break;
        case 5:
            S_Print_Text(0, 16 - 10, 6, ending_msg_5[0]);
            S_Print_Text(0, 18 , 8, ending_msg_5[1]);
            S_Print_Text(0, 12, 12, ending_msg_5[2]);
            // 最後のメッセージ、これ以降は変化しない
            VDP_CommandHMMM(120,256+72,108,110,32,8);
            VDP_CommandHMMM(120+32,256+72,108,118,32,8);
            VDP_CommandHMMM(120+64,256+72,108,126,32,8);
            VDP_CommandHMMM(120+96,256+72,109,134,32,8);
            VDP_CommandHMMM(224,256+16,109,142,32,8);
            break;
    }


    // タイマー更新
    
    if(ending_timer >= MESSAGE_DURATION && ending_message_index < 5) {
        ending_timer = 0;
        ending_message_index++;
        Tile_FillScreen(COLOR_BLACK);

    }        

    
    // スペースキーまたはボタンAでタイトルへ戻る
    if(IsButtonPressed()) {
        current_scene = SCENE_TITLE;
        next_bgm = BGM_STOP;
        initialized = 0;
        ending_timer = 0;
        ending_message_index = 0;
    }
    
    WaitVBlank();
}
