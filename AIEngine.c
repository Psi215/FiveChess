/*
AiEngine.c - AI 引擎实现，思路参考了
https://kimlongli.github.io/2016/12/14/%E5%A6%82%E4%BD%95%E8%AE%BE%E8%AE%A1%E4%B8%80%E4%B8%AA%E8%BF%98%E5%8F%AF%E4%BB%A5%E7%9A%84%E4%BA%94%E5%AD%90%E6%A3%8BAI/
加入了一些自己的优化。
本文件包含了：
1. 棋形评分表的初始化与使用，采用Judge中的掩码匹配思路，对相应棋形进行打分
2. 基于掩码的快速评分表索引
3. 增量打分函数
4. 整盘打分函数
5. 走法函数，走法启用了禁手过滤，启用了邻近检测，启用了必胜剪定，有简单的局势评估
6. AlphaBeta 搜索函数，启用了根据系统配置的多线程搜索，此多线程只作用在第一手，也就是根节点(本着我的电脑配置还行，用来加速,其实也想过用gpu加速)
7. 时间控制
8. Zobrist 哈希与置换表的实现
*/

#include "include/rules.h"
#include "include/common.h"
#include "include/board.h"
#include "include/ai.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <windows.h>
/* 根节点线程函数,用于多线程计算每个候选走法*/
/* 定义线程栈大小：4MB，确保 Debug 模式下递归禁手检测不会栈溢出 */
#define THREAD_STACK_SIZE (4 * 1024 * 1024)

#define AI_MAX_MOVES 225

static int AiColor;             // AI执子颜色
static int SearchDepth = 4;     // 搜索深度
static int TimeLimitMs = 14950; // AI 思考时间上限，毫秒
static int MaxThreads = 0;      // 根节点最大并行线程数，0=按CPU核数

/* Zobrist 哈希相关定义 */
#define TT_SIZE (1 << 20) // 置换表大小，约100万条目
#define TT_FLAG_EXACT 0   // 精确值
#define TT_FLAG_LOWER 1   // 下界（Alpha）
#define TT_FLAG_UPPER 2   // 上界（Beta）

/* Zobrist 哈希表，[x][y][颜色]，颜色0=空，1=黑，2=白 */
static uint64_t ZobristTable[LEN + 1][LEN + 1][3];
static uint64_t ZobristSide[2];  // 行棋方 Zobrist Key：0=白，1=黑
static uint64_t CurrentHash = 0; // 当前局面的哈希值

/* 置换表条目结构 */
typedef struct
{
    uint64_t Hash; // 局面哈希值
    int Depth;     // 搜索深度
    int Score;     // 估值分数
    int Flag;      // 边界类型：EXACT/LOWER/UPPER
    int BestX;     // 最佳走法X坐标
    int BestY;     // 最佳走法Y坐标
} TTEntry;

static TTEntry TransTable[TT_SIZE];       // 置换表
#define KEEP_SHALLOW 32                   // 保留的走法数量（浅层）
#define KEEP_DEEP 16                      // 保留的走法数量（深层）
#define DEEP_DEPTH 6                      // 深层阈值
#define MAX(A, B) ((A) > (B) ? (A) : (B)) // 取最大值
#define MIN(A, B) ((A) < (B) ? (A) : (B)) // 取最小值

/* 方向结构体 */
typedef struct
{
    int x;
    int y;
} Dir;

/* 四象 */
static const Dir dir[] =
    {
        {1, 0},
        {0, 1},
        {1, 1},
        {1, -1},
};

/* AI 走法结构体 */
typedef struct
{
    int X;
    int Y;
    int Score; // 得分
} AiMove;

/* 模式掩码结构体 */
typedef struct
{
    uint32_t SelfMask;  // 落子掩码
    uint32_t EmptyMask; // 空位掩码
    int Score;          // 模式得分
} PatternMask;

/* 棋形分值，按长度分组便于滚动窗口匹配 */
static const PatternMask PatternTable5[] = {
    {0b11111, 0b00000, 50000000}, // 11111
    {0b01111, 0b10000, 720},      // 11110
    {0b11110, 0b00001, 720},      // 01111
    {0b11011, 0b00100, 720},      // 11011
    {0b10111, 0b00010, 720},      // 10111
    {0b11101, 0b01000, 720},      // 11101
};

static const PatternMask PatternTable6[] = {
    {0b011110, 0b100001, 200000}, // 011110
    {0b001110, 0b110001, 720},    // 001110
    {0b011100, 0b110001, 720},    // 011100
    {0b011010, 0b101001, 720},    // 011010
    {0b010110, 0b100101, 720},    // 010110
    {0b001100, 0b110011, 120},    // 001100
    {0b001010, 0b101011, 120},    // 001010
    {0b010100, 0b110101, 120},    // 010100
    {0b000100, 0b110111, 20},     // 000100
    {0b001000, 0b111011, 20},     // 001000
};

static int ScoreTable5[1 << 10]; //  (SelfMask << 5) | EmptyMask
static int ScoreTable6[1 << 12]; //   (SelfMask << 6) | EmptyMask

