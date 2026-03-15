#include "hal.h"
#include "input_hal.h"
#include "audio_print_hal.h"
#include "all_stage_data.h"
#include <string.h>
void vblank_process(void);  // SDL2版前方宣言
void ui_update(void);
void pause_draw_guide(void);
void Ranking_Load(void);
void Ranking_Save(void);
void Ranking_DrawCopyright(void);
void Ranking_Insert(u8 rank, const char* name, u32 score, u8 stage, u8 lap);
i8   Ranking_CheckRank(u32 new_score);
void Ranking_Draw_NameEntry(u8 highlight_row, const char* typing_name, u8 typing_len, u32 entry_score, u8 entry_stage, u8 hl_r, u8 hl_g, u8 hl_b, u8 confirmed);
void Stage_start(void);
////データ類のインクルード
// #include "picdata.h"
// #include "TITLE.h"
// #include "stage1.h"
// #include "enemies.h"
#if (COMPRESS_USE_PLETTER)
	#include "compress/pletter.h"
#endif

// ステージデータフォーマット:
// - タイルマップ: 16x13 u8 array (208 bytes)
// - MUGI 初期位置: u8 x, u8 y (2 bytes)
// - 敵: 各敵 { u8 x, u8 y, u8 type_and_dir } (3 bytes/敵, 10敵=30 bytes)
//   - type_and_dir: (type << 4) | direction (タイプ上位4ビット、向き下位4ビット)
// - バッテリー数: u8 (マップからカウント, 1 byte)
// - 扉位置: u8 x, u8 y (マップから取得, 2 bytes)
// 合計: 約243 bytes/ステージ (未圧縮)


// データは all_stage_data.h で定義済み (extern不要)



// SOUND
// BGM/SFXはAKG_*関数経由で再生 (audio_sdl2.c)

#define MUGI_X_MIN			0
#define MUGI_Y_MIN			0
#define MUGI_X_MAX			239
#define MUGI_Y_MAX			191
#define TILE_SIZE			16
#define OOO			0
#define GET_TILE_AT(x, y) (g_TileMap[((x) / TILE_SIZE) + ((y) / TILE_SIZE) * 16])
#define SPEED_FAST 2
#define SPEED_MID 3
#define SPEED_SLOW 8
#define GUARD_RANGE 3
#define ANIM_CYCLE 4
#define MV(e,d) do{switch(d){case 0:(e)->y+=4;break;case 1:(e)->y-=4;break;case 2:(e)->x-=4;break;case 3:(e)->x+=4;break;}}while(0)
#define UD(e) do{if((e)->state==1){(e)->death_counter++;if((e)->death_counter<=30){u8 p=((e)->death_counter/3)%2?46:45;SW_Sprite_Set((e)->sprite_id,(e)->x,(e)->y,p);}else{(e)->state=2;SW_Sprite_SetVisible((e)->sprite_id,0);}}}while(0)
#define MVS(e,d) do{switch(d){case 0:if((e)->y<=251)(e)->y+=4;break;case 1:if((e)->y>=4)(e)->y-=4;break;case 2:if((e)->x>=4)(e)->x-=4;break;case 3:if((e)->x<=251)(e)->x+=4;break;}}while(0)
#define MV8(p,d) do{switch(d){case 0:(p)->y+=8;break;case 1:(p)->y-=8;break;case 2:(p)->x-=8;break;case 3:(p)->x+=8;break;}}while(0)
#define IS_SW(t) ((t)==TILE_SWITCH_PLUS||(t)==TILE_SWITCH_MINUS)
#define TILE_TO_DIR(t) ((t)==TILE_ARROW_DOWN?0:(t)==TILE_ARROW_UP?1:(t)==TILE_ARROW_LEFT?2:3)
u8  g_Frame = 0;

// タイトル↔ランキングスクロール用オフセット（0=タイトル, GAME_W=ランキング）
// 正の値=右にスクロール（タイトルが見える）、増やすとランキングへ
i16 g_scroll_x = 0;   // 0〜GAME_W の範囲
u8  pt = 0;
u8  g_VBlank = 0;
u8  draw_page = 2;
u8  pause = 0;
u8  mugi_x;
u8  mugi_y;
u8  dir;
u8  energy;
i8  Mugi_Lives = 3;
u32 score = 0;
u32 high_score = 0;
#define MAX_SCORE 999999  // スコアの最大値（カンスト・6桁）
u16 time_bonus = 0;
u16 energy_bonus = 0;
u8 win_timer = 0;
u8 next_se = 0xFF;
u8 next_bgm = 0xFF;
u8 mystery_flag = 0;
u8 sfx_loaded = 0;
u8 backup_bgm = 0xFF;
u8 continue_stage = 0;

// エンディングメッセージテーブル
const char* ending_msg_0[] = {"MUGI -STAFF-", "GAME DESIGN: HANDO", "PROGRAMMING: HANDO", 0};
const char* ending_msg_1[] = {"CHARACTERS : HANDO", "GRAPHICS   : HANDO", "COMPOSITION: HANDO", "MUSIC PROGRAMMING: HANDO", "SFX DESIGN : HANDO", 0};
const char* ending_msg_2[] = {"-TECHNICAL SUPPORT-", "AOINEKO", "TARGHAN", "-SPECIAL THANKS-", "HANDO;S FAMILY", "DISCORD MEMBERS", "YOUTUBE VIEWERS", "NITEN1RYU ORE", "TOKIHIRO NAITO", 0};
const char* ending_msg_3[] = {"-SUPER THANKS-", "MSXGL", "ARKOS TRACKER", "EDGE GRAPHICS TOOL", "MAGIC MAPEDITOR", "-HARD+PACKAGE-", "YONE2", 0};
const char* ending_msg_4[] = {"MUGI HAS SUCCESSFULLY ESCAPED", "THE WEAPONS FACTORY!", "A WONDERFUL,FREE LIFE", "NOW SURELY AWAITS!", "DIDN;T I TELL YOU?", ";WONDER IS NATURAL", "THE FAIRYLAND IS REA--;", "--WHOOPS", "PLEASE LOOK FORWARD", " TO WHAT'S NEXT FOR MUGI!", 0};
const char* ending_msg_5[] = {"THANK YOU FOR", "PLAYING!", "THE END", 0};
char g_password[7] = {0};
u8 password_mode = 0;
u8 password_input_pos = 0;
char password_buffer[7] = {0};
u8 input_cooldown = 0;
extern u8 invincible_mode;  // 無敵モード（actpazle_title.cで定義）
enum BGM_ID{
    BGM_TITLE = 0,
    BGM_MAIN,
    BGM_WIN,
    BGM_STOP,
    BGM_MYSTERY,
    BGM_ENDING,
    BGM_RANKING,
    BGM_NAME_ENTRY,
    BGM_FANFARE,
    BGM_TOP,
};

enum SE_ID{
    SE_CURSOR = 0,
    SE_BUTTON,
    SE_SET,
    SE_SUPER,
    SE_BOMB,
    SE_ENERGY,
    SE_DON,
    SE_BATTERY,
    SE_OPENDOOR,
    SE_KEYWORD,
    SE_PAUSE,
};

u8 g_TileMap[208];
extern u8 g_TileMap[16*13];
#define MAP_WIDTH 16
#define MAP_HEIGHT 13

// ジョイスティック状態
u8 joy_state = 0;

typedef enum {
    BLOCK_TYPE_NORMAL = 2,       
    BLOCK_TYPE_BATTERY = 9,      
    BLOCK_TYPE_SWITCH_PLUS = 11,    
    BLOCK_TYPE_SWITCH_MINUS = 12    
} BlockType;
#define TILE_EMPTY 0
#define TILE_WALL 1
#define TILE_BLOCK_NORMAL 2
#define TILE_BATTERY_BOX 3
#define TILE_FLOOR 4
#define TILE_ARROW_UP 5
#define TILE_ARROW_RIGHT 6
#define TILE_ARROW_DOWN 7
#define TILE_ARROW_LEFT 8
#define TILE_SWITCH_PLUS 9
#define TILE_SWITCH_MINUS 10
#define TILE_BATTERY_BLOCK 11
#define TILE_BATTERY_PLACED 12
#define TILE_SUPERCONDUCTOR_BLOCK 13
#define TILE_DOOR_CLOSED 14
#define TILE_DOOR_OPEN 15
#define TILE_HOLE 28
typedef struct {
    u8 x, y;        
    u8 is_open;     
} Door;
Door g_door;            
u8 g_door_exists = 0;
#define PATH_CACHE_SIZE 12
#define STUN_DURATION 60
#define BLINK_INTERVAL 4
#define SPRITE_BLOCK_NORMAL 40
#define SPRITE_BLOCK_SWITCH_PLUS 41
#define SPRITE_BLOCK_SWITCH_MINUS 42
#define SPRITE_BLOCK_BATTERY 43
#define MAX_ENEMIES_DATA 20
typedef struct {
    u8 type;           
    u8 tile_x, tile_y; 
    u8 x, y;           
    u8 direction;      
    u8 speed_counter;  
    u8 speed_max;      
    u8 active;         
    u8 sprite_id;      
    u8 anim_counter;   
    u8 state;          
    u8 death_counter;  
    u8 guard_center_x; 
    u8 guard_center_y; 
    u8 guard_range;    
    u8 path_cache[PATH_CACHE_SIZE]; 
    u8 path_index;     
    u8 is_returning;   
    u8 force_return;   
    u8 stun_counter;
    u8 blink_counter;
} Enemy;
#define MAX_ENEMIES 5
Enemy g_enemies[MAX_ENEMIES];
u8 g_enemy_count = 0;
Enemy g_MUGI;

// ステージ管理変数
u8 current_stage = 1;
u8 max_stages = 1;//デバッグ時は30のまま。完成版では1にする。
u8 stage_display_timer = 0;
u8 door_tile_x = 0;
u8 door_tile_y = 0;
u16 time = 0;
u8 frame_skip_counter = 0;  
typedef enum {
    SCENE_TITLE,
    SCENE_STAGENO,
    SCENE_STAGE,
    SCENE_GAMEOVER,
    SCENE_GAMECLEAR,
    SCENE_ENDING
} Scene;
u8 current_scene = SCENE_TITLE;
u8 gameover_timer = 0;
u16 ending_timer = 0;      // エンディング用タイマー
u8 ending_message_index = 0; // 現在表示中のメッセージインデックス

// ネームエントリー関連
u8 gc_phase = 0;               // ゲームオーバー画面フェーズ
char name_entry_buffer[8];     // 入力バッファ（最大6字 + NULL + パディング）
u8 name_entry_pos = 0;         // 入力済み文字数（0-6）
u8 name_entry_rank = 0xFF;     // ランキング挿入位置 or 0xFF
u8 name_entry_input_cooldown = 0; // キー入力クールダウン
u32 last_score = 0;            // GAME OVER時のスコア保存用

// ステージデータ（各ステージのタイルマップを保存）
// u8 stage_data[2][16*13];  // ステージ1, ステージ2のデータ
u8 stage_data[2][208];  // ステージ1, ステージ2のデータ
void rotate_arrow_floors(u8 switch_type);
void redraw_tilemap();
void update_enemies();
void initialize_enemies();
u8 chk_col(u8 tile_x, u8 tile_y);
void chk_blk_col();
void upd_death(Enemy* enemy);
void rsp_ene(Enemy* enemy);
u8 hie_safe(Enemy* enemy, u8 direction);
void hie_rec(Enemy* enemy, u8 direction);
u8 hie_ret(Enemy* enemy);
void hie_reset(Enemy* enemy);
u8 opp_dir(u8 direction);
void hdl_ene_arr(Enemy* enemy);

u8 get_enemy_pattern(Enemy* enemy) {
    u8 anim_frame = (enemy->anim_counter / ANIM_CYCLE) % 2;
    u8 base_pattern;
    switch(enemy->type) {
        case 0:   
            base_pattern = 16;
            break;
        case 1:  
            base_pattern = 24;
            break;
        case 2:   
            base_pattern = 32;
            break;
        default:
            base_pattern = 16;
            break;
    }
    return base_pattern + enemy->direction * 2 + anim_frame;
}
typedef enum {
    STATE_IDLE,      
    STATE_MOVING,    
    STATE_PUSH_WAIT, 
    STATE_PUSHING,   
    STATE_DEAD,      
    STATE_WIN,
    STATE_PAUSE
} State;
State state = STATE_IDLE;
u8 push_block_id = 0;      
u8 push_block_x = 0;       
u8 push_block_y = 0;       
u8 push_step_count = 0;    
u8 push_wait_count = 0;    
u8 push_wait_dir = 0;      
u8 push_block_type = 0;    
u8 battery_boxes_remaining = 0;  
u8 batteries_placed = 0;         
u8 battery_blocks_total = 0;     
u8 is_60hz_mode = 0;            
u8 keyword[6] = {'M','A','R','K','U','N'};
u8 keyword_visible = 0;  // キーワード表示中フラグ
typedef struct {
    u8 active;      
    u8 x;           
    u8 y;           
    u8 direction;   
    u8 sprite_id;   
    u8 move_counter; 
} EnergyBullet;
#define MAX_ENERGY_BULLETS 2
EnergyBullet energy_bullets[MAX_ENERGY_BULLETS];
u8 energy_bullet_anim_frame = 0;  
u8 energy_bullet_anim_counter = 0;  
u8 energy_bullet_fire_cooldown = 0;  
#define ENERGY_BULLET_COOLDOWN_TIME 5  
typedef struct {
    u8 active;      
    u8 x;           
    u8 y;           
    u8 direction;   
    u8 sprite_id;   
    u8 move_counter; 
    u8 original_tile; 
} SuperconductorBlock;
#define MAX_SUPERCONDUCTOR_BLOCKS 4
SuperconductorBlock superconductor_blocks[MAX_SUPERCONDUCTOR_BLOCKS];
typedef struct {
    u8 tile_x;      
    u8 tile_y;      
    u8 original_tile; 
    u8 active;      
} SuperconductorTileRecord;
#define MAX_SC_REC 8
SuperconductorTileRecord sc_rec[MAX_SC_REC];
u8 is_pushable_block(u8 tile_type) {
    return (tile_type == TILE_BLOCK_NORMAL || 
            tile_type == TILE_BATTERY_BLOCK || 
            tile_type == TILE_SWITCH_PLUS || 
            tile_type == TILE_SWITCH_MINUS ||
            tile_type == TILE_SUPERCONDUCTOR_BLOCK);
}
u8 is_normal_push_block(u8 tile_type) {
    return (tile_type == TILE_BLOCK_NORMAL || 
            tile_type == TILE_BATTERY_BLOCK || 
            tile_type == TILE_SWITCH_PLUS || 
            tile_type == TILE_SWITCH_MINUS);
}
u8 is_switch_block(u8 tile_type) {
    return IS_SW(tile_type);
}

// パスワード関連関数（actpazle_password.c で定義、ページ0に配置）
void EncodePassword(u8 stage, char out[7]);
u8 DecodePassword(const char in[6], u8* out_stage);

