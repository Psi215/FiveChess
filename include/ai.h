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
#ifndef AI_H
#define AI_H

#include "common.h"

/* 获取当前执子方的最佳落子，side: 0=黑,1=白；返回1成功，0失败 */
int AiGetBestMove(int Side, int *OutX, int *OutY);

/* 可选：调整搜索深度 */
void AiSetSearchDepth(int Depth);

/* 可选：设置根节点并行线程数（0=按CPU核数） */
void AiSetThreadCount(int Threads);

/* 获取最近一次 AI 思考用时（毫秒） */
double AiGetLastTimeMs(void);

/* 预填充评分表，避免运行时重复计算 */
void InitScoreTables(void);

/* 初始化 Zobrist 哈希表和置换表 */
void InitZobrist(void);

/* 清空置换表，在每局开始时调用 */
void ClearTransTable(void);


#endif // AI_H
