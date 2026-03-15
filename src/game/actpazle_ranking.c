// actpazle_ranking.c
// ランキングデータ管理（バイナリ形式 mugi_ranking.dat）
// actpazle.cの末尾でincludeされる

#include <stdio.h>
#include <string.h>

// ────────────────────────────────────────
// 定数
// ────────────────────────────────────────
#define RANKING_MAX     10
#define RANKING_FILE    "mugi_ranking.dat"
#define RANKING_MAGIC   0x4D554749  // "MUGI"
#define RANKING_VERSION 1

// ────────────────────────────────────────
// ランキングエントリ構造体
// ────────────────────────────────────────
typedef struct {
    char name[8];    // プレイヤー名（6文字+'\0'+パディング）
    u32  score;
    u8   stage;      // 1-30
    u8   lap;        // 周回数（1〜）
    u8   _pad[14];   // 予約済み（日付は現在未実装、バイナリ互換性のため保持）
} RankEntry;         // 32bytes固定

// ────────────────────────────────────────
// ファイルヘッダ
// ────────────────────────────────────────
typedef struct {
    u32 magic;
    u8  version;
    u8  count;       // 有効エントリ数
    u8  _pad[2];
} RankHeader;        // 8bytes

// ────────────────────────────────────────
// グローバルランキングデータ
// ────────────────────────────────────────
RankEntry g_ranking[RANKING_MAX];
u8        g_ranking_count = 0;

// ────────────────────────────────────────
// ランキング読み込み
// ────────────────────────────────────────
void Ranking_Load(void) {
    FILE* f = fopen(RANKING_FILE, "rb");
    if (!f) { g_ranking_count = 0; return; }

    RankHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 ||
        hdr.magic != RANKING_MAGIC ||
        hdr.version != RANKING_VERSION) {
        fclose(f);
        g_ranking_count = 0;
        return;
    }
    g_ranking_count = (hdr.count > RANKING_MAX) ? RANKING_MAX : hdr.count;
    fread(g_ranking, sizeof(RankEntry), g_ranking_count, f);
    fclose(f);
}

// ────────────────────────────────────────
// ランキング保存
// ────────────────────────────────────────
void Ranking_Save(void) {
    FILE* f = fopen(RANKING_FILE, "wb");
    if (!f) return;
    RankHeader hdr = { RANKING_MAGIC, RANKING_VERSION, g_ranking_count, {0,0} };
    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(g_ranking, sizeof(RankEntry), g_ranking_count, f);
    fclose(f);
}

// ────────────────────────────────────────
// スコアがランクインするか確認
// 戻り値: 挿入される順位(0始まり) / -1=ランクイン不可
// ────────────────────────────────────────
i8 Ranking_CheckRank(u32 new_score) {
    for (u8 i = 0; i < RANKING_MAX; i++) {
        if (i >= g_ranking_count) {
            // ランキング未満杯: 末尾に追加
            return (i8)i;
        }
        if (new_score > g_ranking[i].score) {
            // スコアランキング内に挿入
            return (i8)i;
        }
    }
    return -1;
}

// ────────────────────────────────────────
// ランキングに新エントリを挿入
// ────────────────────────────────────────
void Ranking_Insert(u8 rank, const char* name, u32 score, u8 stage, u8 lap) {
    // 末尾を押し出す
    u8 new_count = (g_ranking_count < RANKING_MAX) ? g_ranking_count + 1 : RANKING_MAX;
    for (u8 i = new_count - 1; i > rank; i--) {
        g_ranking[i] = g_ranking[i - 1];
    }
    // 新エントリをセット
    RankEntry* e = &g_ranking[rank];
    memset(e, 0, sizeof(RankEntry));
    strncpy(e->name, name, 6);
    e->name[6] = '\0';
    e->score = score;
    e->stage = stage;
    e->lap   = lap;
    // 日付は現在未実装
    g_ranking_count = new_count;
    Ranking_Save();
}

// ────────────────────────────────────────
// ランキング表示（タイトル画面用）
// ────────────────────────────────────────
void Ranking_Draw(void) {
    Tile_SetDrawPage(0);
    Tile_SelectBank(0);
    Tile_FillScreen(COLOR_BLACK);

    // HIGH SCORE: y=3、中央寄せ
    S_Print_Text(0, 11, 3, "HIGH SCORE");

    // ヘッダ: y=5、中央寄せ
    S_Print_Text(0,  4, 5, "RANK");
    S_Print_Text(0,  9, 5, "NAME");
    S_Print_Text(0, 16, 5, "SCORE");
    S_Print_Text(0, 23, 5, "STAGE");

    // データ: y=7〜16（1文字右・1行下）
    for (u8 i = 0; i < RANKING_MAX; i++) {
        u8 y = i + 7;
        if (i < g_ranking_count) {
            RankEntry* e = &g_ranking[i];
            // RANK
            S_Print_Int_Padded(0, 4, y, i + 1, 2, ' ');
            S_Print_Char(0, 6, y, '.');
            // NAME (最大6文字)
            char name_buf[7] = "      ";
            u8 nlen = (u8)strlen(e->name);
            if (nlen > 6) nlen = 6;
            for (u8 c = 0; c < nlen; c++) name_buf[c] = e->name[c];
            S_Print_Text(0, 8, y, name_buf);
            // SCORE
            S_Print_Int_Padded32(0, 15, y, e->score, 6, '0');
            // LAP-STG
            S_Print_Int_Padded(0, 23, y, e->lap,   1, ' ');
            S_Print_Char(0, 24, y, '-');
            S_Print_Int_Padded(0, 25, y, e->stage, 2, '0');
        } else {
            S_Print_Text(0,  4, y, "--");
            S_Print_Text(0,  8, y, "------");
            S_Print_Text(0, 15, y, "------");
            S_Print_Text(0, 23, y, "--");
        }
    }
    // イラスト（行17〜22、中央寄せ）
    Draw_RankingIllust();

    // コピーライトはここでも描画（g_screen[1]に書き込む）
    Ranking_DrawCopyright();
}

// コピーライト行単体描画（スクロール中も固定表示するために分離）
void Ranking_DrawCopyright(void) {
    S_Print_Text(0, 3, 24, "2026 HANDO;S GAME CHANNEL");
}