void init_energy_bullets() {
    for(u8 i = 0; i < MAX_ENERGY_BULLETS; i++) {
        energy_bullets[i].active = 0;
        energy_bullets[i].move_counter = 0; 
        energy_bullets[i].sprite_id = 1 + i;  
        SW_Sprite_SetVisible(energy_bullets[i].sprite_id, 0); 
        SW_Sprite_SetPattern(energy_bullets[i].sprite_id, 47); 
    }
    energy_bullet_anim_frame = 0;
    energy_bullet_anim_counter = 0;
}
u8 find_free_energy_bullet() {
    for(u8 i = 0; i < MAX_ENERGY_BULLETS; i++) {
        if(!energy_bullets[i].active) {
            return i;
        }
    }
    return 255; 
}
void fire_energy_bullet() {
    if(energy_bullet_fire_cooldown > 0) {
        return;
    }
    energy--;
    ui_update();
    u8 front_tile_x = mugi_x / 16;
    u8 front_tile_y = mugi_y / 16;
    switch(dir) {
        case 0: front_tile_y++; break; 
        case 1: front_tile_y--; break; 
        case 2: front_tile_x--; break; 
        case 3: front_tile_x++; break; 
    }
    if(front_tile_x >= 16 || front_tile_y >= 12) {
        return;
    }
    u8 front_tile = g_TileMap[front_tile_x + front_tile_y * 16];
    if(front_tile == TILE_WALL) {
        return; 
    }
    u8 slot = find_free_energy_bullet();
    if(slot == 255) {
        return; 
    }
    energy_bullets[slot].active = 1;
    energy_bullets[slot].x = mugi_x;
    energy_bullets[slot].y = mugi_y;
    energy_bullets[slot].direction = dir;
    energy_bullets[slot].move_counter = 0; 
    // スプライトを確実に初期化
    SW_Sprite_SetVisible(energy_bullets[slot].sprite_id, 1);
    SW_Sprite_Set(energy_bullets[slot].sprite_id, energy_bullets[slot].x, energy_bullets[slot].y, 47);
    energy_bullet_fire_cooldown = ENERGY_BULLET_COOLDOWN_TIME;
}
void update_energy_bullets() {
    if(energy_bullet_fire_cooldown > 0) {
        energy_bullet_fire_cooldown--;
    }
    for(u8 i = 0; i < MAX_ENERGY_BULLETS; i++) {
        if(!energy_bullets[i].active) continue;
        // 8ピクセル移動（敵の移動と同じ）
        MV8(&energy_bullets[i], energy_bullets[i].direction);
        energy_bullets[i].move_counter++;
        if(energy_bullets[i].move_counter >= 32) { // 8ピクセル移動なので寿命を短く
            energy_bullets[i].active = 0;
            SW_Sprite_SetVisible(energy_bullets[i].sprite_id, 0);
            continue;
        }
        if(energy_bullets[i].x > 240 || energy_bullets[i].y > 192) {
            energy_bullets[i].active = 0;
            SW_Sprite_SetVisible(energy_bullets[i].sprite_id, 0);
            continue;
        }
        // タイル境界到達時のタイル衝突チェック
        if(energy_bullets[i].x % 16 == 0 && energy_bullets[i].y % 16 == 0) {
            u8 tile_x = energy_bullets[i].x / 16;
            u8 tile_y = energy_bullets[i].y / 16;
            if(tile_x >= 16 || tile_y >= 12) {
                energy_bullets[i].active = 0;
                SW_Sprite_SetVisible(energy_bullets[i].sprite_id, 0);
                continue;
            }
            u8 tile_type = g_TileMap[tile_x + tile_y * 16];
            if(IS_SW(tile_type)) {
                switch(energy_bullets[i].direction) {
                    case 0: energy_bullets[i].y = tile_y * 16 - 16; break; 
                    case 1: energy_bullets[i].y = tile_y * 16 + 16; break; 
                    case 2: energy_bullets[i].x = tile_x * 16 + 16; break; 
                    case 3: energy_bullets[i].x = tile_x * 16 - 16; break; 
                }
                rotate_arrow_floors(tile_type);
                energy_bullets[i].active = 0;
                SW_Sprite_SetVisible(energy_bullets[i].sprite_id, 0);
                continue;
            }
            if(tile_type == TILE_WALL || (is_pushable_block(tile_type) && tile_type != TILE_SWITCH_PLUS && tile_type != TILE_SWITCH_MINUS)) {
                switch(energy_bullets[i].direction) {
                    case 0: energy_bullets[i].y = tile_y * 16 - 16; break; 
                    case 1: energy_bullets[i].y = tile_y * 16 + 16; break; 
                    case 2: energy_bullets[i].x = tile_x * 16 + 16; break; 
                    case 3: energy_bullets[i].x = tile_x * 16 - 16; break; 
                }
                energy_bullets[i].active = 0;
                SW_Sprite_SetVisible(energy_bullets[i].sprite_id, 0);
                continue;
            }
            if(tile_type >= TILE_ARROW_UP && tile_type <= TILE_ARROW_LEFT) {
                energy_bullets[i].direction = TILE_TO_DIR(tile_type);
            }
        }
        // 毎回敵との当たり判定をチェック
        for(u8 j = 0; j < MAX_ENEMIES; j++) {
            if(!g_enemies[j].active || g_enemies[j].state != 0) continue;
            if(g_enemies[j].stun_counter > 0) continue;
            if(energy_bullets[i].x >= g_enemies[j].x - 8 && 
               energy_bullets[i].x <= g_enemies[j].x + 8 &&
               energy_bullets[i].y >= g_enemies[j].y - 8 && 
               energy_bullets[i].y <= g_enemies[j].y + 8) {
                g_enemies[j].stun_counter = STUN_DURATION;
                g_enemies[j].blink_counter = 0;
                // ステージ20でHIE(タイプ2)に当たった時、mystery_flagを1にする
                if(current_stage == 20 && g_enemies[j].type == 2 && mystery_flag == 0) {
                    mystery_flag = 1;
                }
                energy_bullets[i].active = 0;
                SW_Sprite_SetVisible(energy_bullets[i].sprite_id, 0);
                break;
            }
        }
    }
}
void init_superconductor_blocks() {
    for(u8 i = 0; i < MAX_SUPERCONDUCTOR_BLOCKS; i++) {
        superconductor_blocks[i].active = 0;
        superconductor_blocks[i].move_counter = 0;
        superconductor_blocks[i].sprite_id = 3 + i;  
        SW_Sprite_SetVisible(superconductor_blocks[i].sprite_id, 0); 
        SW_Sprite_SetPattern(superconductor_blocks[i].sprite_id, 44); // 正しい超電導ブロックパターン 
    }
    for(u8 i = 0; i < MAX_SC_REC; i++) {
        sc_rec[i].active = 0;
    }
}
u8 find_free_superconductor_block() {
    for(u8 i = 0; i < MAX_SUPERCONDUCTOR_BLOCKS; i++) {
        if(!superconductor_blocks[i].active) {
            return i;
        }
    }
    return 255; 
}
void add_block_tile_record(u8 tile_x, u8 tile_y, u8 original_tile) {
    for(u8 i = 0; i < MAX_SC_REC; i++) {
        if(!sc_rec[i].active) {
            sc_rec[i].tile_x = tile_x;
            sc_rec[i].tile_y = tile_y;
            sc_rec[i].original_tile = original_tile;
            sc_rec[i].active = 1;
            return;
        }
    }
}
u8 get_and_remove_block_tile_record(u8 tile_x, u8 tile_y) {
    for(u8 i = 0; i < MAX_SC_REC; i++) {
        if(sc_rec[i].active && 
           sc_rec[i].tile_x == tile_x && 
           sc_rec[i].tile_y == tile_y) {
            u8 original_tile = sc_rec[i].original_tile;
            sc_rec[i].active = 0; 
            return original_tile;
        }
    }
    return TILE_FLOOR; 
}
void activate_superconductor_block(u8 block_x, u8 block_y, u8 push_direction) {
    u8 slot = find_free_superconductor_block();
    if(slot == 255) {
        return; 
    }
    superconductor_blocks[slot].active = 1;
    superconductor_blocks[slot].x = block_x * 16;
    superconductor_blocks[slot].y = block_y * 16;
    superconductor_blocks[slot].direction = push_direction;
    superconductor_blocks[slot].move_counter = 0;
    SW_Sprite_Set(superconductor_blocks[slot].sprite_id, 
                  superconductor_blocks[slot].x, 
                  superconductor_blocks[slot].y, 
                  44); 
    SW_Sprite_SetVisible(superconductor_blocks[slot].sprite_id, 1);
    u8 restore_tile = get_and_remove_block_tile_record(block_x, block_y);
    if (restore_tile == TILE_FLOOR) {
        g_TileMap[block_x + block_y * 16] = TILE_EMPTY;  
    } else {
        g_TileMap[block_x + block_y * 16] = restore_tile;  
    }
    Tile_SetDrawPage(2);
    Tile_SelectBank(0);
    Tile_DrawTile(block_x, block_y, restore_tile);
}
void update_superconductor_blocks() {
    for(u8 i = 0; i < MAX_SUPERCONDUCTOR_BLOCKS; i++) {
        if(!superconductor_blocks[i].active) continue;
        SW_Sprite_Set(superconductor_blocks[i].sprite_id,
                      superconductor_blocks[i].x,
                      superconductor_blocks[i].y,
                      44);
        MV8(&superconductor_blocks[i], superconductor_blocks[i].direction);
        superconductor_blocks[i].move_counter++;
        if(superconductor_blocks[i].x > 240 || superconductor_blocks[i].y > 192) {
            superconductor_blocks[i].active = 0;
            SW_Sprite_SetVisible(superconductor_blocks[i].sprite_id, 0);
            continue;
        }
        if(superconductor_blocks[i].move_counter % 2 == 0) {
            u8 tile_x = superconductor_blocks[i].x / 16;
            u8 tile_y = superconductor_blocks[i].y / 16;
            if(tile_x >= 16 || tile_y >= 13) {
                superconductor_blocks[i].active = 0;
                SW_Sprite_SetVisible(superconductor_blocks[i].sprite_id, 0);
                continue;
            }
            u8 tile_type = g_TileMap[tile_x + tile_y * 16];
            if(tile_type == TILE_WALL || tile_type == TILE_DOOR_CLOSED || is_pushable_block(tile_type)) {
                switch(superconductor_blocks[i].direction) {
                    case 0: superconductor_blocks[i].y = tile_y * 16 - 16; break; 
                    case 1: superconductor_blocks[i].y = tile_y * 16 + 16; break; 
                    case 2: superconductor_blocks[i].x = tile_x * 16 + 16; break; 
                    case 3: superconductor_blocks[i].x = tile_x * 16 - 16; break; 
                }
                superconductor_blocks[i].active = 0;
                SW_Sprite_SetVisible(superconductor_blocks[i].sprite_id, 0);
                u8 final_tile_x = superconductor_blocks[i].x / 16;
                u8 final_tile_y = superconductor_blocks[i].y / 16;
                u8 original_tile = g_TileMap[final_tile_x + final_tile_y * 16];
                u8 restore_tile;
                if (original_tile == TILE_EMPTY) {
                    restore_tile = TILE_FLOOR;  
                } else {
                    restore_tile = original_tile;  
                }
                add_block_tile_record(final_tile_x, final_tile_y, restore_tile);
                g_TileMap[final_tile_x + final_tile_y * 16] = TILE_SUPERCONDUCTOR_BLOCK;
                Tile_SetDrawPage(2);
                Tile_SelectBank(0);
                Tile_DrawTile(final_tile_x, final_tile_y, TILE_SUPERCONDUCTOR_BLOCK);
                continue;
            }
            if(tile_type >= TILE_ARROW_UP && tile_type <= TILE_ARROW_LEFT) {
                superconductor_blocks[i].direction = TILE_TO_DIR(tile_type);
            }
        }
    }
}
void rotate_arrow_floors(u8 switch_type) {
    u8 clockwise = (switch_type == TILE_SWITCH_PLUS) ? 1 : 0;
    u8 rotation_count = 0;  
    for(u8 i = 0; i < 16 * 12; i++) {
        u8 tile = g_TileMap[i];
        if(tile >= TILE_ARROW_UP && tile <= TILE_ARROW_LEFT) {
            rotation_count++;  
            if(clockwise) {
                switch(tile) {
                    case TILE_ARROW_UP:    g_TileMap[i] = TILE_ARROW_RIGHT; break;  
                    case TILE_ARROW_RIGHT: g_TileMap[i] = TILE_ARROW_DOWN;  break;  
                    case TILE_ARROW_DOWN:  g_TileMap[i] = TILE_ARROW_LEFT;  break;  
                    case TILE_ARROW_LEFT:  g_TileMap[i] = TILE_ARROW_UP;    break;  
                }
            } else {
                switch(tile) {
                    case TILE_ARROW_UP:    g_TileMap[i] = TILE_ARROW_LEFT;  break;  
                    case TILE_ARROW_LEFT:  g_TileMap[i] = TILE_ARROW_DOWN;  break;  
                    case TILE_ARROW_DOWN:  g_TileMap[i] = TILE_ARROW_RIGHT; break;  
                    case TILE_ARROW_RIGHT: g_TileMap[i] = TILE_ARROW_UP;    break;  
                }
            }
        }
    }
    if(rotation_count > 0) {
        if(clockwise) {
            VDP_SetColor(COLOR_LIGHT_BLUE);  
        } else {
            VDP_SetColor(COLOR_LIGHT_RED);   
        }
        for(u16 delay = 0; delay < 5000; delay++);  
        VDP_SetColor(COLOR_BLACK);  
    }
    redraw_tilemap();
}
void redraw_tilemap() {
    Tile_SetDrawPage(2);
    Tile_SelectBank(0);
    for(u8 y = 0; y < 12; y++) {
        for(u8 x = 0; x < 16; x++) {
            u8 tile_type = g_TileMap[x + y * 16];
            Tile_DrawTile(x, y, tile_type);
        }
    }
    VDP_CommandHMMM(0, 2 * 256 + 16, 0, 16, 256, 212 - 16);
}
void draw_energy_bullets() {
    // アニメーション速度を調整（4フレームに1回更新）
    energy_bullet_anim_counter++;
    if(energy_bullet_anim_counter >= 4) {
        energy_bullet_anim_counter = 0;
        energy_bullet_anim_frame = (energy_bullet_anim_frame + 1) % 2;
    }
        for(u8 i = 0; i < MAX_ENERGY_BULLETS; i++) {
        if(energy_bullets[i].active) {
            u8 pattern = (energy_bullet_anim_frame == 0) ? 47 : 48; // 元の47,48に戻す
            SW_Sprite_Set(energy_bullets[i].sprite_id, energy_bullets[i].x, energy_bullets[i].y, pattern);
            SW_Sprite_SetVisible(energy_bullets[i].sprite_id, 1); 
        }
    }
}
u8 get_block_sprite_pattern(u8 tile_type) {
    switch(tile_type) {
        case TILE_BLOCK_NORMAL:
            return SPRITE_BLOCK_NORMAL;
        case TILE_SWITCH_PLUS:
            return SPRITE_BLOCK_SWITCH_PLUS;
        case TILE_SWITCH_MINUS:
            return SPRITE_BLOCK_SWITCH_MINUS;
        case TILE_BATTERY_BLOCK:
        case TILE_BATTERY_PLACED:
            return SPRITE_BLOCK_BATTERY;
        case TILE_SUPERCONDUCTOR_BLOCK:
            return 44;
        default:
            return SPRITE_BLOCK_NORMAL; 
    }
}
u8 is_arrow_floor(u8 tile_type) {
    return (tile_type >= TILE_ARROW_UP && tile_type <= TILE_ARROW_LEFT);
}
u8 is_closed_door(u8 tile_type) {
    return (tile_type == TILE_DOOR_CLOSED);
}
u8 is_open_door(u8 tile_type) {
    return (tile_type == TILE_DOOR_OPEN);
}
void open_door() {
    if (g_door_exists && !g_door.is_open) {
        next_se = SE_OPENDOOR;
        g_door.is_open = 1;
        g_TileMap[g_door.x + g_door.y * 16] = TILE_DOOR_OPEN;  
        Tile_SetDrawPage(2);
        Tile_DrawTile(g_door.x, g_door.y, TILE_DOOR_OPEN);
        VDP_CommandHMMM(g_door.x * 16, 2 * 256 + g_door.y * 16, g_door.x * 16, g_door.y * 16, 16, 16);
    }
}
u8 chk_stg_clr(u8 mugi_tile_x, u8 mugi_tile_y) {
    u8 tile = g_TileMap[mugi_tile_x + mugi_tile_y * 16];
    return is_open_door(tile);
}
// void nitialize_stage_elements() {
//     g_door_exists = 0;
//     battery_boxes_remaining = 0;
//     batteries_placed = 0;
//     battery_blocks_total = 0;
// }
void initialize_enemies() {
    g_enemy_count = 0;
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        g_enemies[i].active = 0;
        g_enemies[i].sprite_id = 10 + i; 
        g_enemies[i].anim_counter = 0;   
        g_enemies[i].state = 0; 
        g_enemies[i].death_counter = 0;  
        g_enemies[i].guard_center_x = 0; 
        g_enemies[i].guard_center_y = 0; 
        g_enemies[i].guard_range = 0;    
        g_enemies[i].stun_counter = 0;
        g_enemies[i].blink_counter = 0;
        hie_reset(&g_enemies[i]);
        SW_Sprite_SetVisible(g_enemies[i].sprite_id, 0);
    }
}