/* 前置声明 */
static int PreMoves(int Board[LEN + 1][LEN + 1], AiMove Moves[AI_MAX_MOVES], int Depth, int SideColor);
static int AlphaBetaWithHash(int Board[LEN + 1][LEN + 1], int Depth, int Alpha, int Beta, int SideColor, int Eval, uint64_t Hash);
static int EvalDeltaForMove(int Board[LEN + 1][LEN + 1], int x, int y, int PlaceColor, int Eval);
static int CompareMove(const void *A, const void *B);

/* 生成评分表，索引为掩码组合，也就是例如01101 10101对应数组索引为0b0110110101即437，用空间换时间，（类哈希？） */
void InitScoreTables(void)
{
    for (size_t i = 0; i < sizeof(PatternTable5) / sizeof(PatternTable5[0]); ++i) // 遍历所有模式
    {
        uint32_t Index = (PatternTable5[i].SelfMask << 5) | PatternTable5[i].EmptyMask; // 生成查找索引
        ScoreTable5[Index] += PatternTable5[i].Score;                                   // 累加分值
    }
    for (size_t i = 0; i < sizeof(PatternTable6) / sizeof(PatternTable6[0]); ++i) // 遍历所有模式
    {
        uint32_t Index = (PatternTable6[i].SelfMask << 6) | PatternTable6[i].EmptyMask; // 生成查找索引
        ScoreTable6[Index] += PatternTable6[i].Score;                                   // 累加分值
    }
}

/* 生成64位伪随机数，用于初始化 Zobrist 表 */
static uint64_t Rand64(void)
{
    uint64_t r = 0;
    for (int i = 0; i < 4; ++i) // 4次15位随机数拼接成60位，再补4位
        r = (r << 15) | (rand() & 0x7FFF);
    r = (r << 4) | (rand() & 0xF);
    return r;
}

/* 初始化 Zobrist 哈希表
使用固定种子保证可重现性
*/
void InitZobrist(void)
{
    srand(20260112); // 固定种子，保证每次运行生成相同的随机数
    for (int x = 1; x <= LEN; ++x)
        for (int y = 1; y <= LEN; ++y)
            for (int c = 0; c < 3; ++c)
                ZobristTable[x][y][c] = Rand64();
    // 初始化行棋方 Key
    ZobristSide[0] = Rand64();                 // 白方行棋
    ZobristSide[1] = Rand64();                 // 黑方行棋
    CurrentHash = 0;                           // 初始化当前哈希值
    memset(TransTable, 0, sizeof(TransTable)); // 清空置换表
}

/* 根据棋盘颜色获取 Zobrist 索引
传入颜色Color，返回对应的索引(0=空，1=黑，2=白)
*/
static inline int ColorToZIndex(int Color)
{
    if (Color == BLK)
        return 1;
    if (Color == WHI)
        return 2;
    return 0;
}

static inline int SideToIndex(int SideColor)
{
    // SideColor 为 WHI 或 BLK；用于 ZobristSide 索引
    return (SideColor == WHI) ? 0 : 1;
}

/* 更新哈希值：异或操作，落子和撤销都用同一函数
传入位置(x,y)，颜色Color
*/
static inline void UpdateHash(int x, int y, int Color)
{
    CurrentHash ^= ZobristTable[x][y][ColorToZIndex(Color)];
}

/* 根据当前棋盘重新计算完整的哈希值
传入棋盘Board，返回该局面的哈希值
*/
static uint64_t ComputeBoardHash(int Board[LEN + 1][LEN + 1], int SideToMove)
{
    uint64_t Hash = 0;
    for (int x = 1; x <= LEN; ++x)
        for (int y = 1; y <= LEN; ++y)
            if (Board[x][y] != 0)
                Hash ^= ZobristTable[x][y][ColorToZIndex(Board[x][y])];
    // 编码行棋方
    Hash ^= ZobristSide[SideToIndex(SideToMove)];
    return Hash;
}

/* 置换表探测：查询当前局面是否已有记录
传入哈希值Hash，深度Depth，Alpha和Beta剪枝值，输出参数OutScore和OutBestX/Y
返回1表示命中且可直接使用，0表示未命中或深度不足
*/
static int TTProbe(uint64_t Hash, int Depth, int Alpha, int Beta, int *OutScore, int *OutBestX, int *OutBestY)
{
    int Index = (int)(Hash % TT_SIZE);   // 计算索引
    TTEntry *Entry = &TransTable[Index]; // 获取条目
    if (Entry->Hash != Hash)             // 哈希不匹配
        return 0;
    if (OutBestX && Entry->BestX > 0) // 返回最佳走法用于走法排序
        *OutBestX = Entry->BestX;
    if (OutBestY && Entry->BestY > 0)
        *OutBestY = Entry->BestY;
    if (Entry->Depth < Depth) // 深度不足，不能直接使用分值
        return 0;
    if (Entry->Flag == TT_FLAG_EXACT) // 精确值
    {
        *OutScore = Entry->Score;
        return 1;
    }
    if (Entry->Flag == TT_FLAG_LOWER && Entry->Score >= Beta) // 下界剪枝
    {
        *OutScore = Entry->Score;
        return 1;
    }
    if (Entry->Flag == TT_FLAG_UPPER && Entry->Score <= Alpha) // 上界剪枝
    {
        *OutScore = Entry->Score;
        return 1;
    }
    return 0; // 无法直接使用
}

