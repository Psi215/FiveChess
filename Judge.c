/*
 * 禁手判断以及胜负判断+棋型判断（长连，活三、活四、冲四、五连）。
 * 需要补充的是，本程序的禁手判断思路(递归思路)参考借鉴了github上的dhbloo大神的开源程序rapfi，下面是项目链接。
 * https://github.com/dhbloo/rapfi
 *
 * 注意：所有递归深度现在通过函数参数传递，而非全局变量，以保证多线程安全。
 */

#include "include/rules.h"
#include "include/common.h"
#include <stdint.h>
#include <string.h>
#define CENTER 5 // 位置码的中心位置,也就是x，y

/* 最大递归深度限制，防止栈溢出 */
#define MAX_DEPTH_FOUR 2
#define MAX_DEPTH_THREE 2

/* 前置声明 - 内部递归函数 */
static int IsBannedInternal(const int Board[LEN + 1][LEN + 1], int x, int y, int DepthFour, int DepthThree);
static int ThreeCheckInternal(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor, int DepthThree);
static int FourCheckInternal(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor, int DepthFour);

// 声明方向常量：水平；竖直；右斜；左斜；
const int dx[4] = {1, 0, 1, -1};
const int dy[4] = {0, 1, 1, 1};

// 方向索引：0为水平；1为竖直；2为右斜；3为左斜；
char dir;

/* 棋盘内判断函数：判断棋子是否落在棋盘中 */
int InBoard(int x, int y)
{
    return (x > 0 && x <= LEN && y > 0 && y <= LEN);
}

/*
 * 提取选中坐标选中方向的前后各5个位置，
 * 在其中，己方棋子标注为1，空位标注为0
 * 一共返回两个值
 * 注意二进制数从右往左为0到11
 * SelfMask 记录着己方棋子的分布
 * EmptyMask 记录着空位的分布
 */
static void GetLine(const int Board[LEN + 1][LEN + 1], int x, int y, int dir, int SideColor, unsigned int *SelfMask, unsigned int *EmptyMask)
{
    const int CenterPosition = 5;
    int Color = (SideColor == WHI) ? WHI : BLK; // 获取当前执子方颜色

    unsigned int S = 0, E = 0;

    for (int i = -5; i <= 5; i++)
    {
        int SelectedX = x + dx[dir] * i;
        int SelectedY = y + dy[dir] * i;
        int BitIndex = CenterPosition + i;

        if (InBoard(SelectedX, SelectedY))
        {
            int Po = Board[SelectedX][SelectedY];
            if (Po == Color) // 己方棋子
                S |= (1 << BitIndex);
            else if (Po == 0) // 空位
                E |= (1 << BitIndex);
        }
    }

    *SelfMask = S;
    *EmptyMask = E;
}

/*
 * 判断位置码中是否有四连，五连，长连。
 */
int MaskMatch(unsigned int Mask)
{
    Mask = Mask & (Mask >> 1); // 与右移1位的自己做与运算，保留连续的1
    Mask = Mask & (Mask >> 1);
    Mask = Mask & (Mask >> 1); // 3
    if (Mask == 0)
        return 0;

    Mask = Mask & (Mask >> 1); // 4
    if (Mask == 0)
        return 4;

    Mask = Mask & (Mask >> 1); // long
    if (Mask != 0)
        return 6;

    return 5;
}

/* 五连检测 白棋 */
static int FiveCheck(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor)
{
    unsigned int SelfMask, EmptyMask;
    for (int d = 0; d <= 3; d++)
    {
        GetLine(Board, x, y, d, SideColor, &SelfMask, &EmptyMask);
        if (MaskMatch(SelfMask) >= 5)
            return 1;
    }
    return 0;
}

/* 五连检测 黑棋 黑棋必须精确 */
static int ExactFiveCheck(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor)
{
    unsigned int SelfMask, EmptyMask;
    for (int d = 0; d <= 3; d++)
    {
        GetLine(Board, x, y, d, SideColor, &SelfMask, &EmptyMask);
        if (MaskMatch(SelfMask) == 5)
            return 1;
    }
    return 0;
}