// ステージ表示機能
void display_stage_number() {
    mystery_flag = 0;
    g_door.is_open = 0;
    if (current_stage > 30) {
        current_stage = 1; 
    }
    // 画面を黒でクリアしてからSTAGE番号を表示
    Tile_SetDrawPage(0);
    Tile_SelectBank(1);
    Tile_FillScreen(COLOR_BLACK);
    S_Print_Text(0, 10, 10, "STAGE");
    S_Print_Int(0, 16, 10, current_stage);
    stage_display_timer--;
    if(stage_display_timer == 0) {
        g_MUGI.x = mugi_x;
        g_MUGI.y = mugi_y;
        g_MUGI.state = 0;
        g_MUGI.death_counter = 0;
        state = STATE_IDLE;        
        current_scene = SCENE_STAGE;
        Stage_start();        
        // Pletter_UnpackToRAM(g_mugi_bgm,0xD100); //BGMデータ展開        
        if(current_stage % 5 == 0){
            next_bgm = BGM_MYSTERY; // mystery bgm
            backup_bgm = BGM_MYSTERY;
        }else{
            next_bgm = BGM_MAIN; // bgm main
            backup_bgm = BGM_MAIN;
        }
        
    }
    
}

void Game_Title();
void Game_Over();
void Game_Ending();

// ページ0から呼ぶためのヘルパー関数（MSXglライブラリ関数のラッパー）
void Helper_TileSetDrawPage(u8 page) { Tile_SetDrawPage(page); }
void Helper_TileSelectBank(u8 bank) { Tile_SelectBank(bank); }
void Helper_TileFillScreen(u8 color) { Tile_FillScreen(color); }
void Helper_TileDrawScreen(const u8* data) { Tile_DrawScreen(data); }
void Helper_TileSetBankPage(u8 bank) { Tile_SetBankPage(bank); }
void Helper_TileLoadBank(u8 bank, const u8* data, u16 size) { Tile_LoadBank(bank, data, size); }
// Helper_PletterUnpackToRAM: SDL2版では不使用
u8 Helper_VDPGetFrequency(void) { return VDP_GetFrequency(); }
void Helper_SWSprite_Init(void) { SW_Sprite_Init(); }
void Helper_SWSprite_RestoreCache(i8 id) { SW_Sprite_RestoreCache(id); }
void Helper_SWSprite_SaveCache(u8 id) { SW_Sprite_SaveCache(id); }
u8 Helper_SWSprite_IsVisible(u8 id) { return SW_Sprite_IsVisible(id); }
void Helper_SWSprite_DrawAll(void) { SW_Sprite_DrawAll(); }
void Helper_SWSprite_COPY(void) { SW_Sprite_COPY(); }


void Load_enemies(const u8* enemies) {
    g_enemy_count = 0;
    for(u8 i=0; i < MAX_ENEMIES; i++) {
        // 敵データの先頭バイトが0xFFなら終了
        if(enemies[i*5] == 0xFF) break;
        Enemy* enemy = &g_enemies[i];
        enemy->type = enemies[i*5 + 0]; // 5バイトごとに敵データ
        enemy->tile_x = enemies[i*5 + 1];
        enemy->tile_y = enemies[i*5 + 2];
        enemy->x = enemy->tile_x * TILE_SIZE;  
        enemy->y = enemy->tile_y * TILE_SIZE;  
        enemy->direction = enemies[i*5 + 3];
        enemy->speed_counter = 0;
        enemy->speed_max = enemies[i*5 + 4]; 
        enemy->anim_counter = 0;
        enemy->active = 1;
        enemy->state = 0; 
        enemy->death_counter = 0;

        if(enemy->type == 2) {//HIEの場合
            enemy->guard_center_x = enemy->tile_x;
            enemy->guard_center_y = enemy->tile_y;
            enemy->guard_range = 3;            
            hie_reset(enemy);
        }
        u8 pattern = get_enemy_pattern(enemy);
        SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
        SW_Sprite_SetVisible(enemy->sprite_id, 1);

        g_enemy_count++;
    }

}


u8 chk_col(u8 tile_x, u8 tile_y) {
    for(u8 i = 0; i < g_enemy_count; i++) {
        if (g_enemies[i].active && 
            g_enemies[i].state == 0 &&
            g_enemies[i].stun_counter == 0 &&
            g_enemies[i].tile_x == tile_x && 
            g_enemies[i].tile_y == tile_y) {
            return 1; 
        }
    }
    return 0; 
}

