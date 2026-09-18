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
// 此头文件储存禁手、胜负相关
#ifndef RULES_H
#define RULES_H
#include "board.h"
#include <stdlib.h>

// 禁手、胜负码
#define JUDGE_NONE 0              // 无胜负/禁手
#define JUDGE_FORBID_OVERLINE 1   // 长连禁手
#define JUDGE_FORBID_FOURFOUR 2   // 四四禁手
#define JUDGE_FORBID_THREETHREE 3 // 三三禁手
#define JUDGE_WIN_BLACK 4         // 黑方五连取胜
#define JUDGE_WIN_WHITE 5         // 白方五连取胜
#define JUDGE_PEACE 6             // 和棋

int ModeSelect(void); // 模式选择
int PlayerInput(void);
int AiInput(int XValue, int YValue);
int Judge(int x, int y);                                                    // 返回上述之一，根据 Side 判断（全局 Point/Side）
int JudgeOnBoard(int Board[LEN + 1][LEN + 1], int x, int y, int SideColor); // 基于指定棋盘与执子方判定
int JudgeFurther(void);

#endif // RULES_H
