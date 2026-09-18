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
//储存棋盘相关
#ifndef BOARD_H
#define BOARD_H
#include "common.h"

extern int Point[LEN+1][LEN+1];
extern int StepCount;
extern int Side; // 0 黑 1 白
extern int X, Y;//落子坐标
int Draw(void);

void BoardReset(void);
int PutChess(int x, int y, int color);

#endif // BOARD_H