u8 chk_mugi_enemy(Enemy* enemy){
    if (!enemy->active || enemy->state != 0) return 0;
    // 矩形衝突判定
    if (mugi_x < enemy->x + 16 && mugi_x + 16 > enemy->x &&
        mugi_y < enemy->y + 16 && mugi_y + 16 > enemy->y) {
        return 1; // 衝突あり
    }
    return 0;
}
u8 can_enemy_move_to(u8 tile_x, u8 tile_y) {
    if (tile_x >= 16 || tile_y >= 13) {
        return 0;
    }
    u8 tile_type = g_TileMap[tile_x + tile_y * 16];
    if (tile_type == TILE_WALL || 
        tile_type == TILE_BLOCK_NORMAL ||      
        tile_type == TILE_SUPERCONDUCTOR_BLOCK ||
        tile_type == TILE_BATTERY_BLOCK ||
        tile_type == TILE_BATTERY_PLACED ||
        tile_type == TILE_DOOR_CLOSED ||
        tile_type == TILE_SWITCH_PLUS ||       
        tile_type == TILE_SWITCH_MINUS) {      
        return 0;
    }
    u16 pixel_x = tile_x * 16;
    u16 pixel_y = tile_y * 16;
    if(state == STATE_PUSHING && push_block_id > 0) {
        u16 block_pixel_x, block_pixel_y;
        switch(dir) {
            case 0: 
                block_pixel_x = push_block_x * 16;
                block_pixel_y = push_block_y * 16 + (push_step_count + 1) * 2;
                break;
            case 1: 
                block_pixel_x = push_block_x * 16;
                block_pixel_y = push_block_y * 16 - (push_step_count + 1) * 2;
                break;
            case 2: 
                block_pixel_x = push_block_x * 16 - (push_step_count + 1) * 2;
                block_pixel_y = push_block_y * 16;
                break;
            case 3: 
                block_pixel_x = push_block_x * 16 + (push_step_count + 1) * 2;
                block_pixel_y = push_block_y * 16;
                break;
            default:
                block_pixel_x = push_block_x * 16;
                block_pixel_y = push_block_y * 16;
                break;
        }
        if(pixel_x < block_pixel_x + 16 && pixel_x + 16 > block_pixel_x &&
           pixel_y < block_pixel_y + 16 && pixel_y + 16 > block_pixel_y) {
            return 0; 
        }
    }
    for(u8 i = 0; i < MAX_SUPERCONDUCTOR_BLOCKS; i++) {
        if(superconductor_blocks[i].active && 
           SW_Sprite_IsVisible(superconductor_blocks[i].sprite_id)) {
            u8 sprite_x = superconductor_blocks[i].x;
            u8 sprite_y = superconductor_blocks[i].y;
            if(pixel_x < sprite_x + 16 && pixel_x + 16 > sprite_x &&
               pixel_y < sprite_y + 16 && pixel_y + 16 > sprite_y) {
                return 0; 
            }
        }
    }
    return 1; 
}
void hdl_ene_arr(Enemy* enemy) {
    u8 center = g_TileMap[enemy->tile_x + enemy->tile_y * 16];
    if(is_arrow_floor(center)) {
        u8 prev_dir = enemy->direction;
        switch(center) {
            case TILE_ARROW_UP:    enemy->direction = 1; break;    
            case TILE_ARROW_DOWN:  enemy->direction = 0; break;  
            case TILE_ARROW_LEFT:  enemy->direction = 2; break;  
            case TILE_ARROW_RIGHT: enemy->direction = 3; break; 
        }
        // 方向が変わった場合は即座にスプライトパターンを更新
        if (enemy->direction != prev_dir) {
            u8 pattern = get_enemy_pattern(enemy);
            SW_Sprite_SetPattern(enemy->sprite_id, pattern);
        }
    }
}
u8 enemy_common_check(Enemy* enemy) {
    if (enemy->state != 0) return 0;
    if (enemy->stun_counter > 0) {
        enemy->stun_counter--;
        enemy->blink_counter++;
        u8 visible = (enemy->blink_counter / BLINK_INTERVAL) % 2;
        SW_Sprite_SetVisible(enemy->sprite_id, visible);
        if (enemy->stun_counter == 0) {
            SW_Sprite_SetVisible(enemy->sprite_id, 1);
        }
        return 0;
    }
    enemy->speed_counter++;
    enemy->anim_counter++;
    if (enemy->speed_counter < enemy->speed_max) {
        u8 pattern = get_enemy_pattern(enemy);
        SW_Sprite_SetPattern(enemy->sprite_id, pattern);
        return 0;
    }
    enemy->speed_counter = 0;
    return 1;
}
void update_tile_pos(Enemy* enemy) {
    if (enemy->x % 16 == 0 && enemy->y % 16 == 0) {
        enemy->tile_x = enemy->x / 16;
        enemy->tile_y = enemy->y / 16;
        hdl_ene_arr(enemy);
    }
}
void upd_patrol(Enemy* enemy) {
    if (!enemy_common_check(enemy)) return;
    if (enemy->x % 16 == 0 && enemy->y % 16 == 0) {
        // タイル境界に到達した時のみ処理
        u8 tile_x = enemy->x / 16;
        u8 tile_y = enemy->y / 16;
        enemy->tile_x = tile_x;
        enemy->tile_y = tile_y;
        // 1. まず矢印床チェック
        hdl_ene_arr(enemy);
        // 2. その後、次の移動先チェック
        u8 next_tile_x = tile_x;
        u8 next_tile_y = tile_y;
        switch(enemy->direction) {
            case 0:  next_tile_y++; break;
            case 1:    next_tile_y--; break;
            case 2:  next_tile_x--; break;
            case 3: next_tile_x++; break;
        }
        if (!can_enemy_move_to(next_tile_x, next_tile_y)) {
            switch(enemy->direction) {
                case 0:  enemy->direction = 1; break;
                case 1:    enemy->direction = 0; break;
                case 2:  enemy->direction = 3; break;
                case 3: enemy->direction = 2; break;
            }
            u8 pattern = get_enemy_pattern(enemy);
            SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
            return;
        }
    }
    // 移動実行
    MV(enemy, enemy->direction);
    u8 pattern = get_enemy_pattern(enemy);
    SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
}
void upd_chase(Enemy* enemy) {
    if (!enemy_common_check(enemy)) return;
    
    if (enemy->x % 16 == 0 && enemy->y % 16 == 0) {
        // タイル境界に到達した時のみ処理
        u8 tile_x = enemy->x / 16;
        u8 tile_y = enemy->y / 16;
        enemy->tile_x = tile_x;
        enemy->tile_y = tile_y;
        
        // 1. まず矢印床チェック（方向変更される可能性がある）
        hdl_ene_arr(enemy);
        
        // 2. 現在のタイルをチェック
        u8 current_tile = g_TileMap[tile_x + tile_y * 16];
        
        if (is_arrow_floor(current_tile)) {
            // 矢印床の場合：矢印の方向に移動を試行
            u8 next_x = tile_x;
            u8 next_y = tile_y;
            switch(enemy->direction) {
                case 0: next_y++; break;
                case 1: next_y--; break;
                case 2: next_x--; break;
                case 3: next_x++; break;
            }
            // ブロックがあったら立ち止まる（方向変更しない）
            if (!can_enemy_move_to(next_x, next_y)) {
                // 移動できないので停止
                u8 pattern = get_enemy_pattern(enemy);
                SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
                return;
            }
        } else {
            // 通常床の場合：MUGIの方を向いて移動しようとする
            i8 dx = mugi_x - enemy->x;
            i8 dy = mugi_y - enemy->y;
            u8 new_direction = enemy->direction;
            u8 abs_dx = (dx > 0) ? dx : -dx;
            u8 abs_dy = (dy > 0) ? dy : -dy;
            
            // MUGIに向かう方向を決定
            if (abs_dx > abs_dy && dx != 0) {
                new_direction = (dx > 0) ? 3 : 2;
            } else if (dy != 0) {
                new_direction = (dy > 0) ? 0 : 1;
            }
            
            u8 next_x = tile_x;
            u8 next_y = tile_y;
            switch(new_direction) {
                case 0: next_y++; break;
                case 1: next_y--; break;
                case 2: next_x--; break;
                case 3: next_x++; break;
            }
            
            if (can_enemy_move_to(next_x, next_y)) {
                enemy->direction = new_direction;
            } else {
                // ブロックがあるので90度回転して移動を試行（右回転のみ）
                u8 rotated_direction = (new_direction + 1) % 4;
                u8 test_x = tile_x;
                u8 test_y = tile_y;
                switch(rotated_direction) {
                    case 0: test_y++; break;
                    case 1: test_y--; break;
                    case 2: test_x--; break;
                    case 3: test_x++; break;
                }
                
                if (can_enemy_move_to(test_x, test_y)) {
                    enemy->direction = rotated_direction;
                } else {
                    // 右回転もダメなら左回転を試す
                    rotated_direction = (new_direction + 3) % 4;
                    test_x = tile_x;
                    test_y = tile_y;
                    switch(rotated_direction) {
                        case 0: test_y++; break;
                        case 1: test_y--; break;
                        case 2: test_x--; break;
                        case 3: test_x++; break;
                    }
                    
                    if (can_enemy_move_to(test_x, test_y)) {
                        enemy->direction = rotated_direction;
                    } else {
                        // どちらの90度回転もダメなら停止
                        u8 pattern = get_enemy_pattern(enemy);
                        SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
                        return;
                    }
                }
            }
        }
    }
    
    // 移動実行
    MV(enemy, enemy->direction);
    u8 pattern = get_enemy_pattern(enemy);
    SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
}
u8 hie_safe(Enemy* enemy, u8 direction) {
    u8 current_tile_x = enemy->x / 16;
    u8 current_tile_y = enemy->y / 16;
    u8 next_x = enemy->x;
    u8 next_y = enemy->y;
    u8 next_tile_x = current_tile_x;
    u8 next_tile_y = current_tile_y;
    switch(direction) {
        case 0:  
            next_y += 4; 
            next_tile_y = next_y / 16;
            break;
        case 1:    
            if (next_y < 4) return 0; 
            next_y -= 4;
            next_tile_y = next_y / 16;
            break;
        case 2:  
            if (next_x < 4) return 0; 
            next_x -= 4;
            next_tile_x = next_x / 16;
            break;
        case 3: 
            next_x += 4;
            next_tile_x = next_x / 16;
            break;
    }
    if (next_tile_x != current_tile_x || next_tile_y != current_tile_y) {
        return can_enemy_move_to(next_tile_x, next_tile_y);
    }
    return 1; 
}
// u8 hie_escape(Enemy* enemy) {
//     u8 current_tile_x = enemy->x / 16;
//     u8 current_tile_y = enemy->y / 16;
//     if (can_enemy_move_to(current_tile_x, current_tile_y)) {
//         return 0; 
//     }
//     u8 escape_directions[4] = {1, 0, 2, 3};
//     for (u8 i = 0; i < 4; i++) {
//         u8 direction = escape_directions[i];
//         u8 next_x = enemy->x;
//         u8 next_y = enemy->y;
//         u8 next_tile_x = current_tile_x;
//         u8 next_tile_y = current_tile_y;
//         switch(direction) {
//             case 0:  
//                 next_y += 4; 
//                 next_tile_y = next_y / 16;
//                 break;
//             case 1:    
//                 if (next_y < 4) continue; 
//                 next_y -= 4;
//                 next_tile_y = next_y / 16;
//                 break;
//             case 2:  
//                 if (next_x < 4) continue; 
//                 next_x -= 4;
//                 next_tile_x = next_x / 16;
//                 break;
//             case 3: 
//                 next_x += 4;
//                 next_tile_x = next_x / 16;
//                 break;
//         }
//         if (can_enemy_move_to(next_tile_x, next_tile_y)) {
//             enemy->x = next_x;
//             enemy->y = next_y;
//             enemy->direction = direction;
//             return 1; 
//         }
//     }
//     return 0; 
// }
u8 opp_dir(u8 direction) {
    switch(direction) {
        case 1:    return 0;
        case 0:  return 1;
        case 2:  return 3;
        case 3: return 2;
        default: return direction;
    }
}
void hie_rec(Enemy* enemy, u8 direction) {
    if (enemy->type != 2) return;
    if (enemy->is_returning) return;
    if (enemy->path_index >= PATH_CACHE_SIZE) {
        enemy->force_return = 1;
        enemy->is_returning = 1;
        return;
    }
    enemy->path_cache[enemy->path_index] = direction;
    enemy->path_index++;
}
u8 hie_ret(Enemy* enemy) {
    if (enemy->type != 2) return enemy->direction;
    if (!enemy->is_returning) return enemy->direction;
    if (enemy->path_index == 0) {
        // パス履歴が空の場合は直線的にホームに向かう
        i8 home_dx = (enemy->guard_center_x * 16) - enemy->x;
        i8 home_dy = (enemy->guard_center_y * 16) - enemy->y;
        i8 abs_home_dx = (home_dx > 0) ? home_dx : -home_dx;
        i8 abs_home_dy = (home_dy > 0) ? home_dy : -home_dy;
        if (abs_home_dx > abs_home_dy && abs_home_dx > 0) {
            return (home_dx > 0) ? 3 : 2;
        } else if (abs_home_dy > 0) {
            return (home_dy > 0) ? 0 : 1;
        }
        return enemy->direction; 
    }
    enemy->path_index--;
    u8 last_direction = enemy->path_cache[enemy->path_index];
    return opp_dir(last_direction);
}
void hie_reset(Enemy* enemy) {
    if (enemy->type != 2) return;
    enemy->path_index = 0;
    enemy->is_returning = 0;
    enemy->force_return = 0;
    for (u8 i = 0; i < PATH_CACHE_SIZE; i++) {
        enemy->path_cache[i] = 0; 
    }
}
void upd_guard(Enemy* enemy) {
    if (!enemy_common_check(enemy)) return;
    // 正確な座標でホーム判定を行う
    u8 home_x = enemy->guard_center_x * 16;
    u8 home_y = enemy->guard_center_y * 16;
    u8 at_home = (enemy->x == home_x && enemy->y == home_y);
    i8 dx = mugi_x - enemy->x;
    i8 dy = mugi_y - enemy->y;
    i8 dcx = mugi_x - home_x;
    i8 dcy = mugi_y - home_y;
    u8 in_range = (dcx >= -48 && dcx <= 48 && dcy >= -48 && dcy <= 48);
    i8 home_dx = home_x - enemy->x;
    i8 home_dy = home_y - enemy->y;
    u8 too_far = (home_dx > 64 || home_dx < -64 || home_dy > 64 || home_dy < -64);
    
    if (at_home && enemy->is_returning) hie_reset(enemy);
    if (too_far && !enemy->is_returning) enemy->is_returning = 1;
    
    // タイル境界でのみ矢印床チェック
    if (enemy->x % 16 == 0 && enemy->y % 16 == 0) {
        enemy->tile_x = enemy->x / 16;
        enemy->tile_y = enemy->y / 16;
        hdl_ene_arr(enemy);
    }
    if (too_far && !enemy->is_returning) enemy->is_returning = 1;
    u8 new_dir = enemy->direction;
    u8 move = 0;
    if (enemy->is_returning) {
        if (at_home) {
            hie_reset(enemy);
        } else {
            move = 1;
            // タイル境界の調整は最小限に留める
            if (enemy->x % 16 == 0 && enemy->y % 16 == 0) {
                if (enemy->path_index > 0) {
                    new_dir = hie_ret(enemy);
                } else {
                    new_dir = hie_ret(enemy); // パス履歴が空でも直線的にホームに向かう
                }
            }
        }
    } else if (at_home) {
        if (in_range) {
            if (enemy->x % 16 == 0 && enemy->y % 16 == 0) {
                if (dx != 0) new_dir = (dx > 0) ? 3 : 2;
                else if (dy != 0) new_dir = (dy > 0) ? 0 : 1;
            }
            move = 1;
        }
    } else {
        if (in_range) {
            if (enemy->x % 16 == 0 && enemy->y % 16 == 0) {
                if (dx != 0) new_dir = (dx > 0) ? 3 : 2;
                else if (dy != 0) new_dir = (dy > 0) ? 0 : 1;
            }
            move = 1;
        } else {
            enemy->is_returning = 1;
            if (enemy->x % 16 == 0 && enemy->y % 16 == 0) new_dir = hie_ret(enemy);
            move = 1;
        }
    }
    // 移動実行
    if (move && hie_safe(enemy, new_dir)) {
        if (enemy->x % 16 == 0 && enemy->y % 16 == 0) {
            // 進行方向の次タイル座標を計算
            u8 next_x = enemy->x / 16;
            u8 next_y = enemy->y / 16;
            switch(new_dir) {
                case 0: next_y++; break;
                case 1: next_y--; break;
                case 2: next_x--; break;
                case 3: next_x++; break;
            }
            if (can_enemy_move_to(next_x, next_y)) {
                enemy->direction = new_dir;
                if (!enemy->is_returning && !at_home) {
                    hie_rec(enemy, enemy->direction);
                }
                MV(enemy, enemy->direction);
            } else {
                // 障害物があれば停止
                u8 pattern = get_enemy_pattern(enemy);
                SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
                return;
            }
        } else {
            // タイル境界でなければそのままMV
            MV(enemy, enemy->direction);
        }
    }
    // } else if (hie_escape(enemy)) {
    //     // 移動できない場合のみ緊急脱出処理
    //     u8 pattern = get_enemy_pattern(enemy);
    //     SW_Sprite_SetPattern(enemy->sprite_id, pattern);
    //     return;
    // }
    u8 pattern = get_enemy_pattern(enemy);
    SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
}
// ブロックやスイッチとの衝突判定
void chk_blk_col() {
    for(u8 i = 0; i < g_enemy_count; i++) {
        Enemy* enemy = &g_enemies[i];
        if (!enemy->active || enemy->state != 0) continue;
        u8 enemy_tile_x = enemy->x / 16;
        u8 enemy_tile_y = enemy->y / 16;
        u8 tile_type = g_TileMap[enemy_tile_x + enemy_tile_y * 16];
        if (enemy->type == 2) {//
            if(tile_type == TILE_BLOCK_NORMAL || 
               tile_type == TILE_BATTERY_BLOCK || 
               tile_type == TILE_SUPERCONDUCTOR_BLOCK ||
               (tile_type >= TILE_SWITCH_PLUS && tile_type <= TILE_SWITCH_MINUS)) {
                enemy->state = 1;
                enemy->death_counter = 0;
            }
        } else {
            if(tile_type == TILE_BLOCK_NORMAL || 
               tile_type == TILE_BATTERY_BLOCK || 
               tile_type == TILE_SUPERCONDUCTOR_BLOCK ||
               tile_type == TILE_SWITCH_PLUS ||
               tile_type == TILE_SWITCH_MINUS) {
                enemy->state = 1;
                enemy->death_counter = 0;
                continue;
            }
        }
        if(state == STATE_PUSHING && push_block_id > 0) {
            u16 block_pixel_x, block_pixel_y;
            switch(dir) {
                case 0: 
                    block_pixel_x = push_block_x * 16;
                    block_pixel_y = push_block_y * 16 + (push_step_count + 1) * 2;
                    break;
                case 1: 
                    block_pixel_x = push_block_x * 16;
                    block_pixel_y = push_block_y * 16 - (push_step_count + 1) * 2;
                    break;
                case 2: 
                    block_pixel_x = push_block_x * 16 - (push_step_count + 1) * 2;
                    block_pixel_y = push_block_y * 16;
                    break;
                case 3: 
                    block_pixel_x = push_block_x * 16 + (push_step_count + 1) * 2;
                    block_pixel_y = push_block_y * 16;
                    break;
                default:
                    continue;
            }
            if(enemy->x < block_pixel_x + 16 && enemy->x + 16 > block_pixel_x &&
               enemy->y < block_pixel_y + 16 && enemy->y + 16 > block_pixel_y) {
                enemy->state = 1; // 死亡状態に移行
                enemy->death_counter = 0;
                next_se = SE_BOMB;
                if(score < MAX_SCORE) {
                    score += 100*(enemy->type + 1);
                    if(score > MAX_SCORE) score = MAX_SCORE;
                }
                ui_update();
                // ステージ30の謎解き: HIE→KOME→AWAの順に倒す
                // type 2=HIE, type 0=KOME, type 1=AWA
                if(current_stage == 30) {
                    if(mystery_flag == 0 && enemy->type == 2) {
                        mystery_flag = 1; // HIEを倒した
                    } else if(mystery_flag == 1 && enemy->type == 0) {
                        mystery_flag = 2; // KOMEを倒した
                    } else if(mystery_flag == 2 && enemy->type == 1) {
                        mystery_flag = 3; // AWAを倒した
                    }
                }
            }
        }
        // 超伝導ブロックとの衝突判定
        for(u8 j = 0; j < MAX_SUPERCONDUCTOR_BLOCKS; j++) {
            if(superconductor_blocks[j].active && 
               SW_Sprite_IsVisible(superconductor_blocks[j].sprite_id)) {
                u8 sprite_x = superconductor_blocks[j].x;
                u8 sprite_y = superconductor_blocks[j].y;
                if(enemy->x < sprite_x + 16 && enemy->x + 16 > sprite_x &&
                   enemy->y < sprite_y + 16 && enemy->y + 16 > sprite_y) {
                    enemy->state = 1;
                    enemy->death_counter = 0;
                    next_se = SE_BOMB;
                    if(score < MAX_SCORE) {
                        score += 150*(enemy->type + 1);
                        if(score > MAX_SCORE) score = MAX_SCORE;
                    }
                    ui_update();
                    // ステージ30の謎解き: HIE→KOME→AWAの順に倒す
                    if(current_stage == 30) {
                        if(mystery_flag == 0 && enemy->type == 2) {
                            mystery_flag = 1; // HIEを倒した
                        } else if(mystery_flag == 1 && enemy->type == 0) {
                            mystery_flag = 2; // KOMEを倒した
                        } else if(mystery_flag == 2 && enemy->type == 1) {
                            mystery_flag = 3; // AWAを倒した
                        }
                    }
                }
            }
        }
    }
}

