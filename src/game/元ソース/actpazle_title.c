// Page 0: Title and Game Over scenes
// This file is included from actpazle_p0.c to keep heavy UI code and strings off page 1.

#include "actpazle.h"

// Scene constants (must match actpazle.c enum order)
#define SCENE_TITLE    0
#define SCENE_STAGENO  1
#define SCENE_STAGE    2
#define SCENE_GAMEOVER 3
#define SCENE_GAMECLEAR 4
#define SCENE_ENDING   5

// Externs from actpazle.c
extern u8 current_scene;
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
extern u8 Mugi_Lives;
extern u8 pt;
extern u16 ending_timer;
extern u8 ending_message_index;
extern void WaitVBlank(void);
extern u8 g_TileMap[];
extern u32 high_score;

// Helper functions (wrappers in actpazle.c for MSXgl library calls)
extern void Helper_TileSetDrawPage(u8 page);
extern void Helper_TileSelectBank(u8 bank);
extern void Helper_TileFillScreen(u8 color);
extern void Helper_TileDrawScreen(const u8* data);
extern void Helper_PletterUnpackToRAM(const u8* src, u8* dst);
extern u8 Helper_VDPGetFrequency(void);
extern void Helper_SWSprite_Init(void);
extern void Helper_SWSprite_RestoreCache(i8 id);
extern void Helper_SWSprite_SaveCache(u8 id);
extern u8 Helper_SWSprite_IsVisible(u8 id);
extern void Helper_SWSprite_DrawAll(void);
extern void Helper_SWSprite_COPY(void);
extern void Helper_TileSetBankPage(u8 bank);
extern void Helper_TileLoadBank(u8 bank, const u8* data, u16 size);


// Macros to use helper functions
#define Tile_SetDrawPage Helper_TileSetDrawPage
#define Tile_SetBankPage Helper_TileSetBankPage
#define Tile_SelectBank Helper_TileSelectBank
#define Tile_FillScreen Helper_TileFillScreen
#define Tile_DrawScreen Helper_TileDrawScreen
#define Tile_LoadBank Helper_TileLoadBank
#define Pletter_UnpackToRAM Helper_PletterUnpackToRAM
#define VDP_GetFrequency Helper_VDPGetFrequency
#define SW_Sprite_Init Helper_SWSprite_Init
#define SW_Sprite_RestoreCache Helper_SWSprite_RestoreCache
#define SW_Sprite_SaveCache Helper_SWSprite_SaveCache
#define SW_Sprite_IsVisible Helper_SWSprite_IsVisible
#define SW_Sprite_DrawAll Helper_SWSprite_DrawAll
#define SW_Sprite_COPY Helper_SWSprite_COPY

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
#define PW_INPUT_COOLDOWN 7     // キー入力後の待機フレーム数
#define PW_ENTER_COOLDOWN 30     // ENTER確定後の待機フレーム数
#define PW_CANCEL_COOLDOWN 10    // ESCキャンセル後の待機フレーム数
#define PW_BACK_COOLDOWN 4      // バックスペース入力後の待機フレーム数

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

