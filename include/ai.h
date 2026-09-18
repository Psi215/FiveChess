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