// void chk_blk_col2(Enemy* enemy) {
//     u8 enemy_tile_x = enemy->x / 16;
//     u8 enemy_tile_y = enemy->y / 16;
//     u8 tile_type = g_TileMap[enemy_tile_x + enemy_tile_y * 16];
//     if(tile_type == TILE_BLOCK_NORMAL || 
//        tile_type == TILE_BATTERY_BLOCK || 
//        tile_type == TILE_SUPERCONDUCTOR_BLOCK ||
//        (tile_type >= TILE_SWITCH_PLUS && tile_type <= TILE_SWITCH_MINUS)) {
//         enemy->state = 1;
//         enemy->death_counter = 0;
//         return;
//     }
//     if(state == STATE_PUSHING && push_block_id > 0) {
//         u16 block_pixel_x, block_pixel_y;
//         switch(dir) {
//             case 0: 
//                 block_pixel_x = push_block_x * 16;
//                 block_pixel_y = push_block_y * 16 + (push_step_count + 1) * 2;
//                 break;
//             case 1: 
//                 block_pixel_x = push_block_x * 16;
//                 block_pixel_y = push_block_y * 16 - (push_step_count + 1) * 2;
//                 break;
//             case 2: 
//                 block_pixel_x = push_block_x * 16 - (push_step_count + 1) * 2;
//                 block_pixel_y = push_block_y * 16;
//                 break;
//             case 3: 
//                 block_pixel_x = push_block_x * 16 + (push_step_count + 1) * 2;
//                 block_pixel_y = push_block_y * 16;
//                 break;
//             default:
//                 return;
//         }
//         if(enemy->x < block_pixel_x + 16 && enemy->x + 16 > block_pixel_x &&
//            enemy->y < block_pixel_y + 16 && enemy->y + 16 > block_pixel_y) {
//             enemy->state = 1; // 死亡状態に移行
//             enemy->death_counter = 0;
//         }
//     }
//     // 超伝導ブロックとの衝突判定
//     for(u8 j = 0; j < MAX_SUPERCONDUCTOR_BLOCKS; j++) {
//         if(superconductor_blocks[j].active && 
//            SW_Sprite_IsVisible(superconductor_blocks[j].sprite_id)) {
//             u8 sprite_x = superconductor_blocks[j].x;
//             u8 sprite_y = superconductor_blocks[j].y;
//             if(enemy->x < sprite_x + 16 && enemy->x + 16 > sprite_x &&
//                enemy->y < sprite_y + 16 && enemy->y + 16 > sprite_y) {
//                 enemy->state = 1;
//                 enemy->death_counter = 0;
//             }
//         }
//     }
// }
void upd_death(Enemy* enemy) {
    UD(enemy);
}
void rsp_ene(Enemy* enemy) {
    if (enemy->type == 2) {
        enemy->tile_x = enemy->guard_center_x;
        enemy->tile_y = enemy->guard_center_y;
        enemy->x = enemy->tile_x * 16;
        enemy->y = enemy->tile_y * 16;
        enemy->direction = 1; 
        enemy->state = 0;
        enemy->death_counter = 0;
        enemy->active = 1;
        enemy->anim_counter = 0;
        enemy->speed_counter = 0;
        enemy->stun_counter = 0;
        enemy->blink_counter = 0;
        u8 pattern = get_enemy_pattern(enemy);
        SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
        SW_Sprite_SetVisible(enemy->sprite_id, 1);
        return;
    }
    u8 empty_tiles[200]; 
    u8 empty_count = 0;
    u8 mugi_tile_x = mugi_x / 16;
    u8 mugi_tile_y = mugi_y / 16;
    for(u8 y = 2; y < 12; y++) {
        for(u8 x = 1; x < 15; x++) {
            u8 tile_type = g_TileMap[x + y * 16];
            if(tile_type == TILE_EMPTY) {
                u8 distance_x = (x > mugi_tile_x) ? (x - mugi_tile_x) : (mugi_tile_x - x);
                u8 distance_y = (y > mugi_tile_y) ? (y - mugi_tile_y) : (mugi_tile_y - y);
                if(distance_x >= 2 || distance_y >= 2) {
                    if(empty_count < 200) {
                        empty_tiles[empty_count++] = x + y * 16;
                    }
                }
            }
        }
    }
    if(empty_count > 0) {
        u16 unique_seed = g_Frame + (enemy->sprite_id * 37) + (enemy->x * 7) + (enemy->y * 11);
        Math_SetRandomSeed16(unique_seed);
        u8 random_index = Math_GetRandomMax8(empty_count);
        u8 selected_tile = empty_tiles[random_index];
        enemy->tile_x = selected_tile % 16;
        enemy->tile_y = selected_tile / 16;
        enemy->x = enemy->tile_x * 16;
        enemy->y = enemy->tile_y * 16;
        enemy->direction = Math_GetRandomMax8(4);
        enemy->state = 0;
        enemy->death_counter = 0;
        enemy->active = 1;
        enemy->anim_counter = 0;  
        enemy->speed_counter = 0;
        enemy->stun_counter = 0;
        enemy->blink_counter = 0;
        u8 pattern = get_enemy_pattern(enemy);
        SW_Sprite_Set(enemy->sprite_id, enemy->x, enemy->y, pattern);
        SW_Sprite_SetVisible(enemy->sprite_id, 1); 
    } else {
    }
}
void update_enemies() {
    chk_blk_col();
    for(u8 i = 0; i < g_enemy_count; i++) {
        Enemy* enemy = &g_enemies[i];
        if (!enemy->active) continue;
        //chk_blk_col2(enemy);
        if(enemy->state == 1) {
            upd_death(enemy);
            continue;
        } else if(enemy->state == 2) {
            enemy->death_counter++;
            if(enemy->death_counter > 120) { 
                rsp_ene(enemy);
            }
            continue;
        }
        if(enemy->state == 0) {
            switch(enemy->type) {
                case 0:
                    upd_patrol(enemy);
                    break;
                case 1:
                    upd_chase(enemy);
                    break;
                case 2:
                    upd_guard(enemy);
                    break;
            }
        }
    }
}
u8 find_available_block_sprite() {
    for(u8 id = 3; id <= 6; ++id) {
        if(!SW_Sprite_IsVisible(id)) {
            return id;
        }
    }
    return 3; 
}


void clear_tile_at_position(u8 tile_x, u8 tile_y) {
    g_TileMap[tile_x + tile_y * 16] = TILE_EMPTY;
    Tile_SetDrawPage(2);
    Tile_SelectBank(0);
    Tile_DrawTile(tile_x, tile_y, TILE_FLOOR);
}
void start_block_sprite_and_clear(u8 sprite_id, u8 tile_x, u8 tile_y, u8 block_type) {
    u8 sprite_pattern = get_block_sprite_pattern(block_type);
    SW_Sprite_Set(sprite_id, tile_x * 16, tile_y * 16, sprite_pattern);
    SW_Sprite_SetVisible(sprite_id, 1);
    g_TileMap[tile_x + tile_y * 16] = TILE_EMPTY;
    Tile_SetDrawPage(2);
    Tile_SelectBank(0);
    //Tile_DrawTile(tile_x, tile_y, (current_stage % 5 == 0) ? TILE_EMPTY : TILE_FLOOR);
    if(current_stage % 5 == 0) {
        Copy_MysteryBgTile(tile_x, tile_y);
    } else {
        Tile_DrawTile(tile_x, tile_y, TILE_FLOOR);
    }

    SW_Sprite_SaveCache(sprite_id);
    next_se = SE_SET;
}
void draw_block_at_position(u8 tile_x, u8 tile_y, u8 block_type) {
    g_TileMap[tile_x + tile_y * 16] = block_type;
    Tile_SetDrawPage(2);
    Tile_SelectBank(0);
    Tile_DrawTile(tile_x, tile_y, block_type);
}
void place_battery_block(u8 tile_x, u8 tile_y, u8 block_type) {
    if(block_type == TILE_BATTERY_BLOCK) {
        u8 target_tile = g_TileMap[tile_x + tile_y * 16];
        if(target_tile == TILE_BATTERY_BOX) {
            batteries_placed++;
            block_type = TILE_BATTERY_PLACED;
            next_se = SE_BATTERY;
            if(batteries_placed >= battery_boxes_remaining) {
                open_door();
            }
        }
    }
    if(is_switch_block(block_type)) {
        next_se = SE_BATTERY;
        if(block_type == TILE_SWITCH_PLUS) {
            block_type = TILE_SWITCH_MINUS;  
        } else if(block_type == TILE_SWITCH_MINUS) {
            block_type = TILE_SWITCH_PLUS;   
        }
    }
    g_TileMap[tile_x + tile_y * 16] = block_type;
    Tile_SetDrawPage(2);
    Tile_SelectBank(0);
    Tile_DrawTile(tile_x, tile_y, block_type);
    VDP_CommandHMMM(tile_x * 16, tile_y * 16 + 2 * 256, 
                    tile_x * 16, tile_y * 16, 16, 16);
}
static inline u8 is_walkable_tile(u8 tile_type) {
	return (tile_type == TILE_EMPTY || 
	        tile_type == TILE_BATTERY_BOX || 
	        tile_type == TILE_FLOOR || 
            tile_type == TILE_DOOR_OPEN ||
	        is_arrow_floor(tile_type));  
}
static inline u8 can_block_move_to(u8 block_type, u8 target_tile) {
    if(target_tile == TILE_EMPTY || target_tile == TILE_DOOR_OPEN) {
        return 1;
    }
    // 超電導ブロックは矢印床と穴(TILE_HOLE)を通過可能
    if(block_type == TILE_SUPERCONDUCTOR_BLOCK) {
        if(target_tile >= TILE_ARROW_UP && target_tile <= TILE_ARROW_LEFT) {
            return 1;
        }
        if(target_tile == TILE_HOLE) {
            return 1;
        }
    }
    if(block_type == TILE_BATTERY_BLOCK && target_tile == TILE_BATTERY_BOX) {
        return 1;
    }
    return 0;
}
void Mugi_move(){
	u8 up,down,left,right,center;
	i8 next_tile_x, next_tile_y; 
	u8 beyond_tile;
    // エネルギー弾発射
	if(IsButtonPressed()) {
        if(energy > 0 && (mugi_x % 16 == 0) && (mugi_y % 16 == 0)) {
            fire_energy_bullet();
            next_se = SE_ENERGY;
        }
	}
    // F5: デバッグ用スコア加算機能
    if(Keyboard_IsKeyPressed(KEY_F5)) {
        if(score < MAX_SCORE) {
            score += 1000;
            if(score > MAX_SCORE) score = MAX_SCORE;
        }
        ui_update();
    }
    
    // ポーズ
    if(Keyboard_IsKeyPressed(KEY_F1)&&pause==0) {
        state = STATE_PAUSE;
        pause = 30;
        next_se = SE_PAUSE;
        next_bgm = BGM_STOP;
        // ガイドを描画
        SW_Sprite_COPY();
        ui_update();
        pause_draw_guide();
        return;
    }
	Tile_SetDrawPage(2);
	up = g_TileMap[mugi_x/16 + ((mugi_y+8)/16-1)*16];
	down = g_TileMap[mugi_x/16 + (mugi_y/16+1)*16];
	left = g_TileMap[(mugi_x+8)/16-1 + (mugi_y/16)*16];
	right = g_TileMap[mugi_x/16+1 + (mugi_y/16)*16];
	center = g_TileMap[mugi_x/16 + (mugi_y/16)*16];	
	
	// ステージ30の謎解き: mystery_flag==3のとき、閉じた扉に触れようとすると謎が解ける
	if(current_stage == 30 && mystery_flag == 3) {
		u8 target_tile = 0;
		if(IsLeftPressed()) target_tile = left;
		else if(IsRightPressed()) target_tile = right;
		else if(IsUpPressed()) target_tile = up;
		else if(IsDownPressed()) target_tile = down;
		
		if(is_closed_door(target_tile)) {
			mystery_flag = 4;
		}
	}
	
	if(IsLeftPressed()){
		if(is_walkable_tile(left)){
			state = STATE_MOVING;
		} else if(is_pushable_block(left)) { 
			u8 current_block_type = g_TileMap[(mugi_x/16) - 1 + mugi_y/16 * 16];
			if(current_block_type == TILE_SUPERCONDUCTOR_BLOCK) {
				next_tile_x = (mugi_x/16) - 2;
				next_tile_y = mugi_y/16;
				if(next_tile_x >= 0) {
					beyond_tile = g_TileMap[next_tile_x + next_tile_y * 16];
					if(can_block_move_to(current_block_type, beyond_tile)) { 
                        next_se = SE_SUPER;
						activate_superconductor_block((mugi_x/16) - 1, mugi_y/16, 2); 
						state = STATE_MOVING; 
					}
				}
			} else {
				next_tile_x = (mugi_x/16) - 2;
				next_tile_y = mugi_y/16;
				if(next_tile_x >= 0) {
					beyond_tile = g_TileMap[next_tile_x + next_tile_y * 16];
					if(can_block_move_to(current_block_type, beyond_tile)) { 
						push_block_x = (mugi_x/16) - 1;
						push_block_y = mugi_y/16;
						push_wait_count = 0;
						push_wait_dir = 2; 
						state = STATE_PUSH_WAIT;

					}
				}
			}
		}
		dir = 2;
		return;
	}
	if(IsRightPressed()){
		if(is_walkable_tile(right)){
			state = STATE_MOVING;
		} else if(is_pushable_block(right)) { 
			u8 current_block_type = g_TileMap[(mugi_x/16) + 1 + mugi_y/16 * 16];
			if(current_block_type == TILE_SUPERCONDUCTOR_BLOCK) {
				next_tile_x = (mugi_x/16) + 2;
				next_tile_y = mugi_y/16;
				if(next_tile_x < 16) {
					beyond_tile = g_TileMap[next_tile_x + next_tile_y * 16];
					if(can_block_move_to(current_block_type, beyond_tile)) { 
                        next_se = SE_SUPER;                        
						activate_superconductor_block((mugi_x/16) + 1, mugi_y/16, 3); 
						state = STATE_MOVING; 
					}
				}
			} else {
				next_tile_x = (mugi_x/16) + 2;
				next_tile_y = mugi_y/16;
				if(next_tile_x < 16) {
					beyond_tile = g_TileMap[next_tile_x + next_tile_y * 16];
					if(can_block_move_to(current_block_type, beyond_tile)) { 
						push_block_x = (mugi_x/16) + 1;
						push_block_y = mugi_y/16;
						push_wait_count = 0;
						push_wait_dir = 3; 
						state = STATE_PUSH_WAIT;
					}
				}
			}
		}
		dir = 3;
		return;
	}
	if(IsUpPressed()){
		if(is_walkable_tile(up)){
			state = STATE_MOVING;
		} else if(is_pushable_block(up)) { 
			u8 current_block_type = g_TileMap[mugi_x/16 + ((mugi_y/16) - 1) * 16];
			if(current_block_type == TILE_SUPERCONDUCTOR_BLOCK) {
				next_tile_x = mugi_x/16;
				next_tile_y = (mugi_y/16) - 2;
				if(next_tile_y >= 0) {
					beyond_tile = g_TileMap[next_tile_x + next_tile_y * 16];
					if(can_block_move_to(current_block_type, beyond_tile)) { 
                        next_se = SE_SUPER;                        
						activate_superconductor_block(mugi_x/16, (mugi_y/16) - 1, 1); 
						state = STATE_MOVING; 
					}
				}
			} else {
				next_tile_x = mugi_x/16;
				next_tile_y = (mugi_y/16) - 2;
				if(next_tile_y >= 0) {
					beyond_tile = g_TileMap[next_tile_x + next_tile_y * 16];
					if(can_block_move_to(current_block_type, beyond_tile)) { 
						push_block_x = mugi_x/16;
						push_block_y = (mugi_y/16) - 1;
						push_wait_count = 0;
						push_wait_dir = 1; 
						state = STATE_PUSH_WAIT;
					}
				}
			}
		}
		dir = 1;
		return;
	}
	if(IsDownPressed()){
		if(is_walkable_tile(down)){
			state = STATE_MOVING;
		} else if(is_pushable_block(down)) { 
			u8 current_block_type = g_TileMap[mugi_x/16 + ((mugi_y/16) + 1) * 16];
			if(current_block_type == TILE_SUPERCONDUCTOR_BLOCK) {
				next_tile_x = mugi_x/16;
				next_tile_y = (mugi_y/16) + 2;
				if(next_tile_y < 13) {
					beyond_tile = g_TileMap[next_tile_x + next_tile_y * 16];
					if(can_block_move_to(current_block_type, beyond_tile)) { 
                        next_se = SE_SUPER;
						activate_superconductor_block(mugi_x/16, (mugi_y/16) + 1, 0); 
						state = STATE_MOVING; 
					}
				}
			} else {
				next_tile_x = mugi_x/16;
				next_tile_y = (mugi_y/16) + 2;
				if(next_tile_y < 13) {
					beyond_tile = g_TileMap[next_tile_x + next_tile_y * 16];
					if(can_block_move_to(current_block_type, beyond_tile)) { 
						push_block_x = mugi_x/16;
						push_block_y = (mugi_y/16) + 1;
						push_wait_count = 0;
						push_wait_dir = 0; 
						state = STATE_PUSH_WAIT;
					}
				}
			}
		}
		dir = 0;
		return;
	}
}
void handle_arrow_floor() {
	u8 center = g_TileMap[mugi_x/16 + (mugi_y/16)*16];
	if(is_arrow_floor(center)) {
		switch(center) {
			case TILE_ARROW_UP:    dir = 1; break;
			case TILE_ARROW_DOWN:  dir = 0; break;
			case TILE_ARROW_LEFT:  dir = 2; break;
			case TILE_ARROW_RIGHT: dir = 3; break;
		}
		u8 next_x = mugi_x/16, next_y = mugi_y/16;
		switch(dir) {
			case 0: next_y++; break; 
			case 1: next_y--; break; 
			case 2: next_x--; break; 
			case 3: next_x++; break; 
		}
		if(next_x < 16 && next_y < 13) {
			u8 next_tile = g_TileMap[next_x + next_y * 16];
			if(is_walkable_tile(next_tile)) {
				state = STATE_MOVING;
				return;
			}
		}
		state = STATE_IDLE;
		return;
	}
	state = STATE_IDLE;
}
void Mugi_moving(){
	switch(dir){
		case 0: 
			mugi_y+=4;
			if(mugi_y%16==0){
				handle_arrow_floor();
			}
			break;
		case 1: 
			mugi_y-=4;
			if(mugi_y%16==0){
				handle_arrow_floor();
			}
			break;
		case 2: 
			mugi_x-=4;
			if(mugi_x%16==0){
				handle_arrow_floor();
			}
			break;
		case 3: 
			mugi_x+=4;
			if(mugi_x%16==0){
				handle_arrow_floor();
			}
			break;
	}
}
void Mugi_push_wait(){
	u8 key_pressed = 0;
	switch(push_wait_dir) {
		case 0: 
			key_pressed = IsDownPressed();
			break;
		case 1: 
			key_pressed = IsUpPressed();
			break;
		case 2: 
			key_pressed = IsLeftPressed();
			break;
		case 3: 
			key_pressed = IsRightPressed();
			break;
	}
	u8 other_key_pressed = 0;
	if(push_wait_dir != 0 && IsDownPressed()) other_key_pressed = 1;
	if(push_wait_dir != 1 && IsUpPressed()) other_key_pressed = 1;
	if(push_wait_dir != 2 && IsLeftPressed()) other_key_pressed = 1;
	if(push_wait_dir != 3 && IsRightPressed()) other_key_pressed = 1;
	if(!key_pressed || other_key_pressed) {
		state = STATE_IDLE;
		push_wait_count = 0;
		return;
	}
	push_wait_count++;
	if(push_wait_count >= 8) {
		push_block_id = find_available_block_sprite();
		push_step_count = 0;
		dir = push_wait_dir; 
		push_block_type = g_TileMap[push_block_x + push_block_y * 16];
		start_block_sprite_and_clear(push_block_id, push_block_x, push_block_y, push_block_type);
		state = STATE_PUSHING;
		push_wait_count = 0;

        // Stage 15 限定: バッテリーブロック以外を押したときのみ mystery_flag を立てる
        if(current_stage == 15 && mystery_flag == 0 && push_block_type != TILE_BATTERY_BLOCK) {
            mystery_flag = 1;
        }

	}
}
void Mugi_pushing(){
	u8 sprite_pattern = get_block_sprite_pattern(push_block_type);
	switch(dir){
		case 0: 
			mugi_y += 2;
			SW_Sprite_Set(push_block_id, push_block_x * 16, push_block_y * 16 + (push_step_count + 1) * 2, sprite_pattern);
			break;
		case 1: 
			mugi_y -= 2;
			SW_Sprite_Set(push_block_id, push_block_x * 16, push_block_y * 16 - (push_step_count + 1) * 2, sprite_pattern);
			break;
		case 2: 
			mugi_x -= 2;
			SW_Sprite_Set(push_block_id, push_block_x * 16 - (push_step_count + 1) * 2, push_block_y * 16, sprite_pattern);
			break;
		case 3: 
			mugi_x += 2;
			SW_Sprite_Set(push_block_id, push_block_x * 16 + (push_step_count + 1) * 2, push_block_y * 16, sprite_pattern);
			break;
	}
	push_step_count++;
	if(push_step_count >= 8) {
		u8 dest_tile_x = 0, dest_tile_y = 0; 
		switch(dir){
			case 0: 
				dest_tile_x = push_block_x;
				dest_tile_y = push_block_y + 1;
				break;
			case 1: 
				dest_tile_x = push_block_x;
				dest_tile_y = push_block_y - 1;
				break;
			case 2: 
				dest_tile_x = push_block_x - 1;
				dest_tile_y = push_block_y;
				break;
			case 3: 
				dest_tile_x = push_block_x + 1;
				dest_tile_y = push_block_y;
				break;
		}
		place_battery_block(dest_tile_x, dest_tile_y, push_block_type);
		SW_Sprite_SetVisible(push_block_id, 0);
		SW_Sprite_RestoreCache(push_block_id); 
		SW_Sprite_Set(push_block_id, 0, 0, 0); 
		SW_Sprite_SaveCache(push_block_id); 
        SW_Sprite_Clear(push_block_id);
		state = STATE_IDLE;
		push_step_count = 0;
	}
}

