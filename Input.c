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
#include "include/rules.h"
#include <stdio.h>

/* 清空当前行剩余输入，避免 scanf 失败后死循环 */
static void FlushLine(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
    {
    }
}

/* 模式选择 */
int ModeSelect(void)
{
    int c, err = 1;
    while (err)
    {
        int ret = scanf("%d", &c);
        if (ret == 1 && (c == 1 || c == 2 || c == 3 || c == 4)) // 输入有效
            err = 0;
        else
        {
            puts("请输入对应数字！"); // 越界或非数字时提示
            FlushLine();              // 丢弃无效输入，避免陷入死循环
        }
    }
    getchar(); // 清理缓冲区
    return c;
}

/* 玩家落子函数 */
int PlayerInput(void)
{
    int x, y;
    while (1)
    {
        printf("请输入落子位置(格式: A1(请严格大小写))，或输入 @0 悔棋: ");
        while (1)
        {
            char ch = getchar();
            if (ch != '\n')
            {
                x = ch;
                break;
            }
        }
        if (scanf("%d", &y) != 1)
        {
            puts("输入格式错误，请重新输入，如 A1 或 @0。");
            FlushLine();
            continue;
        }
        FlushLine();
        if (x >= 'A' && x <= 'Z')
            x = x - 'A' + 1; // 转换为1-15j//这一部分之前均为输入处理
        else if (x >= 'a' && x <= 'z')
            x = x - 'a' + 1; // 转换为1-15

        if (x == '@' && y == 0)
        {
            return -1; // 悔棋trigger
        }
        if (x < 1 || x > LEN || y < 1 || y > LEN) // 落子越界
        {
            puts("坐标越界，请按规范输入坐标。");
            continue;
        }
        if (Point[x][y] != 0) // 重复落子
        {
            puts("该位置已经有棋子，请重新落子。");
            continue;
        }

        X = x;
        Y = y;

        return 1; // 正常落子trigger
    }
}

/* AI输入 */
int AiInput(int XValue, int YValue)
{
    X = XValue;
    Y = YValue;
    return 1;
}