/* 置换表存储：记录当前局面的搜索结果
传入哈希值Hash，深度Depth，分值Score，边界类型Flag，最佳走法坐标BestX/Y
*/
static void TTStore(uint64_t Hash, int Depth, int Score, int Flag, int BestX, int BestY)
{
    int Index = (int)(Hash % TT_SIZE);   // 计算索引
    TTEntry *Entry = &TransTable[Index]; // 获取条目
    /* 替换策略：深度优先，同深度覆盖 */
    if (Entry->Hash == 0 || Entry->Depth <= Depth)
    {
        Entry->Hash = Hash;
        Entry->Depth = Depth;
        Entry->Score = Score;
        Entry->Flag = Flag;
        Entry->BestX = BestX;
        Entry->BestY = BestY;
    }
}

/* 清空置换表，在每次新对局开始时调用 */
void ClearTransTable(void)
{
    memset(TransTable, 0, sizeof(TransTable));
    CurrentHash = 0;
}

/* 返回对方颜色
传入己方颜色，返回对方颜色
*/
static inline int OtherColor(int Color)
{
    return (Color == BLK) ? WHI : BLK;
}

// 这种启发太弱了
//  /* 粗略相邻打分：适当考虑邻近与中心，但降低非棋形权重 */
//  static int NeighborScore(int Board[LEN + 1][LEN + 1], int x, int y)
//  {
//      int Score = 0;
//      for (int dx = -1; dx <= 1; ++dx)
//          for (int dy = -1; dy <= 1; ++dy)
//          {
//              if (dx == 0 && dy == 0)
//                  continue;
//              int nx = x + dx;
//              int ny = y + dy;
//              if (nx < 1 || nx > LEN || ny < 1 || ny > LEN)
//                  continue;
//              if (Board[nx][ny] != 0)
//                  Score += 10; /* 邻近子轻量加分，避免压过棋形分 */
//          }
//      /* 越靠近中心越好 */
//      int Center = (LEN + 1) / 2;
//      int Manhattan = abs(Center - x) + abs(Center - y); // 取曼哈顿距离
//      int CenterBias = 9 - Manhattan * 3;                // 中心偏好，权重适中
//      if (CenterBias > 0)
//          Score += CenterBias;
//      return Score;
//  }

/* 对一条线(横/竖/斜)的局部形势打分,掩码匹配，这里忽略长度不足5的窗口，这些窗口永远不会形成有效棋形
传入直线数组Line，长度Len，己方颜色Color，返回该线的分值
*/
static int EvaluateLine(const int *Line, int Len, int Color)
{
    int Score = 0;

    /* 将整条线编码为自方/空位，低位对应起点 */
    uint32_t SelfBits = 0;
    uint32_t EmptyBits = 0;
    for (int k = 0; k < Len; ++k)
    {
        int v = Line[k];
        if (v == Color)
            SelfBits |= (1u << k); // 取自方棋子
        else if (v == 0)
            EmptyBits |= (1u << k); // 取空位
    }

    /* 长度 5 的所有窗口 */
    if (Len >= 5)
    {
        const uint32_t Full5 = (1u << 5) - 1u; // 0b11111
        for (int i = 0; i + 5 <= Len; ++i)     // 遍历所有长度为5的窗口
        {
            uint32_t self5 = (SelfBits >> i) & 0x1Fu;   // 取出窗口内的自方掩码
            uint32_t empty5 = (EmptyBits >> i) & 0x1Fu; // 取出窗口内的空位掩码
            if ((self5 | empty5) == Full5)              // 如果窗口内只有自方棋子和空位，则可进行匹配
            {
                uint32_t Index = (self5 << 5) | empty5; // 生成查找索引
                Score += ScoreTable5[Index];            // 累加分值
            }
        }
    }

    /* 长度 6 的所有窗口 */
    if (Len >= 6)
    {
        const uint32_t Full6 = (1u << 6) - 1u; // 0b111111
        for (int i = 0; i + 6 <= Len; ++i)     // 遍历所有长度为6的窗口
        {
            uint32_t self6 = (SelfBits >> i) & 0x3Fu;   // 取出窗口内的自方掩码
            uint32_t empty6 = (EmptyBits >> i) & 0x3Fu; // 取出窗口内的空位掩码
            if ((self6 | empty6) == Full6)              // 如果窗口内只有自方棋子和空位，则可进行匹配
            {
                uint32_t Index = (self6 << 6) | empty6; // 生成查找索引
                Score += ScoreTable6[Index];            // 累加分值
            }
        }
    }

    return Score;
}

