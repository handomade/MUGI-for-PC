// パスワード生成・検証機能
#include "msxgl_types.h"

// 外部変数の参照（actpazle.c で定義）
extern u8 g_Frame;
extern u8 max_stages;

// パスワード関連定数
#define PASSWORD_KEY 0x5A3D
const char PASSWORD_ALPHABET[] = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ";  // 34文字 (I,O除外)
#define PASSWORD_BASE 34

// Base34エンコード (24bit -> 6文字)
void EncodePassword(u8 stage, char out[7]) {
    // ステージ番号を6bit, ソルトを8bit, チェックサムを10bitに配置
    u8 s = stage & 0x3F;
    u8 r = (u8)(s * 17 + 0x2B) & 0xFF;  // マスク後のsから固定ソルトを生成
    
    // チェックサム計算
    u16 c = ((s * 73) ^ (r * 17) ^ 0x5A) + (PASSWORD_KEY & 0xFF);
    c ^= (PASSWORD_KEY >> 8);
    c &= 0x3FF;
    
    // 24bitにパック
    u32 P = ((u32)c << 14) | ((u32)r << 6) | s;
    
    // Base34で6文字に変換
    for(i8 i = 5; i >= 0; i--) {
        out[i] = PASSWORD_ALPHABET[P % PASSWORD_BASE];
        P /= PASSWORD_BASE;
    }
    out[6] = '\0';
}


// Base34デコード (6文字 -> ステージ番号)
u8 DecodePassword(const char in[6], u8* out_stage) {
    u32 P = 0;
    
    // Base34を数値に戻す（最上位桁から処理）
    for(u8 i = 0; i < 6; i++) {
        char ch = in[i];
        u8 digit = 0xFF;
        
        // アルファベット内の位置を探す
        for(u8 j = 0; j < PASSWORD_BASE; j++) {
            if(PASSWORD_ALPHABET[j] == ch) {
                digit = j;
                break;
            }
        }
        
        if(digit == 0xFF) return 0;  // 無効文字
        P = P * PASSWORD_BASE + digit;
    }
    
    // 24bitから展開
    u8 s = P & 0x3F;
    u8 r = (P >> 6) & 0xFF;
    u16 c = (P >> 14) & 0x3FF;
    
    // ステージ番号の基本チェック（1以上であることのみ確認）
    if(s < 1) return 0;  // ステージ0は無効
    
    // ソルトが正しく生成されたものか検証
    u8 r_expected = (u8)(s * 17 + 0x2B) & 0xFF;
    if(r != r_expected) return 0;  // ソルト不一致
    
    // チェックサム再計算
    u16 c_calc = ((s * 73) ^ (r * 17) ^ 0x5A) + (PASSWORD_KEY & 0xFF);
    c_calc ^= (PASSWORD_KEY >> 8);
    c_calc &= 0x3FF;
    
    // 検証
    if(c != c_calc) return 0;  // チェックサム不一致
    
    *out_stage = s;
    return 1;  // 成功
}
