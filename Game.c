/*
 * FiveChess - C 语言五子棋对弈引擎
 * Copyright (C) 2025 朱骏锦
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "include/game.h"
#include "include/ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int Side;      // 0 黑 1 白
extern int StepCount; // 由 board.c 定义的步数
extern int X, Y;      // 由 board.c 定义

int GamePvP(void)
{
    int Exit = 0;     // 游戏结束标志
    int color;        // 落子颜色
    int Maintain = 1; // 游戏进行标志
    int err;          // 错误指示标志，为1时进入落子。
    BoardReset();     // 棋盘重置
    system("cls");

    FILE *FileP = NULL; // 文件指针
    puts("是否需要记录棋谱。\n是，请输入1\n否，请输入非1");
    int IfRecord=0, YesRecord = 0;
    scanf("%d", &IfRecord);
    getchar(); // 清空缓冲区

    if (IfRecord == 1)
    {
        YesRecord = 1;
        FileP = CreateHistory(); // 创建文件并返回指针;
    }

    while (Maintain)
    {
        system("cls");
        Draw();

        Side = StepCount % 2;           // 根据步数决定当前执子方
        color = Side ? PREWHI : PREBLK; // 落子标记颜色

        if (StepCount > 0)
        {
            UpdateTrace();
            printf("上一步落子位置为%c%d\n", X - 1 + 'A', Y);
        }

        err = 1; // 落子标记,用于跳出while

        while (err)
        {
            if (Side == 0)
                puts("当前轮到黑方落子");
            else
                puts("当前轮到白方落子");

            switch (PlayerInput())
            {
            case -1:
                Regret();                       // 悔棋;
                color = Side ? PREWHI : PREBLK; // 悔棋后需要再次更新pre字颜色
                break;                          // 落子必定失败，不更新err;
            case 1:
            {
                PutChess(X, Y, color); // 落子
                err = 0;               // 判断落子成功与否
            }
            }
        } // 跳出循环代表落子完成.

        StepCount++;

        RecordMove(X, Y); // 记录棋谱

        if (YesRecord)                 // 如果启用记录棋谱的话
            WriteHistory(X, Y, FileP); // 向文件写入新的一步

        if ((Exit = JudgeFurther()) != 2) // 禁手胜负等的判断
        {
            if (YesRecord && FileP)
                fclose(FileP);
            // 关闭文件
            return Exit;
        }
    }
    return 0;
}
int GamePvE(void)
{
    int Exit = 0;
    int color;
    int Maintain = 1;
    int err;
    int HumanSide = 0;        // 0=玩家执黑先手,1=玩家执白后手
    double aiTime;            // ai思考用时。
    double TotalAiTime = 0.0; // ai总思考用时
    int TimeCount = 0;        // ai思考次数
    InitScoreTables();        // 预生成评分表
    InitZobrist();            // 初始化 Zobrist 哈希表和置换表
    BoardReset();
    system("cls");

    puts("请选择执子方：\n1. 玩家执黑(人先)\n2. 玩家执白(机先)");
    int SideChoice; // 执子方
    while (1)
    {
        scanf("%d", &SideChoice);
        getchar();
        if (SideChoice == 1 || SideChoice == 2)
            break;
        puts("输入有误，请重新输入:");
    }
    if (SideChoice == 2)
        HumanSide = 1;

    puts("请输入AI搜索深度(1-10):");
    int DepthInput; // ai搜索深度
    while(1)
    {
        scanf("%d", &DepthInput);
        getchar();
        if (DepthInput > 0 && DepthInput <= 10)
            break;
        puts("输入有误，请重新输入:");
    }

    AiSetSearchDepth(DepthInput); // 设置ai搜索深度

    while (Maintain) // 游戏程序（循环）
    {
        system("cls");
        Draw();

        Side = StepCount % 2;
        color = Side ? PREWHI : PREBLK;

        if (StepCount > 0)
        {
            UpdateTrace(); // 提前更新trace
            printf("上一步落子位置为%c%d\n", X - 1 + 'A', Y);
        }

        if (Side == HumanSide)
        {
            aiTime = AiGetLastTimeMs(); // AI思考用时
            TotalAiTime += aiTime;      // 累计总用时
            if (aiTime >= 0 && StepCount > 0)
                printf("上一回合AI思考用时 %.2f s, 平均 %.2f s\n", aiTime / 1000, TotalAiTime / (++TimeCount * 1000));
            err = 1;
            while (err)
            {
                if (HumanSide == 0)//黑子
                    puts("当前轮到玩家(黑)落子");
                else
                    puts("当前轮到玩家(白)落子");
                switch (PlayerInput())
                {
                case -1:
                    Regret();
                    if (StepCount > 0)
                    {
                        Regret();                       // 悔棋两次，回到AI上一步落子前
                    }
                    color = Side ? PREWHI : PREBLK;
                    break;
                case 1:
                    PutChess(X, Y, color);
                    err = 0;
                    break;
                }
            }
        }
        else
        {
            int ax = -1, ay = -1;          // 初始化坐标落子
            AiGetBestMove(Side, &ax, &ay); // ai运算得到最佳落子位置
            PutChess(X, Y, color);         // 落子
        }

        StepCount++;

        RecordMove(X, Y);

        if ((Exit = JudgeFurther()) != 2)
        {
            return Exit;
        }
    }
    return 0;
}
int GameEvE(void)
{
    int Exit = 0;
    int Maintain = 1;
    int color;
    int DepthInput;
    int IfRecord = 0, YesRecord = 0;
    FILE *FileP = NULL;
    InitScoreTables();
    InitZobrist(); // 初始化 Zobrist 哈希表和置换表
    BoardReset();
    system("cls");

    puts("请选择是否记录棋谱。\n是，请输入1\n否，请输入非1");
    while(1)
    {
        scanf("%d", &IfRecord);
        getchar();
        if (IfRecord == 1 || IfRecord != 1)
            break;
        puts("输入有误，请重新输入:");
    }
    if (IfRecord == 1)
    {
        YesRecord = 1;
        FileP = CreateHistory();
    }

    puts("请输入AI搜索深度(1-10):");
    while(1)
    {
        scanf("%d", &DepthInput);
        getchar();
        if (DepthInput > 0 && DepthInput <= 10)
            break;
        puts("输入有误，请重新输入:");
    }

    AiSetSearchDepth(DepthInput);

    while (Maintain)
    {
        system("cls");
        Draw();

        double aiTime = AiGetLastTimeMs();
        if (aiTime >= 0&&StepCount>0)
            printf("上一回合AI思考用时 %.1f ms\n", aiTime);

        Side = StepCount % 2;
        color = Side ? PREWHI : PREBLK;

        if (StepCount > 0)
        {
            UpdateTrace(); // 提前更新trace
            printf("上一步落子位置为%c%d\n", X - 1 + 'A', Y);
        }

        int ax = -1, ay = -1;
        AiGetBestMove(Side, &ax, &ay);
        PutChess(ax, ay, color);

        StepCount++;
        RecordMove(X, Y);
        if (YesRecord)
            WriteHistory(X, Y, FileP);

        if ((Exit = JudgeFurther()) != 2)
        {
            if (YesRecord && FileP)
                fclose(FileP);
            return Exit;
        }
    }
    return 0;
}