/* 对单方整盘打分：遍历四个方向的所有线段
传入棋盘Board，己方颜色Color，返回该方总分
*/
static int EvaluateOneSide(int Board[LEN + 1][LEN + 1], int Color)
{
    int Buffer[LEN];
    int Score = 0;

    /* 横向 */
    for (int y = 1; y <= LEN; ++y)
    {
        for (int x = 1; x <= LEN; ++x)
            Buffer[x - 1] = Board[x][y];
        Score += EvaluateLine(Buffer, LEN, Color);
    }

    /* 纵向 */
    for (int x = 1; x <= LEN; ++x)
    {
        for (int y = 1; y <= LEN; ++y)
            Buffer[y - 1] = Board[x][y];
        Score += EvaluateLine(Buffer, LEN, Color);
    }

    /* 右斜 (/) */
    for (int k = 2; k <= 2 * LEN; ++k)
    {
        int idx = 0;
        for (int x = 1; x <= LEN; ++x)
        {
            int y = k - x;
            if (y < 1 || y > LEN)
                continue;
            Buffer[idx++] = Board[x][y];
        }
        if (idx >= 5)
            Score += EvaluateLine(Buffer, idx, Color);
    }

    /* 左斜 (\) */
    for (int k = -LEN; k <= LEN; ++k)
    {
        int idx = 0;
        for (int x = 1; x <= LEN; ++x)
        {
            int y = x - k;
            if (y < 1 || y > LEN)
                continue;
            Buffer[idx++] = Board[x][y];
        }
        if (idx >= 5)
            Score += EvaluateLine(Buffer, idx, Color);
    }

    return Score;
}

/* 对单个位置的四象打分,左右各取6个子,太大没好处
传入棋盘Board，位置(x,y)，己方颜色Color，返回该位置四象总分
*/
static int FourdirEval(int Board[LEN + 1][LEN + 1], int x, int y, int Color)
{
    int Score = 0;
    for (int i = 0; i < 4; ++i)
    {
        int line[13];
        int cnt = 0;

        for (int k = -6; k < 0; ++k) // 向负方向最多取 6 个
        {
            int nx = x + k * dir[i].x;
            int ny = y + k * dir[i].y;
            if (nx < 1 || nx > LEN || ny < 1 || ny > LEN)
                continue;
            line[cnt++] = Board[nx][ny];
        }

        line[cnt++] = Board[x][y]; // 自身

        for (int k = 1; k <= 6; ++k) // 向正方向最多取 6 个
        {
            int nx = x + k * dir[i].x;
            int ny = y + k * dir[i].y;
            if (nx < 1 || nx > LEN || ny < 1 || ny > LEN)
                continue;
            line[cnt++] = Board[nx][ny];
        }
        Score += EvaluateLine(line, cnt, Color);
    }
    return Score;
}

/* 计算在 (x,y) 落子后对全局估值的增量（己方-对方），再加上原来的值，不保留落子
传入棋盘Board，位置(x,y)，落子颜色PlaceColor，原估值Eval，返回新的估值
*/
static int EvalDeltaForMove(int Board[LEN + 1][LEN + 1], int x, int y, int PlaceColor, int Eval)
{
    int MyBefore = FourdirEval(Board, x, y, AiColor);              // 落子前己方
    int OppBefore = FourdirEval(Board, x, y, OtherColor(AiColor)); // 落子前对方

    Board[x][y] = PlaceColor;                                     // 模拟落子
    int MyAfter = FourdirEval(Board, x, y, AiColor);              // 落子后己方
    int OppAfter = FourdirEval(Board, x, y, OtherColor(AiColor)); // 落子后对方
    Board[x][y] = 0;                                              // 还原棋盘

    int Delta = (MyAfter - MyBefore) - (OppAfter - OppBefore); // 估值增量
    return Eval + Delta;
}

/* 棋盘总评估：己方分 - 对方分
传入棋盘Board，己方颜色Color，返回估值
*/
static int EvaluateBoard(int Board[LEN + 1][LEN + 1], int Color)
{
    int MyScore = EvaluateOneSide(Board, Color);
    int OppScore = EvaluateOneSide(Board, OtherColor(Color));
    return MyScore - OppScore;
}

/* 检查 (x,y) 周围 Dist 范围内是否有棋子，用于去除远离战场的点
传入棋盘Board，位置(x,y)，距离Dist，返回1有邻居，0无邻居
*/
static int HasNeighbor(int Board[LEN + 1][LEN + 1], int x, int y, int Dist)
{
    for (int dx = -Dist; dx <= Dist; ++dx)
        for (int dy = -Dist; dy <= Dist; ++dy)
        {
            if (dx == 0 && dy == 0)
                continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 1 || nx > LEN || ny < 1 || ny > LEN)
                continue;
            if (Board[nx][ny] != 0)
                return 1;
        }
    return 0;
}

/* 比较函数，用于qsort，按 Score 降序
 */