/* 长连检测 */
static int OverCheck(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor)
{
    unsigned int SelfMask, EmptyMask;
    for (int d = 0; d <= 3; d++)
    {
        GetLine(Board, x, y, d, SideColor, &SelfMask, &EmptyMask);
        if (MaskMatch(SelfMask) == 6)
            return 1;
    }
    return 0;
}

/*
 * 单方向活四检测（仅检查位置码模式，不检查禁手）
 */
static int CheckLiveFourPatternOnly(unsigned int SelfMask, unsigned int EmptyMask)
{
    for (int k = 1; k <= 4; k++)
    {
        if (!((EmptyMask >> k) & 1))
            continue;
        if (!((EmptyMask >> (k + 5)) & 1))
            continue;

        unsigned int Pattern = (0xF << (k + 1));
        if ((SelfMask & Pattern) == Pattern)
            return 1;
    }
    return 0;
}

/*
 * 单方向活四检测（带禁手检查）
 * DepthFour: 当前递归深度
 */
static int CheckLiveFourOnOneLineInternal(const int Board[LEN + 1][LEN + 1], int x, int y, int d, unsigned int SelfMask, unsigned int EmptyMask, int DepthFour)
{
    for (int k = 1; k <= 4; k++)
    {
        if (!((EmptyMask >> k) & 1))
            continue;
        if (!((EmptyMask >> (k + 5)) & 1))
            continue;

        unsigned int Pattern = (0xF << (k + 1));
        if ((SelfMask & Pattern) != Pattern)
            continue;

        int idx1 = k;
        int idx2 = k + 5;

        int x1 = x + dx[d] * (idx1 - CENTER);
        int y1 = y + dy[d] * (idx1 - CENTER);
        int x2 = x + dx[d] * (idx2 - CENTER);
        int y2 = y + dy[d] * (idx2 - CENTER);

        if (!InBoard(x1, y1) || !InBoard(x2, y2)) //  边界检查：防止越界访问导致崩溃
            continue;

        if (DepthFour < 2)
        {
            if (!IsBannedInternal(Board, x1, y1, DepthFour + 1, 0) && !IsBannedInternal(Board, x2, y2, DepthFour + 1, 0)) // 两端都不是禁手才算活四
            {
                return 1;
            }
        }
        else
        {
            return 1; // 深度过深，假设是活四
        }
    }
    return 0;
}

/*
 * 四四禁手检测
 */
static int FourCheckInternal(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor, int DepthFour)
{
    unsigned int SelfMask, EmptyMask;
    int Count = 0;

    for (int d = 0; d <= 3; d++)
    {
        GetLine(Board, x, y, d, SideColor, &SelfMask, &EmptyMask);

        int PosCount = 0;

        for (int i = 0; i < 11; i++)
        {
            if (EmptyMask & (1 << i))
            {
                unsigned int SelfMask_S = SelfMask | (1 << i);
                if (MaskMatch(SelfMask_S) == 5)
                {
                    unsigned int SelfMask_SS = (SelfMask_S & (~(1 << CENTER)));
                    if (MaskMatch(SelfMask_SS) < 5)
                    {
                        PosCount++;
                    }
                }
            }
        }
        if (PosCount == 1)
            Count++;
        else if (PosCount >= 2)
        {
            if (CheckLiveFourOnOneLineInternal(Board, x, y, d, SelfMask, EmptyMask, DepthFour))
                Count++;
            else
                Count += PosCount;
        }
    }
    if (Count >= 2)
        return 1;
    return 0;
}

/*
 * 三三禁手检测
 */