void Load_Picdata(u8 num){
    (void)num;
    // SDL2版: VDP_LoadPicdata() で assets/picdata.bmp をバンク1に読み込み済み
    Tile_SelectBank(1);
}





void Game_Init(){

    //u8 freq_flag = *(u8*)0xFCA1;
 
	//Bios_SetKeyClick(FALSE);
	VDP_SetMode(VDP_MODE_SCREEN5);
	VDP_SetColor(COLOR_BLACK);    
    VDP_EnableDisplay(FALSE);
    VDP_ClearVRAM();    


    VDP_EnableVBlank(TRUE);
	//VDP_SetFrequency(VDP_FREQ_50HZ);
	VDP_DisableSprite();

	//VDP_ClearVRAM();
	Math_SetRandomSeed8((u8)g_Frame + 123);    



    Load_Picdata(1);//画像データ読み込み

    Tile_SetDrawPage(0);
	Tile_FillScreen(COLOR_BLACK);
    VDP_EnableDisplay(TRUE);    
    S_Print_Init(1, 0, 64);//SimplePrint初期化
    // SDL2版: SFXはAudio_Init()で初期化済み
    current_stage = 1;

}

void Sprite_ini(){
	SW_Sprite_Init();
    SW_SpritePT_Init();
	for(u8 i=0;i<16;i++){
		SW_SetSpritePT(i,1,i*16,16*2,16,16);
	}
	for(u8 i=16; i<24; i++){
		u8 x_offset = ((i-16) % 8) * 16;  
		SW_SetSpritePT(i, 1, x_offset, 48, 16, 16);  
	}
	for(u8 i=24; i<32; i++){
		u8 x_offset = ((i-24) % 8) * 16;  
		SW_SetSpritePT(i, 1, x_offset, 16, 16, 16);  
	}
	for(u8 i=32; i<40; i++){
		u8 x_offset = 8*16 + ((i-32) % 8) * 16;
		SW_SetSpritePT(i, 1, x_offset, 48, 16, 16);
	}
	SW_SetSpritePT(SPRITE_BLOCK_NORMAL,1,2*16,0,16,16);          
	SW_SetSpritePT(SPRITE_BLOCK_SWITCH_PLUS,1,9*16,0,16,16);     
	SW_SetSpritePT(SPRITE_BLOCK_SWITCH_MINUS,1,10*16,0,16,16);   
	SW_SetSpritePT(SPRITE_BLOCK_BATTERY,1,11*16,0,16,16);        
	SW_SetSpritePT(44,1,13*16,1*16,16,16); 
	SW_SetSpritePT(45,1,8*16,16*1,16,16);        
	SW_SetSpritePT(46,1,9*16,16*1,16,16);        
	SW_SetSpritePT(47,1,10*16,16*1,16,16);          
	SW_SetSpritePT(48,1,11*16,16*1,16,16);          

}

void ui_update(){
    // SDL2版: UI はpage0(表示バッファ)に直接描画
    if (keyword_visible) {
        u8 no = current_stage / 5;
        S_Print_Text(0,6,2,"KEYWORD   IS ; ;");
        S_Print_Int(0,14,2,no);
        S_Print_Char(0,20,2,keyword[no - 1]);
        S_Print_Text(0,9,3,"BONUS 3000 POINTS!");
    }
    S_Print_Text(0,0,0,"SCORE");  
    S_Print_Int_Padded32(0,7,0,score,6,'0');

    S_Print_Text(0,15,0,"TIME");  
    S_Print_Int_Padded(0,20,0,time,4,'0');

    S_Print_Text(0,15,1,"STAGE");  
    S_Print_Int_Padded(0,22,1,current_stage,2,'0');

    S_Print_Text(0,0,1,"ENERGY");
    S_Print_Int_Padded(0,10,1,energy,3,'0');
}

// PAUSE中のガイド表示（画面中央）
void pause_draw_guide(void) {
    S_Print_Text(0, 11,  6, "- PAUSE -");
    S_Print_Text(0, 10,  8, "F1   RESUME");
    S_Print_Text(0, 10,  9, "F3   SETTING");
    S_Print_Text(0, 10, 10, "ESC  GAME END");
}

void Stage_start(){
    // SDL2版: ルックアップテーブルで直接参照
    const u8* g_stage_data    = g_stage_map_table[current_stage];
    const u8* g_stage_mugi    = g_stage_mugi_table[current_stage];
    const u8* g_stage_enemies = g_stage_enemies_table[current_stage];
    (void)g_stage_data; // memcpyはこの後のPletter置き換えで使用済み
    if(0) switch(current_stage) {  // 以下のcaseは無効化
        case 1:
            g_stage_data = g_stage1;
            g_stage_mugi = g_stage1_mugi;
            g_stage_enemies = g_stage1_enemies;
            break;
        case 2:
            g_stage_data = g_stage2;
            g_stage_mugi = g_stage2_mugi;
            g_stage_enemies = g_stage2_enemies;
            break;
        case 3:
            g_stage_data = g_stage3;
            g_stage_mugi = g_stage3_mugi;
            g_stage_enemies = g_stage3_enemies;
            break;
        case 4:
            g_stage_data = g_stage4;            
            g_stage_mugi = g_stage4_mugi;
            g_stage_enemies = g_stage4_enemies;
            break;
        case 5:
            g_stage_data = g_stage5;
            g_stage_mugi = g_stage5_mugi;
            g_stage_enemies = g_stage5_enemies;
            break;  
        case 6:
            g_stage_data = g_stage6;
            g_stage_mugi = g_stage6_mugi;
            g_stage_enemies = g_stage6_enemies;
            break;
        case 7:
            g_stage_data = g_stage7;
            g_stage_mugi = g_stage7_mugi;
            g_stage_enemies = g_stage7_enemies;
            break;            
        case 8:
            g_stage_data = g_stage8;
            g_stage_mugi = g_stage8_mugi;
            g_stage_enemies = g_stage8_enemies;
            break;
        case 9:
            g_stage_data = g_stage9;
            g_stage_mugi = g_stage9_mugi;
            g_stage_enemies = g_stage9_enemies;
            break;
        case 10:
            g_stage_data = g_stage10;
            g_stage_mugi = g_stage10_mugi;
            g_stage_enemies = g_stage10_enemies;
            break;
        case 11:
            g_stage_data = g_stage11;
            g_stage_mugi = g_stage11_mugi;
            g_stage_enemies = g_stage11_enemies;
            break;
        case 12:
            g_stage_data = g_stage12;            
            g_stage_mugi = g_stage12_mugi;
            g_stage_enemies = g_stage12_enemies;
            break;
        case 13:
            g_stage_data = g_stage13;
            g_stage_mugi = g_stage13_mugi;
            g_stage_enemies = g_stage13_enemies;
            break;
        case 14:
            g_stage_data = g_stage14;
            g_stage_mugi = g_stage14_mugi;
            g_stage_enemies = g_stage14_enemies;
            break;
        case 15:
            g_stage_data = g_stage15;
            g_stage_mugi = g_stage15_mugi;
            g_stage_enemies = g_stage15_enemies;
            break;
        case 16:
            g_stage_data = g_stage16;
            g_stage_mugi = g_stage16_mugi;
            g_stage_enemies = g_stage16_enemies;
            break;
        case 17:
            g_stage_data = g_stage17;
            g_stage_mugi = g_stage17_mugi;
            g_stage_enemies = g_stage17_enemies;
            break;
        case 18:
            g_stage_data = g_stage18;
            g_stage_mugi = g_stage18_mugi;
            g_stage_enemies = g_stage18_enemies;
            break;
        case 19:
            g_stage_data = g_stage19;
            g_stage_mugi = g_stage19_mugi;
            g_stage_enemies = g_stage19_enemies;
            break;
        case 20:
            g_stage_data = g_stage20;
            g_stage_mugi = g_stage20_mugi;
            g_stage_enemies = g_stage20_enemies;
            break;
        case 21:
            g_stage_data = g_stage21;
            g_stage_mugi = g_stage21_mugi;
            g_stage_enemies = g_stage21_enemies;
            break;
        case 22:
            g_stage_data = g_stage22;
            g_stage_mugi = g_stage22_mugi;
            g_stage_enemies = g_stage22_enemies;
            break;
        case 23:
            g_stage_data = g_stage23;
            g_stage_mugi = g_stage23_mugi;
            g_stage_enemies = g_stage23_enemies;
            break;
        case 24:
            g_stage_data = g_stage24;
            g_stage_mugi = g_stage24_mugi;
            g_stage_enemies = g_stage24_enemies;
            break;
        case 25:
            g_stage_data = g_stage25;
            g_stage_mugi = g_stage25_mugi;
            g_stage_enemies = g_stage25_enemies;
            break;
        case 26:
            g_stage_data = g_stage26;
            g_stage_mugi = g_stage26_mugi;
            g_stage_enemies = g_stage26_enemies;
            break;
        case 27:
            g_stage_data = g_stage27;
            g_stage_mugi = g_stage27_mugi;
            g_stage_enemies = g_stage27_enemies;
            break;
        case 28:
            g_stage_data = g_stage28;
            g_stage_mugi = g_stage28_mugi;
            g_stage_enemies = g_stage28_enemies;
            break;
        case 29:
            g_stage_data = g_stage29;
            g_stage_mugi = g_stage29_mugi;
            g_stage_enemies = g_stage29_enemies;
            break;
        case 30:
            g_stage_data = g_stage30;
            g_stage_mugi = g_stage30_mugi;
            g_stage_enemies = g_stage30_enemies;
            break;
        default:
            g_stage_data = g_stage1;
            g_stage_mugi = g_stage1_mugi;
            g_stage_enemies = g_stage1_enemies;
            current_stage = 1;
            break;
    }
	init_energy_bullets();
	init_superconductor_blocks();
	initialize_enemies();

    // 背景描画
    Tile_SetDrawPage(2);
	Tile_SelectBank(0);
	Tile_FillScreen(COLOR_BLACK);
    if(current_stage % 5 != 0){
        for(u8 y=2;y<12;y++){
            for(u8 x=1;x<15;x++){
                Tile_DrawTile(x,y,4);		
            }		
        }            
    }else{
        Draw_MysteryBg();
    }
    
    memcpy(g_TileMap, g_stage_map_table[current_stage], 208);
	Tile_DrawScreen(g_TileMap); 

    // バッテリーボックスの数をカウント
    battery_boxes_remaining = 0;
    batteries_placed = 0;
    for(u8 i = 0; i < 16 * 13; i++) {
        if(g_TileMap[i] == TILE_BATTERY_BOX) {
            battery_boxes_remaining++;
        }
    }

    // 扉の位置を特定
    door_tile_x = 0xFF;
    door_tile_y = 0xFF;
    for(u8 y = 0; y < 13; y++) {
        for(u8 x = 0; x < 16; x++) {
            if(g_TileMap[x + y * 16] == TILE_DOOR_CLOSED) {
                g_door.x = x;
                g_door.y = y;
                g_door.is_open = 0;                
                g_door_exists = 1;
                break;
            }
        }
    }
    if(g_door.x == 0xFF || g_door.y == 0xFF) {
        g_door.x = 0;
        g_door.y = 0;
    }

    //　MUGIの残機を表示
    for(i8 i=0;i<Mugi_Lives;i++){
        Tile_DrawTile(16-i,0,32);
    }

    // MUGI初期位置
    mugi_x = g_stage_mugi[0] * 16;
    mugi_y = g_stage_mugi[1] * 16;
    dir = g_stage_mugi[2];
    energy = g_stage_mugi[3];
    
    // スプライトパターン: dir * 2 (右向きか左向きで異なるパターンを表示)
    SW_Sprite_Init();   // 前ステージのキャッシュを完全クリア
    SW_Sprite_Set(0, mugi_x, mugi_y, dir * 2);
    SW_Sprite_SetVisible(0, 1);
    Load_enemies(g_stage_enemies);

    for(u8 i=0;i<SW_SPRITE_MAX;i++)
		SW_Sprite_SaveCache(i);

    time = 5000;
    energy_bonus = 0;
    time_bonus = 0;

    // UI初期化
    ui_update();

    // ページ２からページ０へコピー
	VDP_CommandHMMM(0,256*2 + 16,0,16,256,212 - 16);
}