static int CompareMove(const void *A, const void *B)
{
    const AiMove *MoveA = (const AiMove *)A;
    const AiMove *MoveB = (const AiMove *)B;
    if (MoveA->Score == MoveB->Score)
        return 0;
    return (MoveA->Score < MoveB->Score) ? 1 : -1; // 降序排序，避免减法溢出
}

/* 预生成走法列表，返回走法数量,这里通过模拟落子，结合棋形和禁手进行排序
传入棋盘Board，走法数组Moves，当前深度Depth，行棋方颜色SideColor，返回生成的走法数量
*/
static int PreMoves(int Board[LEN + 1][LEN + 1], AiMove Moves[AI_MAX_MOVES], int Depth, int SideColor)
{
    int i = 1;      // 必胜走法序列
    int HasWin = 0; // 看胜负模式标志
    int Count = 0;  // 已生成走法数
    int livefour = 0;
    if (StepCount == 0) // 第一步直接下中间
    {
        Moves[0].X = (LEN + 1) / 2;
        Moves[0].Y = (LEN + 1) / 2;
        Moves[0].Score = 100000;
        Count = 1;
        return 1;
    }

    for (int x = 1; x <= LEN; ++x) // 遍历棋盘所有点
    {
        for (int y = 1; y <= LEN; ++y)
        {
            livefour = 0;
            if (Board[x][y] == 0) // 空点
            {
                int ResMY = 0, ResOP = 0;        // 判定结果缓存
                int Dist = (Depth <= 4) ? 1 : 2; // 每次循环重置邻居距离，浅层2，深层1
                if (StepCount == 1)
                    Dist = 1;                       // 第二手特殊处理，必须下在邻近位置
                if (HasNeighbor(Board, x, y, Dist)) // 有邻居
                {
                    ResMY = JudgeOnBoard(Board, x, y, SideColor);             // 模拟判定己方
                    ResOP = JudgeOnBoard(Board, x, y, OtherColor(SideColor)); // 模拟判定对方

                    if (SideColor == BLK)
                    {
                        if (ResMY == JUDGE_FORBID_OVERLINE || ResMY == JUDGE_FORBID_FOURFOUR || ResMY == JUDGE_FORBID_THREETHREE)
                            continue; // 黑棋禁手直接舍弃，不参与搜索
                    }
                    // 己方能直接成五，立即返回，不再考虑其他走法**
                    if (ResMY == JUDGE_WIN_BLACK || ResMY == JUDGE_WIN_WHITE)
                    {
                        Moves[0].X = x;
                        Moves[0].Y = y;
                        Moves[0].Score = 100000000; // 最高优先级
                        return 1;                   // 立即返回，不再搜索
                    }
                    // 对方能赢，进入看胜负模式
                    if (ResOP == JUDGE_WIN_BLACK || ResOP == JUDGE_WIN_WHITE)
                    {
                        if (HasWin == 0) // 首次发现对方必胜点
                        {
                            Moves[0].X = x;
                            Moves[0].Y = y;
                            Moves[0].Score = 1000000000; // 必须堵
                            HasWin = 1;
                            i = 1;
                            Count = 1;
                            continue;
                        }
                        else // 已经在看胜负模式，继续记录对方必胜点
                        {
                            if (i < AI_MAX_MOVES && Count < 10)
                            {
                                Moves[i].X = x;
                                Moves[i].Y = y;
                                Moves[i++].Score = 1000000000;
                                Count = i;
                                continue;
                            }
                        }
                    }
                    // 看胜负模式下，跳过非必胜点
                    if (HasWin == 1)
                        continue;
                    // 常规走法评分
                    Moves[Count].X = x;
                    Moves[Count].Y = y;
                    int MoveScore = EvalDeltaForMove(Board, x, y, SideColor, 0);
                    int FinalScore = MoveScore;
                    if (MoveScore > 200000)
                    {
                        livefour = 1;
                        Moves[Count].X = x;
                        Moves[Count].Y = y;
                        Moves[Count].Score = 1000000; // 极高优先级
                    }
                    if (SideColor == AiColor) // 如果是我方走，则兼顾进攻与防守
                    {
                        int OppDelta = EvalDeltaForMove(Board, x, y, OtherColor(AiColor), 0);
                        int Threat = -OppDelta; // 对方在此落子对我们的伤害
                        int DefBonus = 0;       // 防守加分
                        if (Threat >= 4000)     // 近似活四/冲四
                        {
                            DefBonus = Threat * 3;             // 强力堵冲四/活四
                            FinalScore = MoveScore + DefBonus; // 防守优先
                        }
                        else if (Threat >= 700) // 近似活三，提高权重
                        {
                            DefBonus = Threat * 3 + 500;       // 活三优先级再提升，附加常数拉开差距
                            FinalScore = MoveScore + DefBonus; // 防守偏重
                        }
                        else // 势均力敌或占优，偏向进攻
                        {
                            FinalScore = MoveScore * 2; // 进攻加倍，防守减半
                        }
                    }
                    if (livefour == 0)
                    {
                        Moves[Count].Score = (SideColor == AiColor) ? FinalScore : -MoveScore; // 按当前行棋方视角排序
                    }
                    Count++;
                }
            }
        }
    }
    if (Count == 0) // 无可用走法
        return 0;

    qsort(Moves, Count, sizeof(AiMove), CompareMove); // 按分数降序排序

    int Keep = (Depth >= DEEP_DEPTH) ? KEEP_DEEP : KEEP_SHALLOW; // 深层保留更少走法
    if (Count > Keep)
        Count = Keep; // 截断低分走法
    return Count;
}