static int ThreeCheckInternal(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor, int DepthThree)
{
    int Count = 0;
    unsigned int SelfMask, EmptyMask;

    for (int d = 0; d <= 3; d++)
    {
        GetLine(Board, x, y, d, SideColor, &SelfMask, &EmptyMask);

        if (CheckLiveFourPatternOnly(SelfMask, EmptyMask))
            continue;

        for (int i = 0; i <= 10; i++)
        {
            if (!(EmptyMask & (1 << i)))
                continue;
            unsigned int SelfMask_S = SelfMask | (1 << i);
            unsigned int EmptyMask_S = EmptyMask & (~(1 << i));
            int isRealLiveFour = 0; // 检查模拟落子后有没有形成活四模式，并验证是真活四
            for (int k = 1; k <= 4; k++)
            {
                if (!((EmptyMask_S >> k) & 1))
                    continue;
                if (!((EmptyMask_S >> (k + 5)) & 1))
                    continue;

                unsigned int Pattern = (0xF << (k + 1));
                if ((SelfMask_S & Pattern) != Pattern)
                    continue;
                int idx1 = k; // 找到活四模式，检查两端是否至少有一个不是禁手
                int idx2 = k + 5;
                int x1 = x + dx[d] * (idx1 - CENTER);
                int y1 = y + dy[d] * (idx1 - CENTER);
                int x2 = x + dx[d] * (idx2 - CENTER);
                int y2 = y + dy[d] * (idx2 - CENTER);
                int x_sim = x + dx[d] * (i - CENTER); // 创建临时棋盘，包含模拟落子
                int y_sim = y + dy[d] * (i - CENTER);

                if (!InBoard(x1, y1) || !InBoard(x2, y2) || !InBoard(x_sim, y_sim))
                    continue;

                if (DepthThree < MAX_DEPTH_THREE)
                {
                    int SavedValue = Board[x_sim][y_sim];
                    ((int (*)[LEN + 1]) Board)[x_sim][y_sim] = BLK; // 直接在 Board 上临时落子，检查完后恢复

                    if (!IsBannedInternal(Board, x1, y1, 0, DepthThree + 1) || !IsBannedInternal(Board, x2, y2, 0, DepthThree + 1)) // 两端至少有一个不是禁手才是真活四
                    {
                        isRealLiveFour = 1;
                    }

                    ((int (*)[LEN + 1]) Board)[x_sim][y_sim] = SavedValue; // 恢复
                }
                else
                {
                    isRealLiveFour = 1; // 递归深度限制
                }

                if (isRealLiveFour)
                    break;
            }

            if (isRealLiveFour)
            {
                unsigned int SelfMask_SS = (SelfMask_S & (~(1 << CENTER)));
                unsigned int EmptyMask_SS = (EmptyMask_S | (1 << CENTER));

                if (!CheckLiveFourPatternOnly(SelfMask_SS, EmptyMask_SS))
                {
                    int x_s = x + dx[d] * (i - CENTER);
                    int y_s = y + dy[d] * (i - CENTER);

                    if (!InBoard(x_s, y_s))
                        continue;

                    if (DepthThree < 2)
                    {
                        int banned = IsBannedInternal(Board, x_s, y_s, 0, DepthThree + 1);
                        if (!banned)
                        {
                            Count++;
                            break;
                        }
                    }
                    else
                    {
                        Count++;
                        break;
                    }
                }
            }
        }
    }

    if (Count >= 2)
        return 1;
    return 0;
}

/*
 * 内部禁手检查函数（线程安全版本）
 * DepthFour: 活四递归深度
 * DepthThree: 活三递归深度
 * 注意：为减少栈使用，直接在传入的棋盘副本上操作
 */
static int IsBannedInternal(const int BoardIn[LEN + 1][LEN + 1], int x, int y, int DepthFour, int DepthThree)
{

    if (!InBoard(x, y))
        return 0;

    if (DepthFour > MAX_DEPTH_FOUR || DepthThree > MAX_DEPTH_THREE) // 递归保护
        return 0;

    int Board[LEN + 1][LEN + 1];
    memcpy(Board, BoardIn, sizeof(Board));
    Board[x][y] = BLK;

    if (ExactFiveCheck(Board, x, y, BLK)) // 五连优先
        return 0;

    if (OverCheck(Board, x, y, BLK)) // 长连禁手
        return 1;

    if (ThreeCheckInternal(Board, x, y, BLK, DepthThree)) // 三三禁手
        return 1;

    if (FourCheckInternal(Board, x, y, BLK, DepthFour)) // 四四禁手
        return 1;

    return 0;
}

