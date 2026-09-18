// 棋谱记录相关，包含悔棋程序。
#include "include/history.h"
#include "include/rules.h"
#include "include/board.h"
#include <stdlib.h>

int TraceX[LEN * LEN + 2];
int TraceY[LEN * LEN + 2];
extern int StepCount; // 由 board.c 定义的步数

/* 记录当前落子位置 */
void RecordMove(int x, int y)
{
    if (StepCount < 0 || StepCount > LEN * LEN)
        return; /* 防止越界 */
    if (x < 1 || x > LEN || y < 1 || y > LEN)
        return; /* 坐标越界 */
    TraceX[StepCount] = x;
    TraceY[StepCount] = y;
}

/* 用来更新最后一步的位置 */
void UpdateTrace(void)
{
    if (StepCount < 1 || StepCount > LEN * LEN)
        return; /* 防止越界 */
    int tx = TraceX[StepCount];
    int ty = TraceY[StepCount];
    if (tx < 1 || tx > LEN || ty < 1 || ty > LEN)
        return; /* 坐标越界 */
    Point[tx][ty] = (Side == 1 ? BLK : WHI);
}

/*
 * 悔棋函数，先防止在第0步悔棋，再抹除上一步坐标，减一步步数，更新双方棋子，值得注意的是，color非全局变量，因此只能放回game中更新（懒得改了（全局变量已经够多了，怕出错））
 */
void Regret(void)
{
    if (StepCount <= 0) // 第0步不悔棋
    {
        puts("不能悔棋");
        return;
    }
    if (StepCount > LEN * LEN) // 防止越界
    {
        puts("步数异常");
        return;
    }
    int XLast = TraceX[StepCount]; // 获取最后一步落子坐标X
    int YLast = TraceY[StepCount]; // 获取最后一步落子坐标y

    /* 边界检查 */
    if (XLast >= 1 && XLast <= LEN && YLast >= 1 && YLast <= LEN)
        Point[XLast][YLast] = 0; // 清除上一步

    StepCount--; // 更新步数

    Side = StepCount % 2; // 更新当前执子方

    system("cls");
    Draw();
    printf("悔棋成功，已撤销最后一步 (%c%d)\n", XLast + 'A' - 1, YLast);
    return;
}

/* 创建棋谱，棋谱文件名不能重复，返回指向文件名的指针*/
FILE *CreateHistory(void)
{
    char FileName[20];
    puts("（请不要在需要以文件形式存棋谱的棋局中悔棋）请输入你想命名的棋谱名称（需要后缀）:");
    scanf("%s", FileName);
    getchar();
    return (fopen(FileName, "a")); // fopen返回的是指向文件名的指针
}

/* 载入棋谱 */
void LoadHistory(void)
{
    char Name[50];
    puts("请输入棋谱名称（不含后缀）");
    scanf("%s", Name);
}

/*
 *棋谱输出函数，为了保证统一性，方便读取，每一步均长4个字符，末尾有回车。
 */
void WriteHistory(int x, int y, FILE *p)
{
    char LineInput[5];
    LineInput[0] = x - 1 + 'A';
    if (y >= 10)
    {
        switch (y)
        {
        case 10:
            LineInput[1] = '1';
            LineInput[2] = '0';
            break;
        case 11:
            LineInput[1] = '1';
            LineInput[2] = '1';
            break;
        case 12:
            LineInput[1] = '1';
            LineInput[2] = '2';
            break;
        case 13:
            LineInput[1] = '1';
            LineInput[2] = '3';
            break;
        case 14:
            LineInput[1] = '1';
            LineInput[2] = '4';
            break;
        case 15:
            LineInput[1] = '1';
            LineInput[2] = '5';
            break;
        }
        LineInput[3] = '\n';
        LineInput[4] = '\0';
    }
    else
    {
        LineInput[1] = y - 1 + '1';
        LineInput[2] = ' ';
        LineInput[3] = '\n';
        LineInput[4] = '\0';
    }
    fputs(LineInput, p);
    fflush(p); // 每下一步刷新一次文件，确保棋谱功能性
}