static clock_t SearchStart;     // 搜索开始时间
static atomic_int OverTime = 0; // 是否因超时提前终止搜索标志,采用原子变量防止多线程污染

/* 检查是否超出时间限制*/
static int TimeOver(void)
{
    clock_t Now = clock(); // 获取当前时钟计数
    long long ElapsedMs = (long long)((Now - SearchStart) * 1000 / CLOCKS_PER_SEC);
    return ElapsedMs >= TimeLimitMs;
}

/* 根节点任务结构，用于并行计算每个候选走法 */
typedef struct
{
    int Board[LEN + 1][LEN + 1];
    AiMove Move;
    int RootEval;
    int Depth;
    int Result;
} RootTask;

/*
 *   Alpha-Beta 搜索：传入棋局、深度、Alpha、Beta剪枝值、执子方、当前估值、当前哈希值,返回估值
  终止条件：
    1) 超时，返回当前估值
    2) 有五子，返回极大/极小值
    3) 达到叶节点或必胜必败，返回估值
  加入置换表优化：
    1) 搜索前查询置换表，若命中且深度足够则直接返回
    2) 搜索后将结果存入置换表
 */
static int AlphaBetaWithHash(int Board[LEN + 1][LEN + 1], int Depth, int Alpha, int Beta, int SideColor, int Eval, uint64_t Hash)
{
    if (atomic_load(&OverTime))
        return Eval;

    if (TimeOver()) // 超时，直接返回当前估值
    {
        atomic_store(&OverTime, 1);
        return Eval;
    }

    if (Depth == 0 || Eval >= 50000 || Eval <= -50000) // 到达叶节点或必胜必败
        return Eval;

    int OrigAlpha = Alpha; // 记录原始 Alpha 用于判断边界类型

    /* 置换表探测 */
    int TTScore = 0;
    int TTBestX = 0, TTBestY = 0;
    if (TTProbe(Hash, Depth, Alpha, Beta, &TTScore, &TTBestX, &TTBestY))
        return TTScore; // 命中且可用，直接返回

    AiMove Moves[AI_MAX_MOVES];                               // 生成走法，进行略微优化
    int MoveCount = PreMoves(Board, Moves, Depth, SideColor); // 生成走法数量
    if (MoveCount == 0)                                       // 无可下子
        return Eval;

    /* 如果置换表有最佳走法，将其提到最前面 */
    if (TTBestX > 0 && TTBestY > 0)
    {
        for (int i = 1; i < MoveCount; ++i) // 从1开始，0已是最高分
        {
            if (Moves[i].X == TTBestX && Moves[i].Y == TTBestY)
            {
                AiMove Tmp = Moves[0];
                Moves[0] = Moves[i];
                Moves[i] = Tmp;
                break;
            }
        }
    }

    int BestScore = (SideColor == AiColor) ? -0x3F3F3F3F : 0x3F3F3F3F; // 极大/极小初值
    int BestX = 0, BestY = 0;                                          // 记录最佳走法坐标

    for (int i = 0; i < MoveCount; ++i) // 遍历所有走法
    {
        int x = Moves[i].X;
        int y = Moves[i].Y;

        /* 边界检查：防止越界访问 */
        if (x < 1 || x > LEN || y < 1 || y > LEN)
            continue;

        int ChildEval = EvalDeltaForMove(Board, x, y, SideColor, Eval); // 增量估值

        Board[x][y] = SideColor;                                                                                                                                          // 落子
        uint64_t ChildHash = Hash ^ ZobristSide[SideToIndex(SideColor)] ^ ZobristTable[x][y][ColorToZIndex(SideColor)] ^ ZobristSide[SideToIndex(OtherColor(SideColor))]; // 增量更新哈希（含行棋方切换）
        int SonScore = AlphaBetaWithHash(Board, Depth - 1, Alpha, Beta, OtherColor(SideColor), ChildEval, ChildHash);                                                     // 递归搜索
        Board[x][y] = 0;                                                                                                                                                  // 撤销落子

        if (SideColor == AiColor) // 极大节点：需要选择最大的分值
        {
            if (SonScore > BestScore)
            {
                BestScore = SonScore;
                BestX = x;
                BestY = y;
            }
            Alpha = MAX(Alpha, SonScore); // 更新 Alpha
            if (Alpha >= Beta)
                break; // 剪枝
        }
        else
        {
            if (SonScore < BestScore)
            {
                BestScore = SonScore;
                BestX = x;
                BestY = y;
            }
            Beta = MIN(Beta, SonScore); // 更新 Beta
            if (Alpha >= Beta)
                break; // 剪枝
        }
    }

    /* 存储到置换表 */
    int Flag;
    if (BestScore <= OrigAlpha)
        Flag = TT_FLAG_UPPER; // 未超过原始 Alpha，是上界
    else if (BestScore >= Beta)
        Flag = TT_FLAG_LOWER; // 产生剪枝，是下界
    else
        Flag = TT_FLAG_EXACT; // 精确值
    TTStore(Hash, Depth, BestScore, Flag, BestX, BestY);

    return BestScore;
}

