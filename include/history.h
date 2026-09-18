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