// Title scene
void Game_Title() {
    // タイトル画面の描画
    Tile_SetDrawPage(0);
    Tile_SelectBank(0);
    Tile_FillScreen(COLOR_BLACK); // 画面クリア
    Pletter_UnpackToRAM(g_TITLE, g_TileMap); // タイトルロゴ展開
    Tile_DrawScreen(g_TileMap);

    // タイトル文字の描画
    S_Print_Text(0, 7, 0, "MECHANICAL UNIT FOR");
    S_Print_Text(0, 5, 1, "GROWTH AND INDEPENDENCE");
    S_Print_Text(0, 9, 17, "SPACE KEY START");
    S_Print_Text(0, 8, 24, "HIGH-SCORE:");
    S_Print_Int_Padded32(0, 19, 24, high_score, 6, '0');
    S_Print_Text(0, 3, 25, "2026 HANDO;S GAME CHANNEL");
    S_Print_Text(0, 12, 15, "STAGE");
    //S_Print_Text(0, 0, 0, "FREQ");
    S_Print_Text(0, 8, 19, "P INPUT PASSWORD");

    // 初期設定
    current_scene = SCENE_STAGENO;
    // continue_stageに合わせてcurrent_stageを初期化
    current_stage = (continue_stage > 0) ? continue_stage : 1;
    stage_display_timer = 30;
    s_pw_msg_type = 0;  // パスワードメッセージをクリア
    u8 frame_skip_counter = 0;
    Pletter_UnpackToRAM(g_mugi_bgm, 0xD100); //BGMデータ展開
    next_bgm = BGM_TITLE; // title bgm
    is_60hz_mode = VDP_GetFrequency(); // 現在の周波数モードを取得
    if(is_60hz_mode == VDP_FREQ_50HZ) {
        is_60hz_mode = VDP_FREQ_60HZ;
    } else {
        is_60hz_mode = VDP_FREQ_50HZ;
    }

    // パスワード入力モードの初期化
    password_mode = 0;
    password_input_pos = 0;
    for(u8 i = 0; i < 7; i++) password_buffer[i] = 0;
    input_cooldown = 0;
    
    // 無敵モードをリセット（タイトル画面に戻ったら解除）
    invincible_mode = 0;

    // スタートボタンが押されたらステージ1へ
    while(!IsButtonPressed()) {
        WaitVBlank();
        joy_state = Joystick_Read(JOY_PORT_1);
        if(g_Frame < 2) continue;
        g_Frame = 0;

        // クールダウン更新
        if(input_cooldown > 0) input_cooldown--;

        // パスワード入力モード
        if(password_mode) {
            // パスワード入力表示（画面下部に配置）
            const u8 y_label = 22;    // ラベル行
            const u8 x_label = 8;
            const u8 y_input = 22;    // 入力表示行
            const u8 x_input = 18;    // 入力開始位置
            const u8 y_help  = 23;    // ヘルプ行
            const char blankChar = ' '; // フォントでスペースが空白でない場合は ':' 等に変更

            // ラベル表示
            S_Print_Text(0, x_label, y_label, "PASSWORD:");
            // 入力域を毎フレーム空白でクリアしてからバッファ内容を描画（消し残り防止）
            for(u8 i = 0; i < 6; i++) { S_Print_Char(0, x_input + i, y_input, blankChar); }
            for(u8 i = 0; i < 6; i++) {
                if(i < password_input_pos && password_buffer[i] != 0) {
                    S_Print_Char(0, x_input + i, y_input, password_buffer[i]);
                }
            }
            // NULL終端を常に確保
            password_buffer[6] = 0;
            // ヘルプ
            //S_Print_Text(0, 2, y_help, "ENTER:OK  ESC:CANCEL");

            if(input_cooldown == 0) {
                // 数字入力（各キー個別判定）
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
                }

                // アルファベット入力（I, Oは無効化してBase34に合わせる）
                if(input_cooldown == 0 && password_input_pos < 6) {
                    if(Keyboard_IsKeyPressed(KEY_A)) { RegisterPasswordChar('A'); }
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
                    else if(Keyboard_IsKeyPressed(KEY_O)) { /* skip O */ }
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

                // バックスペース（DEL/BACKSPACE対応）
                if((Keyboard_IsKeyPressed(KEY_DEL) || Keyboard_IsKeyPressed(KEY_BS)) && password_input_pos > 0) {
                    password_input_pos--;
                    // 消した文字位置を空白で上書き（バッファクリアと画面更新）
                    password_buffer[password_input_pos] = 0;
                    password_buffer[6] = 0; // NULL終端を確保
                    // 画面上の文字を即座に空白で消す
                    S_Print_Text(0, x_input, y_input, "      "); // 全6文字分クリア
                    input_cooldown = PW_BACK_COOLDOWN;
                    next_se = SE_CURSOR;
                }

                // ENTERで確定
                if(Keyboard_IsKeyPressed(KEY_ENTER)) {
                    // パスワードUI全体を削除（空白で上書き）
                    S_Print_Text(0, x_label, y_label, "                              "); // PASSWORD:と入力欄をクリア
                    //S_Print_Text(0, 2, y_help, "                              "); // ヘルプをクリア
                    
                    if(password_input_pos == 6) {
                        // 特殊コード "MARKUN" チェック
                        if(password_buffer[0] == 'M' && password_buffer[1] == 'A' &&
                           password_buffer[2] == 'R' && password_buffer[3] == 'K' &&
                           password_buffer[4] == 'U' && password_buffer[5] == 'N') {
                            // エンディングシーンへ遷移
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
                            Tile_FillScreen(COLOR_BLACK); // 画面クリア    
                            return; // タイトル画面ループを抜ける
                        }
                        
                        // 特殊コード "MYSTERY" チェック（6文字パスワードのみ）
                        u8 matched = 0;
                        for(u8 i = 0; i < 1; i++) {  // MYSTERYのみ6文字
                            if(StrNCmp(password_buffer, special_codes[i].code, special_codes[i].len) == 0) {
                                next_bgm = special_codes[i].bgm;
                                s_pw_msg_type = special_codes[i].type;
                                s_pw_msg_timer = 90;
                                matched = 1;
                                break;
                            }
                        }
                        
                        // 特殊コードでない場合は通常のパスワードとしてデコード
                        if(!matched) {
                            u8 decoded_stage;
                            if(DecodePassword(password_buffer, &decoded_stage)) {
                                // 成功
                                if(decoded_stage > continue_stage) {
                                    continue_stage = decoded_stage;
                                }
                                current_stage = (continue_stage > 0) ? continue_stage : 1;
                                // 成功メッセージを点滅表示（しばらくして自動消去）
                                s_pw_msg_type = 1;
                                s_pw_msg_timer = 90; // 約1.5秒相当（環境依存）
                                next_se = SE_PAUSE;
                            } else {
                                // 失敗メッセージを点滅表示（しばらくして自動消去）
                                s_pw_msg_type = 2;
                                s_pw_msg_timer = 90;
                                next_se = SE_BOMB;
                            }
                        }

                    } else {
                        // 特殊コード(4文字以下)チェック
                        u8 matched = 0;
                        for(u8 i = 1; i < 5; i++) {  // MAIN, TITLE, CLEAR, 216
                            if(StrNCmp(password_buffer, special_codes[i].code, special_codes[i].len) == 0) {
                                if(special_codes[i].bgm != 0xFF) next_bgm = special_codes[i].bgm;
                                if(i == 4) invincible_mode = 1;  // 216の場合のみ無敵化
                                s_pw_msg_type = special_codes[i].type;
                                s_pw_msg_timer = 90;
                                if(special_codes[i].se != 0xFF) next_se = special_codes[i].se;
                                matched = 1;
                                break;
                            }
                        }
                        // いずれの特殊コードにも該当しない場合は失敗
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
                }

                // ESCでキャンセル
                if(Keyboard_IsKeyPressed(KEY_ESC)) {
                    password_mode = 0;
                    password_input_pos = 0;
                    for(u8 i = 0; i < 7; i++) password_buffer[i] = 0;
                    input_cooldown = PW_CANCEL_COOLDOWN;
                    next_se = SE_CURSOR;
                    
                    // パスワードUI全体を削除（空白で上書き）
                    S_Print_Text(0, x_label, y_label, "                              "); // PASSWORD:と入力欄をクリア
                    //S_Print_Text(0, 2, y_help, "                              "); // ヘルプをクリア
                }
            }
        } else {
            // 通常のステージセレクト
            if(pause == 0 && input_cooldown == 0){
                // Pキーでパスワードモード
                if(Keyboard_IsKeyPressed(KEY_P)) {
                    password_mode = 1;
                    password_input_pos = 0;
                    for(u8 i = 0; i < 7; i++) password_buffer[i] = 0;
                    input_cooldown = PW_CANCEL_COOLDOWN;
                    next_se = SE_BUTTON;
                    continue;
                }

                if(IsUpPressed()) {
                    current_stage++;
                    pause = 10;
                    next_se = SE_CURSOR;
                }else if(IsDownPressed()) {
                    current_stage--;
                    pause = 10;
                    next_se = SE_CURSOR;
                }else if(IsRightPressed()) {
                    current_stage += 10;
                    pause = 10;
                    next_se = SE_CURSOR;
                }else if(Keyboard_IsKeyPressed(KEY_Z)) {
                    if(is_60hz_mode == VDP_FREQ_50HZ){
                        is_60hz_mode = VDP_FREQ_60HZ;
                    }else{
                        is_60hz_mode = VDP_FREQ_50HZ;
                    }
                    pause = 10;
                    next_se = SE_CURSOR;
                }
            }
        }

        // ステージ範囲制限（continue_stageが上限）
        u8 max_selectable = (continue_stage > 0) ? continue_stage : max_stages;
        if(current_stage < 1) current_stage = 1;
        if(current_stage > max_selectable) current_stage = max_selectable;

        S_Print_Int_Padded(0, 18, 15, current_stage, 2, '0');

        // S_Print_Int(0, 0, 0, is_60hz_mode);

        // パスワード結果メッセージの点滅表示と自動消去
        if(s_pw_msg_timer > 0) {
            // 簡易的な点滅（一定周期で表示/消去を切替）
            u8 show = ((s_pw_msg_timer / 6) & 1);
            if(show && s_pw_msg_type < 8) {
                S_Print_Text(0, pw_msg_x[s_pw_msg_type], PW_MSG_Y, pw_messages[s_pw_msg_type]);
            } else {
                S_Print_Text(0, 10, PW_MSG_Y, "            ");
            }
            s_pw_msg_timer--;
            if(s_pw_msg_timer == 0) {
                S_Print_Text(0, 10, PW_MSG_Y, "            ");
                s_pw_msg_type = 0;
            }
        }
    }

    next_bgm = BGM_STOP; // bgm stop
    next_se = SE_BUTTON;
    Tile_FillScreen(COLOR_BLACK); // 画面クリア
}

// GAME OVER scene
void Game_Over(){
    if(gameover_timer > 0) {
        for(i8 id = SW_SPRITE_MAX-1; id >= 0; --id)
            SW_Sprite_RestoreCache(id);
        update_enemies();

    pt = ++pt % 4;
        for(u8 id = 0; id < SW_SPRITE_MAX; ++id) {
            if(SW_Sprite_IsVisible(id)) {
                SW_Sprite_SaveCache(id);
            }
        }
        SW_Sprite_DrawAll();

        S_Print_Text(2,12,8,"GAME OVER");
        S_Print_Text(2,11,10,"PASS:");
        S_Print_Text(2,16,10,g_password);
        SW_Sprite_COPY();
        if(IsButtonPressed() || Keyboard_IsKeyPressed(KEY_ENTER)){
            gameover_timer = 1;
        }
        gameover_timer--;
    } else {
        current_scene = SCENE_TITLE;
        Tile_SetDrawPage(0);
        Tile_SelectBank(0);
        SW_Sprite_Init();
        Tile_FillScreen(COLOR_BLACK);
        Mugi_Lives = 3;
    }
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