/* 将棋局复制到out*/
static void CopyBoard(int Out[LEN + 1][LEN + 1])
{
    memcpy(Out, Point, sizeof(int) * (LEN + 1) * (LEN + 1));
}

/* 设定搜索深度 */
void AiSetSearchDepth(int Depth)
{
    SearchDepth = Depth;
}

/* 运行根节点任务,里面封装了AlphaBeta */
static void RunRootTask(RootTask *Task)
{
    int x = Task->Move.X; // 得到这个任务分配的坐标
    int y = Task->Move.Y;

    /* 边界检查：防止越界访问 */
    if (x < 1 || x > LEN || y < 1 || y > LEN)
    {
        Task->Result = -0x3F3F3F3F; // 无效坐标，返回最低分
        return;
    }

    int ChildEval = EvalDeltaForMove(Task->Board, x, y, AiColor, Task->RootEval); // 对每一个根节点进行取增量
    Task->Board[x][y] = AiColor;
    uint64_t Hash = ComputeBoardHash(Task->Board, OtherColor(AiColor)); // 计算落子后的哈希（下一手行棋方）
    int Score = AlphaBetaWithHash(Task->Board, Task->Depth - 1, -0x3F3F3F3F, 0x3F3F3F3F, OtherColor(AiColor), ChildEval, Hash);
    Task->Result = Score;
}

static DWORD WINAPI RootThread(LPVOID Param)
{
    RootTask *Task = (RootTask *)Param;
    RunRootTask(Task);
    return 0;
}

/* 设定根节点并行线程数：0 表示按 CPU 核数自动决定*/
void AiSetThreadCount(int Threads)
{
    if (Threads < 0)
        Threads = 0;
    if (Threads > 64)
        Threads = 64; // Windows WaitForMultipleObjects 上限64
    MaxThreads = Threads;
}

static double LastAITime = -1.0; // 最近一次 AI 思考用时，毫秒