void Stage_clear_text(u8 page){
    S_Print_Text(page,10,8,"BONUS");
    S_Print_Int_Padded(page,17,8,time - (time % 100),4,'0');                    
    S_Print_Text(page,10,10,"ENERGY");
    S_Print_Int_Padded(page,17,10,energy,2,'0');
    S_Print_Char(page,19,10,'*');
    S_Print_Int_Padded(page,20,10,100,3,'0');
    S_Print_Text(page,10,12,"TOTAL");
    S_Print_Int_Padded(page,17,12,energy_bonus + time_bonus,6,'0');
}

void Game_Stage(){

    for(i8 id = SW_SPRITE_MAX-1; id >= 0; --id)
    SW_Sprite_RestoreCache(id);

    switch(state) {
        case STATE_IDLE:
            Mugi_move();
            break;
        case STATE_MOVING:
            Mugi_moving();
            break;
        case STATE_PUSH_WAIT:
            Mugi_push_wait();
            break;
        case STATE_PUSHING:
            Mugi_pushing();
            break;
        case STATE_DEAD:
            upd_death(&g_MUGI);
            if(g_MUGI.state == 2) {
                Mugi_Lives--;

                if(Mugi_Lives == 0) {
                    // continue_stage更新とパスワード生成
                    if(current_stage > continue_stage) {
                        continue_stage = current_stage;
                    }
                    EncodePassword(current_stage, g_password);
                    
                    if(score > high_score) high_score = score;  // ハイスコア更新
                    last_score = score;  // ランキング用にスコア保存
                    score = 0;  // スコアをクリア
                    current_scene = SCENE_GAMEOVER;
                    state = STATE_IDLE;
                    gameover_timer = 180;
                    break;

                }else{
                    state = STATE_IDLE;
                    stage_display_timer = 45;
                    current_scene = SCENE_STAGENO;
                    Tile_SetDrawPage(0);
                    Tile_SelectBank(0);
                    SW_Sprite_Init();
                    Tile_FillScreen(COLOR_BLACK);
                    break;
                }
            }
            break;
        case STATE_WIN:
            if(win_timer > 0) {
                win_timer--;


            }else{

                current_stage++;
                current_scene = SCENE_STAGENO;
                stage_display_timer = 45;
                state = STATE_IDLE;
                keyword_visible = 0;  // 次ステージへ進んだらリセット
                Tile_SetDrawPage(0);
                Tile_SelectBank(0);
                SW_Sprite_Init();
                Tile_FillScreen(COLOR_BLACK);

            }
            break;
        case STATE_PAUSE:
            if(Keyboard_IsKeyPressed(KEY_F1)) {
                if(pause==0){
                    next_se = SE_PAUSE;
                    next_bgm = backup_bgm;
                    pause = 30;
                    state = STATE_IDLE;
                }
            }
            // F3: SETTING画面へ（戻ったらゲーム継続）
            if(Keyboard_IsKeyPressed(KEY_F3) && pause == 0) {
                Game_Setting();
                // 戻り後に画面を再描画
                Tile_SetDrawPage(0);
                SW_Sprite_COPY();
                ui_update();
                pause_draw_guide();
                pause = 10;
            }
            // ESC: ゲーム終了
            if(Keyboard_IsKeyPressed(KEY_ESC) && pause == 0) {
                Setting_SaveOnExit();
                SDL_Quit();
                exit(0);
            }
            break;
    }
    if(state != STATE_PAUSE) {
        update_energy_bullets();
        update_superconductor_blocks();
        update_enemies();        
    }

    if(state == STATE_IDLE) {
        u8 mugi_tile_x = mugi_x / 16;
        u8 mugi_tile_y = mugi_y / 16;
        for(u8 i=0; i<g_enemy_count; i++) {
            if(chk_mugi_enemy(&g_enemies[i])){
                // STATE_WINの場合、または無敵モードの場合はやられ判定を無効化
                if(state != STATE_WIN && invincible_mode == 0) {
                    state = STATE_DEAD;
                    g_MUGI.sprite_id = 0;
                    g_MUGI.x = mugi_x;
                    g_MUGI.y = mugi_y;
                    g_MUGI.state = 1;
                    g_MUGI.death_counter = 0;
                    next_se = SE_BOMB;
                    next_bgm = BGM_STOP;
                }
                break;
            }
        }
        if(state != STATE_WIN && state != STATE_DEAD && current_scene != SCENE_STAGENO){
            if(mugi_x % 16 ==0 && mugi_y % 16 == 0) {
                if(chk_stg_clr(mugi_tile_x, mugi_tile_y)) {
                    state = STATE_WIN;
                    win_timer = 60;

                    next_bgm = BGM_WIN; //ステージクリアBGM

                    time_bonus = time - (time % 100);
                    energy_bonus = energy * 100;
                    if(score < MAX_SCORE) {
                        score += time_bonus + energy_bonus;
                        if(score > MAX_SCORE) score = MAX_SCORE;
                    }
                    if(score > high_score)high_score = score;
                    Stage_clear_text(0);
                    ui_update();

                }
            }
        }        
    }
    if(state != STATE_PAUSE)pt = ++pt % 4;
    u8 mugi_pattern;    

    if(state == STATE_PUSHING || state == STATE_PUSH_WAIT) {
        u8 anim_dir = (state == STATE_PUSH_WAIT) ? push_wait_dir : dir;
        mugi_pattern = (pt/2 + anim_dir*2) + 8;
    } else {
        mugi_pattern = pt/2 + dir*2;
    }
    
    // GAME OVERシーンではスプライト描画をスキップして、テキストが欠落しないようにする
    if(current_scene != SCENE_GAMEOVER) {
        if((state != STATE_DEAD)&&(current_scene != SCENE_GAMEOVER)) SW_Sprite_Set(0, mugi_x, mugi_y, mugi_pattern);
        draw_energy_bullets();
        for(u8 id = 0; id < SW_SPRITE_MAX; ++id) {
            if(SW_Sprite_IsVisible(id)) {
                SW_Sprite_SaveCache(id);
            }
        }
        SW_Sprite_DrawAll();
        if(win_timer > 0){
            Stage_clear_text(2);
        }    
        SW_Sprite_COPY();
        ui_update(); // SDL2版: スプライトコピー後にUIをpage0に描画
        if(state == STATE_PAUSE) pause_draw_guide(); // PAUSEガイドは毎フレーム上書き
    }
    // if(current_scene == SCENE_STAGE && state == STATE_IDLE && win_timer == 0){ 

    //     S_Print_Int_Padded(2,20,0,time,4,'0');        
    //     S_Print_Int_Padded(0,20,0,time,4,'0');  
    //     //VDP_CommandHMMM(160,256*2,160,0,32,8); //タイマー更新部分のみコピー
    // }


}

// ランダム色テーブル (8色サイクル)
static const u8 s_name_colors[8][3] = {
    {255, 128, 128}, {255, 200,  80}, {255, 255,  80}, {100, 255, 120},
    { 80, 210, 255}, {160, 140, 255}, {255, 100, 220}, {255, 200, 200},
};

// GAME OVER画面
void Game_Over() {
    // gc_phase:
    //  0=初期化, 1=GAME OVER表示待機(ランクイン情報も同時表示),
    //  3=ネームエントリー, 4=ランダム色変化→白固定, 5=完了待機

    if (gc_phase == 0) {
        // 画面は消さず、GAME OVERテキストを重ねる
        name_entry_rank = 0xFF;
        name_entry_pos = 0;
        name_entry_input_cooldown = 0;
        for (u8 i = 0; i < 8; i++) name_entry_buffer[i] = 0;

        i8 rank = Ranking_CheckRank(last_score);
        if (rank >= 0) {
            name_entry_rank = (u8)rank;
            if (rank == 0) next_bgm = BGM_FANFARE;
        }

        S_Print_Text(0, 12, 8, "GAME OVER");
        S_Print_Text(0, 11, 10, "PASS:");
        S_Print_Text(0, 16, 10, g_password);
        gameover_timer = 150;
        gc_phase = 1;
    }

    if (gc_phase == 1) {
        // GAME OVERとランクイン情報を毎フレーム描画
        S_Print_Text(0, 12, 8, "GAME OVER");
        S_Print_Text(0, 11, 10, "PASS:");
        S_Print_Text(0, 16, 10, g_password);
        if (name_entry_rank != 0xFF) {
            if (name_entry_rank == 0) {
                u8 ci = (150 - gameover_timer) % 8;
                S_Set_Font_Color(s_name_colors[ci][0], s_name_colors[ci][1], s_name_colors[ci][2]);
                S_Print_Text(0, 11, 5, "NEW RECORD!");
                S_Reset_Font_Color();
            }
            S_Print_Text(0, 10, 6, "RANK IN! NO.");
            if (name_entry_rank == 0) {
                S_Set_Font_Color(255, 0, 0);
                S_Print_Int(0, 22, 6, (u32)(name_entry_rank + 1));
                S_Reset_Font_Color();
            } else {
                S_Print_Int(0, 22, 6, (u32)(name_entry_rank + 1));
            }
        }

        gameover_timer--;
        if (gameover_timer == 0 || IsButtonPressed() || Keyboard_IsKeyPressed(KEY_ENTER) || Keyboard_IsKeyPressed(KEY_SPACE)) {
            if (name_entry_rank == 0xFF) {
                gameover_timer = 90;
                gc_phase = 5;
            } else {
                next_bgm = (name_entry_rank == 0) ? BGM_TOP : BGM_NAME_ENTRY;  // 1位はtop.ogg
                gc_phase = 3;
            }
        }
        return;
    }

    if (gc_phase == 3) {
        // ネームエントリー: ランキング画面流用、入力行を黄色
        Ranking_Draw_NameEntry(name_entry_rank, name_entry_buffer, name_entry_pos,
                               last_score, current_stage, 255, 255, 0, 0);

        if (name_entry_input_cooldown > 0) {
            name_entry_input_cooldown--;
        } else {
            // A-Z
            for (u8 k = KEY_A; k <= KEY_Z; k++) {
                if (Keyboard_IsKeyPressed((KeyCode)k) && name_entry_pos < 6) {
                    name_entry_buffer[name_entry_pos++] = 'A' + (k - KEY_A);
                    name_entry_input_cooldown = 10;
                    next_se = SE_CURSOR;
                    break;
                }
            }
            // 0-9
            if (name_entry_pos < 6) {
                for (u8 k = KEY_0; k <= KEY_9; k++) {
                    if (Keyboard_IsKeyPressed((KeyCode)k)) {
                        name_entry_buffer[name_entry_pos++] = '0' + (k - KEY_0);
                        name_entry_input_cooldown = 10;
                        next_se = SE_CURSOR;
                        break;
                    }
                }
            }
            // ピリオド
            if (name_entry_pos < 6 && Keyboard_IsKeyPressed(KEY_PERIOD)) {
                name_entry_buffer[name_entry_pos++] = '.';
                name_entry_input_cooldown = 10;
                next_se = SE_CURSOR;
            }
            // BACKSPACE
            else if (Keyboard_IsKeyPressed(KEY_BS) && name_entry_pos > 0) {
                name_entry_buffer[--name_entry_pos] = 0;
                name_entry_input_cooldown = 10;
                next_se = SE_CURSOR;
            }
            // ENTER: 確定(1文字以上)
            else if (Keyboard_IsKeyPressed(KEY_ENTER) && name_entry_pos > 0) {
                Ranking_Insert(name_entry_rank, name_entry_buffer, last_score, current_stage, 1);
                next_se = SE_KEYWORD;
                next_bgm = BGM_STOP;
                gameover_timer = 60;
                gc_phase = 4;
            }
            // ESC: スキップ
            else if (Keyboard_IsKeyPressed(KEY_ESC)) {
                gameover_timer = 60;
                gc_phase = 5;
            }
        }
        return;
    }

    if (gc_phase == 4) {
        // 60fレインボー色変化 → タイトルへ。SPACE/ボタンでスキップ
        u8 elapsed  = 60 - gameover_timer;   // g_Frameは常時リセットされるため相対カウンタを使用
        u8 col_idx  = elapsed % 8;
        u8 hl_r = s_name_colors[col_idx][0];
        u8 hl_g = s_name_colors[col_idx][1];
        u8 hl_b = s_name_colors[col_idx][2];
        Ranking_Draw_NameEntry(name_entry_rank, name_entry_buffer, name_entry_pos,
                               last_score, current_stage, hl_r, hl_g, hl_b, 1);
        gameover_timer--;
        if (gameover_timer == 0) {
            // 白で上書き
            Ranking_Draw_NameEntry(name_entry_rank, name_entry_buffer, name_entry_pos,
                                   last_score, current_stage, 255, 255, 255, 1);
            gameover_timer = 90;
            gc_phase = 5;
        } else if (IsButtonPressed() || Keyboard_IsKeyPressed(KEY_SPACE)) {
            Ranking_Draw_NameEntry(name_entry_rank, name_entry_buffer, name_entry_pos,
                                   last_score, current_stage, 255, 255, 255, 1);
            gameover_timer = 30;
            gc_phase = 5;
        }
        return;
    }

    if (gc_phase == 5) {
        // 完了待機 → タイトルへ (SPACE/ボタンでスキップ可)
        gameover_timer--;
        if (gameover_timer == 0 || IsButtonPressed() || Keyboard_IsKeyPressed(KEY_SPACE)) {
            gc_phase = 0;
            Mugi_Lives = 3;
            next_bgm = BGM_TITLE;
            current_scene = SCENE_TITLE;
        }
        return;
    }
}

