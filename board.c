// 棋盘操作相关，包含绘制棋盘与重置棋盘程序，以及落子程序。

#include "include/board.h"
#include "include/rules.h"
int Point[LEN + 1][LEN + 1] = {0}; // 0空 1白 2黑 3 4分别
int StepCount = 0;                 // 步数
int Side = 0;                      // 0 表示当前为黑，1 白
int X = 0, Y = 0;                  // 当前输入的坐标

/* 重置棋盘与状态 */
void BoardReset(void)
{
    for (int i = 1; i <= LEN; ++i)
        for (int j = 1; j <= LEN; ++j)
            Point[i][j] = 0;
    StepCount = 0; // 步数清零
    Side = 0;      // 方清零
    X = 0;
    Y = 0;
}

/* 绘制棋盘和棋子,这里返回字符串 ，返回结果再绘制*/
char *Drop(int v, int x, int y)
{
    switch (v) // 传入的v就是01234等。
    {
    case 0: // 不同位置有不同的棋盘格子
        if (x == 1 && y == LEN)
            return "┌";
        if (x == LEN && y == LEN)
            return "┐";
        if (x == 1 && y == 1)
            return "└";
        if (x == LEN && y == 1)
            return "┘";
        if (y == LEN)
            return "┬";
        if (y == 1)
            return "┴";
        if (x == 1)
            return "├";
        if (x == LEN)
            return "┤";
        return "┼";
    case 1:
        return "○"; // 白子
    case 2:
        return "●"; // 黑子
    case 3:
        return "◇"; // 白子落子标记
    case 4:
        return "▼"; // 黑子落子标记
    default:
        return "?";
    }
}

/* 绘制棋盘 */
int Draw(void)
{
    for (int y = LEN; y > 0; --y) // 上高下低
    {
        printf("%2d", y); // 固定为两位数对齐
        for (int x = 1; x <= LEN; ++x)
        {
            printf("%s", Drop(Point[x][y], x, y));
            if (x != LEN)
                printf("-"); // 减号让棋盘变成正方形而不是水平短竖直长的长方形。否则实在不好看
        }
        printf("\n");
    }
    printf("  "); // 对齐
    for (int x = 1; x <= LEN; ++x)
    {
        printf("%c ", 'A' + x - 1);
    }
    printf("\n");
    return 0;
}

/* 落子，含边界检查 */
int PutChess(int x, int y, int color) // color: 1=白子 2=黑子 3=白子落子标记 4=黑子落子标记
{
    if (x < 1 || x > LEN || y < 1 || y > LEN)
        return -1; /* 越界返回错误 */
    Point[x][y] = color;
    return 0;
}