static int FourCheck(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor) // 外部接口：四四禁手
{
    return FourCheckInternal(Board, x, y, SideColor, 0);
}

static int ThreeCheck(const int Board[LEN + 1][LEN + 1], int x, int y, int SideColor) // 外部接口：三三禁手
{
    return ThreeCheckInternal(Board, x, y, SideColor, 0);
}

/* 和棋检测 */
static int Peace(const int Board[LEN + 1][LEN + 1])
{
    int Filled = 0;
    for (int i = 1; i <= LEN; ++i)
        for (int j = 1; j <= LEN; ++j)
            Filled += (Board[i][j] != 0);
    return (Filled >= LEN * LEN);
}

/* 基于指定棋盘与执子方的禁手/胜负判定 */
int JudgeOnBoard(int Board[LEN + 1][LEN + 1], int x, int y, int SideColor)
{
    if (x < 1 || x > LEN || y < 1 || y > LEN)
        return JUDGE_NONE;

    int Storage = Board[x][y];
    Board[x][y] = (SideColor == 1) ? WHI : BLK;

    int Result = JUDGE_NONE;

    if (SideColor == 1) // 白方
    {
        if (FiveCheck(Board, x, y, SideColor))
            Result = JUDGE_WIN_WHITE;
        else
            Result = JUDGE_NONE;
    }
    else // 黑方
    {
        if (ExactFiveCheck(Board, x, y, SideColor))
            Result = JUDGE_WIN_BLACK;
        else if (OverCheck(Board, x, y, SideColor))
            Result = JUDGE_FORBID_OVERLINE;
        else
        {
            if (FourCheck(Board, x, y, SideColor))
                Result = JUDGE_FORBID_FOURFOUR;
            else if (ThreeCheck(Board, x, y, SideColor))
                Result = JUDGE_FORBID_THREETHREE;
            else if (Peace(Board))
                Result = JUDGE_PEACE;
            else
                Result = JUDGE_NONE;
        }
    }

    Board[x][y] = Storage;
    return Result;
}

/* 保持原有接口：使用全局 Point 与 Side */
int Judge(int x, int y)
{
    return JudgeOnBoard(Point, x, y, Side);
}

/* 禁手胜负判断和进一步禁手胜负的提示语句 */
int JudgeFurther(void)
{
    int Exit = 0;
    int Res = Judge(X, Y);
    switch (Res)
    {
    case JUDGE_NONE:
        break;
    case JUDGE_FORBID_OVERLINE:
        system("cls");
        Draw();
        printf("长连禁手，白方获胜！\n");
        printf("输入1再来一局，输入0返回主菜单: ");
        scanf("%d", &Exit);
        return Exit;
    case JUDGE_FORBID_FOURFOUR:
        system("cls");
        Draw();
        printf("四四禁手，白方获胜！\n");
        printf("输入1再来一局，输入0返回主菜单: ");
        scanf("%d", &Exit);
        return Exit;
    case JUDGE_FORBID_THREETHREE:
        system("cls");
        Draw();
        printf("三三禁手，白方获胜！\n");
        printf("输入1再来一局，输入0返回主菜单: ");
        scanf("%d", &Exit);
        return Exit;
    case JUDGE_WIN_BLACK:
        system("cls");
        Draw();
        printf("五连取胜！恭喜黑方！\n");
        printf("输入1再来一局，输入0返回主菜单: ");
        scanf("%d", &Exit);
        return Exit;
    case JUDGE_WIN_WHITE:
        system("cls");
        Draw();
        printf("五连取胜！恭喜白方！\n");
        printf("输入1再来一局，输入0退出游戏: ");
        scanf("%d", &Exit);
        return Exit;
    case JUDGE_PEACE:
        system("cls");
        Draw();
        printf("和棋！\n");
        printf("输入1再来一局，输入0返回主菜单: ");
        scanf("%d", &Exit);
        return Exit;
    }
    return 2;
}