//-----------------------------------------------------------------------------
// VBlank interrupt
void vblank_process(void)
{
    g_VBlank = 1;  // SDL2版: 60fps固定のためスキップ処理なし
    if(current_scene == SCENE_ENDING && ending_message_index < 5) {
        ending_timer++;
        
    }
    
    if(current_scene==SCENE_STAGE && state != STATE_WIN && time>0){
        if(state!=STATE_PAUSE)time--;
    }
    if(pause>0){
        pause--;
    }

    if(next_bgm != 0xFF){
        AKG_Play(next_bgm, 0);
        //if(next_se == 0xFF)AKG_InitSFX(0);
        next_bgm = 0xFF;
    }
    if(next_se != 0xFF){
        if(sfx_loaded == 0){
            AKG_InitSFX(0); // SDL2版: アドレス引数は無視
            sfx_loaded = 1;
        }
        AKG_PlaySFX(next_se, 0, 0);
        next_se = 0xFF;
        
    }
    // AKG_Decode不要 (SDL2_mixerが自動処理)    
}

void print_keyword()
{
    u8 no = current_stage / 5;
    keyword_visible = 1;  // フラグを立てる
    S_Print_Text(0,6,2,"KEYWORD   IS ; ;");
    S_Print_Int(0,14,2,no);
    S_Print_Char(0,20,2,keyword[no - 1]);
    S_Print_Text(0,9,3,"BONUS 3000 POINTS!");
    if(score < MAX_SCORE) {
        score += 3000;
        if(score > MAX_SCORE) score = MAX_SCORE;
    }
    next_se = SE_KEYWORD;
    ui_update();
}
//-----------------------------------------------------------------------------
// Wait for V-Blank period
void WaitVBlank(void)
{
    // SDL2版: オリジナルMSX2の50Hz基準に合わせて固定50fps制御
    //
    // オリジナルの仕組み:
    //   - ゲームロジック・BGMテンポ・キー入力は全て50Hz(PAL)基準で設計
    //   - 60Hz(NTSC)環境では VDP_InterruptHandler で6フレームに1回スキップ
    //     → 60 × (5/6) = 50回/秒 に合わせていた
    //
    // SDL2版はモニターのHzに依存しないよう SDL_GetTicks() で20ms毎に制御する
    #define TARGET_FPS  50
    #define FRAME_MS    20u   // 1000ms / 50fps = 20ms

    // 描画
    SDL_SetRenderTarget(g_renderer, NULL);
    // 常に黒でクリアしてからレンダリング（フルスクリーン時の残像防止）
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    if (g_scroll_x == 0) {
        // 通常描画（タイトルのみ）
        SDL_RenderCopy(g_renderer, g_screen[0], NULL, NULL);
    } else {
        // スクロール中 / ランキング表示中:
        // LogicalSizeを一時無効化して実ピクセル座標で描画
        // SDL_RenderSetLogicalSize有効時はLetterboxで左右に黒帯が入る
        // → 実描画領域(rw×rh)とオフセット(ox,oy)を自前で計算する
        SDL_RenderSetLogicalSize(g_renderer, 0, 0);
        int win_w, win_h;
        SDL_GetRendererOutputSize(g_renderer, &win_w, &win_h);

        // ゲーム論理サイズをウィンドウに収めるscaleとオフセット
        float fsx = (float)win_w / GAME_W;
        float fsy = (float)win_h / GAME_H;
        float sc  = (fsx < fsy) ? fsx : fsy;
        int rw = (int)(GAME_W * sc);
        int rh = (int)(GAME_H * sc);
        int ox = (win_w - rw) / 2;
        int oy = (win_h - rh) / 2;

        // g_scroll_x(0〜GAME_W)をrw基準のオフセットに変換
        int off = (int)((float)g_scroll_x / GAME_W * rw);

        // スクロール領域: y=0〜行23まで (192px分)
        // 行24(コピーライト)は固定表示
        #define SCROLL_ROWS     24          // スクロールする行数
        #define SCROLL_PX       (SCROLL_ROWS * 8)  // = 192px

        // スクロール対象のClipRect（実ピクセル座標）
        int scroll_rh = (int)(sc * SCROLL_PX);  // 192論理pxを実ピクセルに変換
        SDL_Rect clip = { ox, oy, rw, scroll_rh };
        SDL_RenderSetClipRect(g_renderer, &clip);

        SDL_Rect src_scroll = { 0, 0, GAME_W, SCROLL_PX };  // テクスチャの上192px
        int dst_scroll_h = scroll_rh;
        SDL_Rect dst_title   = { ox - off,      oy, rw, dst_scroll_h };
        SDL_Rect dst_ranking = { ox + rw - off,  oy, rw, dst_scroll_h };
        if (dst_title.x + rw > 0)
            SDL_RenderCopy(g_renderer, g_screen[0], &src_scroll, &dst_title);
        if (dst_ranking.x < win_w)
            SDL_RenderCopy(g_renderer, g_screen[1], &src_scroll, &dst_ranking);

        SDL_RenderSetClipRect(g_renderer, NULL); // クリップ解除

        // 行24（コピーライト）は固定: タイトル側(g_screen[0])から直接コピー
        int fixed_src_y  = SCROLL_PX;                     // 192px
        int fixed_src_h  = GAME_H - SCROLL_PX;            // 20px
        int fixed_dst_y  = oy + scroll_rh;
        int fixed_dst_h  = (int)(sc * fixed_src_h);
        SDL_Rect src_fixed = { 0, fixed_src_y, GAME_W, fixed_src_h };
        SDL_Rect dst_fixed = { ox, fixed_dst_y, rw, fixed_dst_h };
        SDL_RenderCopy(g_renderer, g_screen[0], &src_fixed, &dst_fixed);

        SDL_RenderSetLogicalSize(g_renderer, GAME_W, GAME_H);
    }

    // CRTスキャンライン（FILTER=CRT時）
    // 内部ピクセル行（256x212）の各行の下端に半透明黒線を引く
    // LogicalSizeが有効なので論理座標(0〜GAME_H-1)で直接描ける
    // 各論理行の「下端」= その行の最後のサブピクセルなので、
    // 論理座標でy=0.0〜0.999...の行に対して y+0.75 相当の位置に引く
    // 実用上は整数論理座標 y に対して線を引くと「行と行の境界」になる
    if (g_setting_filter == 1 || g_setting_filter == 3) {
        SDL_RenderSetLogicalSize(g_renderer, 0, 0);
        int win_w, win_h;
        SDL_GetRendererOutputSize(g_renderer, &win_w, &win_h);

        int cell_h, thickness;
        int is_fullscreen = (g_setting_fullscreen != 0);

        if (!is_fullscreen) {
            // ウィンドウ時: win_h = 212*N なので整数計算で完全一致
            cell_h    = win_h / GAME_H;
            thickness = cell_h / 2;
        } else {
            // フルスクリーン時: 浮動小数で各行独立に計算（わずかなズレは許容）
            cell_h    = 0;  // 未使用
            thickness = -1; // 浮動小数パスへ
        }

        if (!is_fullscreen && thickness >= 1) {
            // ウィンドウ: 完全整数計算
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 77);
            for (int row = 0; row < GAME_H; row++) {
                int scan_top = row * cell_h + (cell_h - thickness);
                for (int t = 0; t < thickness; t++) {
                    SDL_RenderDrawLine(g_renderer, 0, scan_top + t, win_w - 1, scan_top + t);
                }
            }
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
        } else if (is_fullscreen) {
            // フルスクリーン: 各行独立に浮動小数で計算
            float fcell_h    = (float)win_h / (float)GAME_H;
            int   fthickness = (int)(fcell_h / 2.0f);
            if (fthickness >= 1) {
                SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 77);
                for (int row = 0; row < GAME_H; row++) {
                    // 各行の上端・下端を独立して丸める（累積誤差なし）
                    int row_top = (int)((float)row       * fcell_h + 0.5f);
                    int row_bot = (int)((float)(row + 1) * fcell_h + 0.5f);
                    int h       = row_bot - row_top;  // この行の実ピクセル高さ
                    int ft      = h / 2;              // 下半分の太さ
                    if (ft >= 1) {
                        int scan_top = row_bot - ft;
                        for (int t = 0; t < ft; t++) {
                            SDL_RenderDrawLine(g_renderer, 0, scan_top + t, win_w - 1, scan_top + t);
                        }
                    }
                }
                SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
            }
        }
        SDL_RenderSetLogicalSize(g_renderer, GAME_W, GAME_H);
    }

    SDL_RenderPresent(g_renderer);

    // 入力・VDP処理
    Input_Update();
    if (Input_IsQuitRequested()) {
        Setting_SaveOnExit();
        SDL_Quit();
        exit(0);
    }
    vblank_process();
    g_VBlank = 0;
    g_Frame++;

    // 次フレームまで残り時間をSleep
    static Uint32 s_next_ticks = 0;
    Uint32 now = SDL_GetTicks();
    if (s_next_ticks == 0 || s_next_ticks > now + FRAME_MS * 4) {
        s_next_ticks = now; // 初回 or 大幅ズレはリセット
    }
    s_next_ticks += FRAME_MS;
    if (now < s_next_ticks) {
        SDL_Delay(s_next_ticks - now);
    } else if (now > s_next_ticks + FRAME_MS * 3) {
        // 処理落ちが3フレーム以上蓄積したらリセット
        s_next_ticks = SDL_GetTicks();
    }
}

// メインループ

void game_main(void)
{
    
    Game_Init();
    Sprite_ini();
    Setting_Load();
    Ranking_Load();
    VDP_CreateScreenTextures(); // フィルター設定を反映してからテクスチャ生成
    Setting_ApplyWindowScale(g_setting_window_scale);
    Setting_ApplyFullscreen(g_setting_fullscreen);
    Setting_ApplyBGMVolume(g_setting_bgm_vol);
    Setting_ApplySFXVolume(g_setting_sfx_vol);
	u8 count = 0;
    u8 i = 0;
	while(1)
	{
  
        WaitVBlank();
        
        // ジョイスティック読み取り（ポート1）
        joy_state = Joystick_Read(JOY_PORT_1);
   
		if(g_Frame < 2) continue;
		g_Frame = 0;

        switch(current_scene){
            case SCENE_STAGENO:
                display_stage_number();
                break;
            case SCENE_STAGE:
                Game_Stage();
                if(current_scene==SCENE_STAGE){
                    S_Print_Int_Padded(0,20,0,time,4,'0');        
                }
                break;
            case SCENE_GAMEOVER:
                Game_Over();
                break;
            case SCENE_TITLE:
                Game_Title();
                break;
            case SCENE_ENDING:
                Game_Ending();
                break;

        }

        // 謎解きステージの処理
        if(current_stage % 5 == 0 && current_scene == SCENE_STAGE){
            // 謎解きステージ
            switch(current_stage){
                case 5://扉を開けている状態で一番下に行く
                    if(g_door.is_open == 1 && mugi_y == 11*16 && mystery_flag == 0){
                      mystery_flag = 1;  
                      print_keyword();
                    } 
                    break;
                case 10://スイッチマイナスを2個設置して扉を開ける
                    if(g_door.is_open == 1 && mystery_flag == 0){
                        mystery_flag = 1;
                        for(i=0;i<16*13;i++){
                            if(g_TileMap[i] == TILE_SWITCH_MINUS){
                                mystery_flag++;
                            }
                        }
                        if(mystery_flag == 3){
                            print_keyword();
                        }
                    }
                    break;
                case 15://ブロックを押さないで扉を開ける
                    if(g_door.is_open == 1 && mystery_flag == 0){
                        mystery_flag = 9;
                        print_keyword();
                    }
                    break;
                case 20:
                    if(g_door.is_open == 1 && mystery_flag == 1){
                        mystery_flag = 2;
                        print_keyword();
                    
                    }
                    break;
                case 25:
                    if(mugi_x/16 == 11 && mugi_y/16 == 6 && mystery_flag < 254){
                        mystery_flag++;
                        if(mystery_flag == 254){
                            print_keyword();
                            mystery_flag = 255;
                        }
                    }
                    break;
                case 30:
                    // HIE→KOME→AWAの順に倒し、閉じた扉に触れると謎が解ける
                    if(mystery_flag == 4) {
                        print_keyword();
                        mystery_flag = 5;
                    }
                    break;
                default:
                    break;
            }
            
        }
        //　ESCキー自殺処理
        if(Keyboard_IsKeyPressed(KEY_ESC)&& current_scene == SCENE_STAGE && state != STATE_PAUSE && state != STATE_DEAD && state != STATE_WIN){
            state = STATE_DEAD;
            g_MUGI.sprite_id = 0;
            g_MUGI.x = mugi_x;
            g_MUGI.y = mugi_y;
            g_MUGI.state = 1;
            g_MUGI.death_counter = 0;
            next_bgm = BGM_STOP;
            next_se = SE_BOMB;

    	}
    }
}

#include "actpazle_title.c"
#include "actpazle_setting.c"
#include "actpazle_ranking.c"
