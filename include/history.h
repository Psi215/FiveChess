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
// 棋谱记录功能
#ifndef HISTORY_H
#define HISTORY_H

#include "rules.h"

extern int TraceX[LEN * LEN + 2];
extern int TraceY[LEN * LEN + 2]; // 加个2防止越界

void RecordMove(int x, int y);
void UpdateTrace(void);
void Regret(void);
FILE *CreateHistory(void);
void WriteHistory(int x, int y, FILE *p);

#endif // HISTORY_H