/*  Alpha-Beta 搜索
传入行棋方Side(0=黑，1=白)，输出坐标OutX, OutY，返回1成功找到最佳走法，0无可下子
*/
int AiGetBestMove(int Side, int *OutX, int *OutY)
{
    AiColor = (Side == 0) ? BLK : WHI; // 设置 AI 颜色
    atomic_store(&OverTime, 0);        // 重置超时标志
    SearchStart = clock();             // 记录搜索开始时间
    AiMove Best = {0, 0, 0};           // 初始化最佳解
    AiMove RootMoves[AI_MAX_MOVES];    // 起始走法数组(根走法，多线程只是将根走法分给不同cpu进行计算)
    int Board[LEN + 1][LEN + 1];
    CopyBoard(Board); // 复制当前棋盘

    int RootEval = EvaluateBoard(Board, AiColor); // 当前局面估值

    int RootCount = PreMoves(Board, RootMoves, SearchDepth, AiColor); // 生成根走法

    if (RootCount <= 0) // 无可下子,几乎不可能发生
    {
        LastAITime = (double)(clock() - SearchStart) * 1000.0 / CLOCKS_PER_SEC;
        return 0;
    }
    int BestScore = -0x3F3F3F3F; // 初始化最佳分值,负无穷
    for (int i = 0; i < RootCount; ++i)
        RootMoves[i].Score = -0x3F3F3F3F; // 默认分值，便于禁手替换时按评分选择

    RootTask Tasks[AI_MAX_MOVES];
    HANDLE Handles[AI_MAX_MOVES];
    // 多线程
    int ThreadLimit = MaxThreads; // 线程数限制
    if (ThreadLimit <= 0)
    {
        SYSTEM_INFO SysInfo;
        GetSystemInfo(&SysInfo);                         // 获取系统信息
        ThreadLimit = (int)SysInfo.dwNumberOfProcessors; // 按 CPU 核数决定线程数
    }
    if (ThreadLimit < 1) // 安全保护
        ThreadLimit = 1;
    if (ThreadLimit > 64)
        ThreadLimit = 64; // Windows WaitForMultipleObjects 上限64
    if (ThreadLimit > RootCount)
        ThreadLimit = RootCount;

    for (int start = 0; start < RootCount; start += ThreadLimit) // 根据根节点数量分批次创建线程
    {
        int batch = MIN(ThreadLimit, RootCount - start); // 同一批次任务不超过线程数限制,避免多余线程创建
        for (int b = 0; b < batch; ++b)
        {
            int idx = start + b;                                                                    // 当前任务索引
            CopyBoard(Tasks[idx].Board);                                                            // 复制棋盘进任务
            Tasks[idx].Move = RootMoves[idx];                                                       // 复制走法进任务
            Tasks[idx].RootEval = RootEval;                                                         // 复制估值进任务
            Tasks[idx].Depth = SearchDepth;                                                         // 复制深度进任务
            Handles[idx] = CreateThread(NULL, THREAD_STACK_SIZE, RootThread, &Tasks[idx], 0, NULL); // 创建线程，栈大小4MB防止Debug模式栈溢出
            if (Handles[idx] == NULL)                                                               // 创建线程失败
            {
                RunRootTask(&Tasks[idx]);
            }
        }

        HANDLE WaitList[64];            // 等待当前批次完成
        int WaitCount = 0;              // 等待句柄数量
        for (int b = 0; b < batch; ++b) // 遍历当前批次任务
        {
            int idx = start + b;
            if (Handles[idx])                         // 如果线程创建成功
                WaitList[WaitCount++] = Handles[idx]; // 加入等待列表
        }
        if (WaitCount > 0)                                               // 如果有需要等待的线程
            WaitForMultipleObjects(WaitCount, WaitList, TRUE, INFINITE); // 等待所有线程完成

        for (int b = 0; b < batch; ++b) // 收集当前批次结果
        {
            int idx = start + b;
            if (Handles[idx]) // 如果线程创建成功
            {
                CloseHandle(Handles[idx]); // 关闭线程句柄
                Handles[idx] = NULL;       // 清空句柄
            }
            // 如果线程未创建，则 RunRootTask 已经算完；如果创建了线程，结果已写入
            int Score = Tasks[idx].Result;
            RootMoves[idx].Score = Score;
            if (Score > BestScore) // 更新最佳分值和走法
            {
                BestScore = Score;
                Best.X = Tasks[idx].Move.X;
                Best.Y = Tasks[idx].Move.Y;
            }
        }
        if (atomic_load(&OverTime)) // 检查是否超时
            break;
    }
    if (Best.X <= 0 || Best.Y <= 0) // 未找到最佳解,找到周围有子的第一个空点
    {
        for (int X = 1; X <= LEN; ++X)
            for (int Y = 1; Y <= LEN; ++Y)
                if (Board[X][Y] == 0 && HasNeighbor(Board, X, Y, 1))
                {
                    Best.X = X;
                    Best.Y = Y;
                    break;
                }
    }

    if (OutX)
        *OutX = Best.X;
    if (OutY)
        *OutY = Best.Y;

    // /* 黑棋禁手过滤：事实上，在走法函数处已近过滤，如果最佳点为禁手，尝试选择下一个最佳点 */
    // if (AiColor == BLK) // 黑棋指定
    // {
    //     int Res = JudgeOnBoard(Board, Best.X, Best.Y, AiColor); // 判断禁手
    //     if (Res == JUDGE_FORBID_OVERLINE || Res == JUDGE_FORBID_FOURFOUR || Res == JUDGE_FORBID_THREETHREE)
    //     {
    //         int BestLegalScore = -0x3F3F3F3F; // 初始化合法点分值
    //         int BestLegalX = Best.X;          // 初始化最佳合法点坐标
    //         int BestLegalY = Best.Y;
    //         for (int i = 0; i < RootCount; ++i) // 遍历所有根走法
    //         {
    //             int Rx = RootMoves[i].X;                                                                               // 根走法X坐标
    //             int Ry = RootMoves[i].Y;                                                                               // 根走法Y坐标
    //             int RRes = JudgeOnBoard(Board, Rx, Ry, AiColor);                                                       // 判断该点禁手情况
    //             if (RRes != JUDGE_FORBID_OVERLINE && RRes != JUDGE_FORBID_FOURFOUR && RRes != JUDGE_FORBID_THREETHREE) // 合法点
    //             {
    //                 if (RootMoves[i].Score > BestLegalScore) // 选择分值最高的合法点
    //                 {
    //                     BestLegalScore = RootMoves[i].Score;
    //                     BestLegalX = Rx;
    //                     BestLegalY = Ry;
    //                 }
    //             }
    //         } // 遍历结束后，更新最佳点为最佳合法点
    //         Best.X = BestLegalX;
    //         Best.Y = BestLegalY;
    //         if (OutX) // 写回输出参数
    //             *OutX = Best.X;
    //         if (OutY)
    //             *OutY = Best.Y;
    //     }
    // }

    AiInput(Best.X, Best.Y); // 通知外部 AI 走法

    LastAITime = (double)(clock() - SearchStart) * 1000.0 / CLOCKS_PER_SEC; // 计算思考时间

    return (Best.X > 0 && Best.Y > 0);
}

/* 获取最近一次 AI 思考用时（毫秒），供外部显示 */
double AiGetLastTimeMs(void)
{
    return LastAITime;
}
